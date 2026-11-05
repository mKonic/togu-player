#include "statussync.h"

#include "core/jp/tokenizer.h"
#include "core/util/db.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>
#include <QTimer>

namespace statussync {

Status statusFor(const CardState &c, const Rules &r, Status previous)
{
    if (c.suspended) return previous; // keeps whatever it had
    if (c.type == u"new") return Status::Tracked;
    if (c.type == u"learning" || c.type == u"relearning") return Status::Learning;
    const double strength = c.stability >= 0 ? c.stability : c.intervalDays;
    // Leeches stay in learning until they really stick.
    if (c.lapses >= r.leechLapses && strength < r.matureDays) return Status::Learning;
    if (strength >= r.matureDays) return Status::Known;
    // A known word drops back only on a lapse (relearning, above), not a dip in strength.
    if (previous == Status::Known) return Status::Known;
    return Status::Learning;
}

Status combine(const QList<Status> &statuses)
{
    // Weakest first: tracked < learning < known.
    auto rank = [](Status s) {
        switch (s) {
        case Status::Tracked: return 0;
        case Status::Learning: return 1;
        case Status::Known: return 2;
        default: return 3;
        }
    };
    Status out = Status::Unknown;
    for (Status s : statuses)
        if (s != Status::Unknown && (out == Status::Unknown || rank(s) < rank(out))) out = s;
    return out;
}

namespace {

// The newer AnkiConnect also speaks JSON-RPC at /api/v1, which reports FSRS stability.
QHash<qlonglong, double> fsrsStability(const anki::Conn &c, const QList<qlonglong> &ids)
{
    QHash<qlonglong, double> out;
    QNetworkAccessManager net;
    QString base = c.url;
    while (base.endsWith(u'/')) base.chop(1);
    for (qsizetype i = 0; i < ids.size(); i += 500) {
        QJsonArray chunk;
        for (qsizetype j = i; j < std::min(ids.size(), i + 500); ++j) chunk.append(QString::number(ids[j]));
        QNetworkRequest req{QUrl(base + QStringLiteral("/api/v1"))};
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        QJsonObject body{{"jsonrpc", "2.0"}, {"id", 1}, {"method", "cards.getMany"}, {"params", QJsonObject{{"ids", chunk}}}};
        QNetworkReply *r = net.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
        QEventLoop loop;
        QTimer t;
        t.setSingleShot(true);
        QObject::connect(&t, &QTimer::timeout, r, &QNetworkReply::abort);
        QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        t.start(20000);
        loop.exec();
        const QJsonObject res = QJsonDocument::fromJson(r->readAll()).object();
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError || !res.contains("result")) return {}; // plain AnkiConnect: no FSRS data
        for (const QJsonValue &it : res.value("result").toObject().value("items").toArray()) {
            const QJsonObject card = it.toObject().value("result").toObject();
            const QJsonValue mem = card.value("scheduling").toObject().value("fsrsMemory");
            if (mem.isObject()) out.insert(card.value("id").toString().toLongLong(), mem.toObject().value("stabilityDays").toDouble());
        }
    }
    return out;
}

QString deckQuery(const QStringList &decks)
{
    QStringList parts;
    for (QString d : decks) parts << QStringLiteral("\"deck:%1\"").arg(d.replace(u'"', QStringLiteral("\\\"")));
    return parts.join(QStringLiteral(" OR "));
}

} // namespace

Report sync(const anki::Conn &c, const Config &cfg)
{
    Report rep;
    auto d = db::app();
    QHash<QString, Status> wordStatus;     // word -> combined status (learning decks)
    QHash<QString, qint64> wordMod;        // word -> latest card mod
    QSet<QString> seenWords;

    if (!cfg.decks.isEmpty()) {
        const anki::Result found = anki::invoke(c, QStringLiteral("findCards"), {{"query", deckQuery(cfg.decks)}});
        if (!found.ok()) {
            rep.error = found.error;
            return rep;
        }
        QList<qlonglong> ids;
        for (const QJsonValue &v : found.value.toArray()) ids << v.toVariant().toLongLong();
        rep.cards = int(ids.size());

        // Which cards changed since last time.
        QHash<qlonglong, qint64> cached;
        QSqlQuery q(d);
        q.exec(QStringLiteral("SELECT card, mod FROM anki_cards"));
        while (q.next()) cached.insert(q.value(0).toLongLong(), q.value(1).toLongLong());
        QList<qlonglong> changed;
        for (qsizetype i = 0; i < ids.size(); i += 1000) {
            QJsonArray chunk;
            for (qsizetype j = i; j < std::min(ids.size(), i + 1000); ++j) chunk.append(ids[j]);
            const anki::Result mods = anki::invoke(c, QStringLiteral("cardsModTime"), {{"cards", chunk}});
            for (const QJsonValue &m : mods.value.toArray()) {
                const qlonglong id = m.toObject().value("cardId").toVariant().toLongLong();
                const qint64 mod = m.toObject().value("mod").toVariant().toLongLong();
                if (!cached.contains(id) || cached.value(id) != mod) changed << id;
            }
        }
        rep.changed = int(changed.size());
        const QHash<qlonglong, double> stability = fsrsStability(c, changed);

        db::Tx tx(d);
        // Cards no longer in the learning decks.
        const QSet<qlonglong> idSet(ids.begin(), ids.end());
        for (auto it = cached.constBegin(); it != cached.constEnd(); ++it)
            if (!idSet.contains(it.key())) db::exec(d, QStringLiteral("DELETE FROM anki_cards WHERE card=?"), {it.key()});
        for (qsizetype i = 0; i < changed.size(); i += 300) {
            QJsonArray chunk;
            for (qsizetype j = i; j < std::min(changed.size(), i + 300); ++j) chunk.append(changed[j]);
            const anki::Result info = anki::invoke(c, QStringLiteral("cardsInfo"), {{"cards", chunk}});
            for (const QJsonValue &v : info.value.toArray()) {
                const QJsonObject card = v.toObject();
                const qlonglong id = card.value("cardId").toVariant().toLongLong();
                const QString model = card.value("modelName").toString();
                const ModelConfig mc = cfg.models.value(model);
                const QJsonObject fields = card.value("fields").toObject();
                const QString wordField = mc.wordField.isEmpty() ? fields.keys().value(0) : mc.wordField;
                const QString word = anki::plainField(fields.value(wordField).toObject().value("value").toString());
                const QString reading = mc.readingField.isEmpty() ? QString() : anki::plainField(fields.value(mc.readingField).toObject().value("value").toString());
                if (word.isEmpty()) continue;
                const int templ = card.value("ord").toInt();
                const bool counted = mc.templates.isEmpty() || mc.templates.contains(templ);
                const int typeNum = card.value("type").toInt();
                CardState st;
                st.type = QStringList{"new", "learning", "review", "relearning"}.value(typeNum, QStringLiteral("review"));
                st.suspended = card.value("queue").toInt() < 0;
                st.intervalDays = card.value("interval").toDouble();
                st.stability = stability.value(id, -1);
                st.lapses = card.value("lapses").toInt();
                const QString key = jp::wordKey(word, reading == word ? QString() : reading);
                const QString prevName = db::scalar(d, QStringLiteral("SELECT status FROM anki_cards WHERE card=?"), {id}).toString();
                const Status prev = prevName.isEmpty() ? Status::Unknown : Status(prevName.toInt());
                const Status s = counted ? statusFor(st, cfg.rules, prev) : Status::Unknown;
                db::exec(d, QStringLiteral("INSERT OR REPLACE INTO anki_cards (card, note, word, mod, status, strength, lapses, type, queue) VALUES (?,?,?,?,?,?,?,?,?)"),
                         {id, card.value("note").toVariant(), key, card.value("mod").toVariant(), int(s),
                          st.stability >= 0 ? st.stability : st.intervalDays, st.lapses, st.type, card.value("queue").toInt()});
            }
        }
        tx.commit();

        // Every word's status from all its cards (unchanged ones come from the cache).
        QHash<QString, QList<Status>> perWord;
        QSqlQuery all(d);
        all.exec(QStringLiteral("SELECT word, status, mod FROM anki_cards"));
        while (all.next()) {
            const QString w = all.value(0).toString();
            perWord[w] << Status(all.value(1).toInt());
            wordMod[w] = std::max(wordMod.value(w), all.value(2).toLongLong());
            seenWords.insert(w);
        }
        for (auto it = perWord.constBegin(); it != perWord.constEnd(); ++it) wordStatus.insert(it.key(), combine(it.value()));
    }

    // Known decks: everything in them is known, whatever its schedule.
    if (!cfg.knownDecks.isEmpty()) {
        const anki::Result notes = anki::invoke(c, QStringLiteral("findNotes"), {{"query", deckQuery(cfg.knownDecks)}});
        const QJsonArray ids = notes.value.toArray();
        for (qsizetype i = 0; i < ids.size(); i += 500) {
            QJsonArray chunk;
            for (qsizetype j = i; j < std::min(ids.size(), i + 500); ++j) chunk.append(ids[j]);
            for (const QJsonValue &n : anki::invoke(c, QStringLiteral("notesInfo"), {{"notes", chunk}}).value.toArray()) {
                const QJsonObject note = n.toObject();
                const ModelConfig mc = cfg.models.value(note.value("modelName").toString());
                const QJsonObject fields = note.value("fields").toObject();
                QString wordField = mc.wordField;
                if (wordField.isEmpty())
                    for (auto it = fields.begin(); it != fields.end(); ++it)
                        if (it.value().toObject().value("order").toInt() == 0) wordField = it.key();
                const QString word = anki::plainField(fields.value(wordField).toObject().value("value").toString());
                const QString reading = mc.readingField.isEmpty() ? QString() : anki::plainField(fields.value(mc.readingField).toObject().value("value").toString());
                if (word.isEmpty()) continue;
                const QString key = jp::wordKey(word, reading == word ? QString() : reading);
                if (wordStatus.value(key) != Status::Known) wordStatus.insert(key, Status::Known);
                wordMod.insert(key, std::max<qint64>(wordMod.value(key), 1));
                seenWords.insert(key);
            }
        }
    }

    QList<Words::AnkiState> states;
    for (auto it = wordStatus.constBegin(); it != wordStatus.constEnd(); ++it)
        states.append({it.key(), it.value(), wordMod.value(it.key())});
    // Words Anki set before whose cards are gone go back to unknown (hand settings stay).
    QSqlQuery prev(d);
    prev.exec(QStringLiteral("SELECT w FROM words WHERE manual=0"));
    while (prev.next())
        if (!seenWords.contains(prev.value(0).toString())) states.append({prev.value(0).toString(), Status::Unknown, 0});
    Words::instance().applyAnki(states);
    rep.words = int(wordStatus.size());
    return rep;
}

} // namespace statussync
