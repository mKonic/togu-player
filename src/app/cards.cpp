#include "cards.h"

#include "core/anki/anki.h"
#include "core/dict/glossary.h"
#include "core/jp/jp.h"
#include "core/jp/tokenizer.h"
#include "core/media/cut.h"
#include "core/media/wordaudio.h"
#include "core/subs/subs.h"
#include "core/util/db.h"
#include "core/words/words.h"
#include "settings.h"

#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QQmlEngine>
#include <QQuickItemGrabResult>
#include <QtConcurrent>

namespace {

anki::Conn conn()
{
    return {Settings::v("ankiUrl").toString(), Settings::v("ankiKey").toString()};
}

QVariant plain(const QVariant &v) { return v.canConvert<QJSValue>() ? v.value<QJSValue>().toVariant() : v; }

QString pitchName(QChar c)
{
    switch (c.unicode()) {
    case 'h': return QStringLiteral("heiban");
    case 'a': return QStringLiteral("atamadaka");
    case 'n': return QStringLiteral("nakadaka");
    case 'o': return QStringLiteral("odaka");
    case 'k': return QStringLiteral("kifuku");
    }
    return {};
}

} // namespace

Cards *Cards::instance()
{
    static Cards *c = new Cards;
    return c;
}

Cards *Cards::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

Cards::Cards(QObject *parent) : QObject(parent)
{
    QTimer::singleShot(2000, this, [this] { refresh(); });
}

void Cards::busyInc(int d)
{
    m_busy += d;
    emit busyChanged();
}

QVariantList Cards::markers() const { return anki::markers(); }

void Cards::refresh()
{
    busyInc(1);
    const QString model = Settings::v("ankiModel").toString();
    const bool needFields = !model.isEmpty() && Settings::v("ankiFields").toMap().isEmpty();
    (void)QtConcurrent::run([this, c = conn(), model, needFields] {
        const anki::Result v = anki::invoke(c, QStringLiteral("version"));
        QStringList decks, models, fields;
        if (v.ok()) {
            for (const QJsonValue &d : anki::invoke(c, QStringLiteral("deckNames")).value.toArray()) decks << d.toString();
            for (const QJsonValue &m : anki::invoke(c, QStringLiteral("modelNames")).value.toArray()) models << m.toString();
            if (needFields)
                for (const QJsonValue &f : anki::invoke(c, QStringLiteral("modelFieldNames"), {{"modelName", model}}).value.toArray()) fields << f.toString();
        }
        QMetaObject::invokeMethod(this, [this, v, decks, models, fields, model] {
            // A note type chosen but never mapped: start from the guesses.
            if (!fields.isEmpty() && Settings::v("ankiFields").toMap().isEmpty()) Settings::instance()->set(QStringLiteral("ankiFields"), guessFields(fields, model));
            busyInc(-1);
            m_connected = v.ok();
            m_error = v.error;
            m_decks = decks;
            m_models = models;
            emit ankiChanged();
            if (m_connected) syncCardMarks();
        });
    });
}

void Cards::modelFields(const QString &model)
{
    (void)QtConcurrent::run([this, model, c = conn()] {
        QStringList fields;
        for (const QJsonValue &f : anki::invoke(c, QStringLiteral("modelFieldNames"), {{"modelName", model}}).value.toArray()) fields << f.toString();
        QMetaObject::invokeMethod(this, [this, model, fields] { emit fieldsReady(model, fields); });
    });
}

QVariantMap Cards::guessFields(const QStringList &fields, const QString &model) const
{
    if (model.contains(u"Lapis", Qt::CaseInsensitive)) {
        QVariantMap preset = anki::lapisPreset(), out;
        for (const QString &f : fields) out.insert(f, preset.value(f, anki::guessField(f)));
        return out;
    }
    QVariantMap out;
    for (const QString &f : fields) out.insert(f, anki::guessField(f));
    return out;
}

QVariantMap Cards::draft(const QVariant &entryV, const QVariantMap &line, const QString &surface, int itemId, double offset)
{
    Q_UNUSED(offset)
    const QVariantMap entry = plain(entryV).toMap();
    QVariantMap d;
    const QString sentence = subs::tidyLine(line.value("text").toString());
    d.insert("expression", entry.value("expression"));
    d.insert("reading", entry.value("reading"));
    d.insert("surface", surface.isEmpty() ? entry.value("expression").toString() : surface);
    d.insert("key", entry.value("key"));
    d.insert("sentence", sentence);
    d.insert("translation", line.value("text2"));
    d.insert("start", line.value("start"));
    d.insert("end", line.value("end"));
    // Furigana for the whole line, in Anki's bracket notation.
    QString furi;
    for (const jp::Token &t : jp::tokenize(sentence)) {
        const auto segs = jp::furiganaSegments(t.surface, t.reading);
        const QString part = jp::furiganaPlain(segs);
        if (!furi.isEmpty() && part.contains(u'[') && !furi.endsWith(u' ')) furi += u' ';
        furi += part;
    }
    d.insert("sentenceFurigana", furi);
    // Definitions: each dictionary's block, labelled as Yomitan does.
    QVariantList blocks;
    for (const QVariant &b : entry.value("blocks").toList()) {
        const QVariantMap bm = b.toMap();
        blocks << QVariantMap{{"dict", bm.value("dict")}, {"html", bm.value("cardHtml")}, {"preview", bm.value("html")}, {"on", true}};
    }
    d.insert("blocks", blocks);
    // Frequency and pitch.
    QStringList freqHtml;
    int freqSort = 0;
    for (const QVariant &f : entry.value("freqs").toList()) {
        const QVariantMap fm = f.toMap();
        freqHtml << QStringLiteral("<li>%1: %2</li>").arg(fm.value("dict").toString().toHtmlEscaped(), fm.value("display").toString().toHtmlEscaped());
        const int v = int(fm.value("value").toDouble());
        if (v > 0 && (!freqSort || v < freqSort)) freqSort = v;
    }
    d.insert("frequency", freqHtml.isEmpty() ? QString() : u"<ul>" + freqHtml.join(QString()) + u"</ul>");
    d.insert("freqSort", freqSort);
    QStringList positions, cats;
    for (const QVariant &p : entry.value("pitches").toList())
        for (const QVariant &x : p.toMap().value("positions").toList()) {
            const QString s = QString::number(x.toInt());
            if (positions.contains(s)) continue;
            positions << s;
            cats << pitchName(jp::pitchClass(x.toInt(), entry.value("reading").toString(), {}));
        }
    d.insert("pitch", positions.join(u','));
    d.insert("pitchCategories", cats.join(u','));
    // Source: show, episode and time.
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT COALESCE(s.meta_title, s.title), i.episode, i.title, i.path, i.kind FROM items i LEFT JOIN series s ON s.id=i.series WHERE i.id=?"));
    q.addBindValue(itemId);
    QString source;
    if (db::exec(q) && q.next()) {
        source = q.value(0).toString().isEmpty() ? q.value(2).toString() : q.value(0).toString();
        if (!q.value(1).isNull()) source += QStringLiteral(" · %1").arg(q.value(1).toDouble());
        d.insert("path", q.value(3));
        d.insert("kind", q.value(4));
    }
    const qint64 t = qint64(line.value("start").toDouble());
    source += QStringLiteral(" · %1:%2").arg(t / 60).arg(t % 60, 2, 10, QChar(u'0'));
    d.insert("source", source);
    d.insert("itemId", itemId);
    d.insert("images", QVariantList());
    d.insert("sentenceAudio", QString());
    const auto clips = wordaudio::find(wordaudio::collectionDir(Settings::v("wordAudioDir").toString()),
                                       entry.value("expression").toString(), entry.value("reading").toString());
    d.insert("wordAudio", clips.isEmpty() ? QString() : clips.first().path);
    d.insert("notes", QString());
    return d;
}

int Cards::cutAudio(const QString &path, double start, double end, int audioTrack)
{
    const int token = ++m_token;
    busyInc(1);
    const double padB = Settings::v("audioPadBefore").toDouble(), padA = Settings::v("audioPadAfter").toDouble();
    const QString fmt = Settings::v("audioFormat").toString();
    (void)QtConcurrent::run([this, token, path, start, end, audioTrack, padB, padA, fmt] {
        const QString audio = media::cutAudio({path, start - padB, end + padA, audioTrack, fmt});
        QMetaObject::invokeMethod(this, [this, token, audio] {
            busyInc(-1);
            emit mediaReady(token, {{"sentenceAudio", audio}});
        });
    });
    return token;
}

int Cards::cutImage(const QString &path, double start, double end, const QString &imageMode, double frameAt)
{
    const int token = ++m_token;
    busyInc(1);
    const int h = Settings::v("imageHeight").toInt();
    const double maxS = Settings::v("clipMaxSeconds").toDouble();
    const QString fmt = Settings::v("imageFormat").toString();
    const int track = -1;
    (void)QtConcurrent::run([this, token, path, start, end, imageMode, frameAt, h, maxS, fmt, track] {
        QString img;
        if (imageMode == u"animated") img = media::animatedImage(path, start, end, h, maxS);
        else if (imageMode == u"clip") img = media::videoClip(path, start, end, h, track, maxS);
        else img = media::stillFrame(path, frameAt >= 0 ? frameAt : (start + end) / 2, h, fmt);
        QMetaObject::invokeMethod(this, [this, token, img] {
            busyInc(-1);
            emit mediaReady(token, {{"image", img}});
        });
    });
    return token;
}

int Cards::cutMedia(const QString &path, double start, double end, int audioTrack, const QString &imageMode, double frameAt)
{
    cutAudio(path, start, end, audioTrack);
    return cutImage(path, start, end, imageMode, frameAt);
}

QVariantList Cards::wordAudio(const QString &expression, const QString &reading) const
{
    QVariantList out;
    for (const auto &c : wordaudio::find(wordaudio::collectionDir(Settings::v("wordAudioDir").toString()), expression, reading))
        out << QVariantMap{{"path", c.path}, {"source", c.source}, {"speaker", c.speaker}};
    return out;
}

QString Cards::saveGrab(const QVariant &image) const
{
    QImage img = plain(image).value<QImage>();
    if (img.isNull()) return {};
    const int h = Settings::v("imageHeight").toInt();
    if (img.height() > h) img = img.scaledToHeight(h, Qt::SmoothTransformation);
    const QString out = media::mediaCacheDir() + QStringLiteral("/grab-%1.webp").arg(QDateTime::currentMSecsSinceEpoch());
    return img.save(out, "WEBP", 82) ? out : QString();
}

namespace {

anki::Draft toDraft(const QVariantMap &d)
{
    anki::Draft a;
    a.expression = d.value("expression").toString();
    a.reading = d.value("reading").toString();
    a.surface = d.value("surface").toString();
    QStringList all, text;
    QString first;
    for (const QVariant &b : d.value("blocks").toList()) {
        const QVariantMap bm = b.toMap();
        if (!bm.value("on").toBool()) continue;
        const QString html = bm.value("html").toString();
        if (first.isEmpty()) first = html;
        all << QStringLiteral("<div data-dict=\"%1\"><i>(%1)</i> %2</div>").arg(bm.value("dict").toString().toHtmlEscaped(), html);
        text << glossary::htmlToText(html);
    }
    a.glossaryHtml = all.join(QString());
    a.glossaryFirstHtml = first;
    a.glossaryText = text.join(u'\n');
    a.sentence = d.value("sentence").toString();
    a.sentenceTranslation = d.value("translation").toString();
    a.sentenceFurigana = d.value("sentenceFurigana").toString();
    a.source = d.value("source").toString();
    a.notes = d.value("notes").toString();
    for (const QVariant &i : d.value("images").toList()) a.images << i.toString();
    a.sentenceAudio = d.value("sentenceAudio").toString();
    a.wordAudio = d.value("wordAudio").toString();
    a.frequency = d.value("frequency").toString();
    a.freqSort = d.value("freqSort").toInt();
    a.pitchPositions = d.value("pitch").toString();
    a.pitchCategories = d.value("pitchCategories").toString();
    return a;
}

} // namespace

void Cards::add(const QVariantMap &draftIn)
{
    const QVariantMap d = plain(draftIn).toMap();
    const QString deck = Settings::v("ankiDeck").toString(), model = Settings::v("ankiModel").toString();
    const QVariantMap fields = Settings::v("ankiFields").toMap();
    if (deck.isEmpty() || model.isEmpty() || fields.isEmpty()) {
        emit failed(tr("Choose a deck, a note type and its fields in Settings → Anki first."));
        return;
    }
    const QStringList tags = Settings::v("ankiTags").toString().split(u' ', Qt::SkipEmptyParts);
    const bool dup = Settings::v("ankiDuplicates").toBool();
    busyInc(1);
    (void)QtConcurrent::run([this, d, deck, model, fields, tags, dup, c = conn()] {
        const anki::Note n = anki::buildNote(toDraft(d), deck, model, fields, tags, dup, QDateTime::currentMSecsSinceEpoch());
        const anki::Result r = anki::invoke(c, QStringLiteral("addNote"), {{"note", n.json}});
        QMetaObject::invokeMethod(this, [this, r, d] {
            busyInc(-1);
            if (!r.ok()) {
                emit failed(r.error.contains(u"duplicate") ? tr("Anki already has this card. Turn on duplicates in Settings → Anki to add it anyway.") : r.error);
                return;
            }
            const qlonglong id = r.value.toVariant().toLongLong();
            const QString key = d.value("key").toString();
            db::exec(db::app(), QStringLiteral("INSERT INTO cards (note, expression, sentence, item, time) VALUES (?,?,?,?,?)"),
                     {id, d.value("expression"), d.value("sentence"), d.value("itemId"), QDateTime::currentSecsSinceEpoch()});
            if (!key.isEmpty()) {
                Words::instance().addCard(key);
                // A word you make a card for is one you're learning.
                const Status s = Words::instance().status(key);
                if (s == Status::Unknown || s == Status::Tracked) Words::instance().setManual({{key, Status::Learning}});
            }
            emit added(id, d.value("expression").toString());
            emit toast(tr("Card added: %1").arg(d.value("expression").toString().isEmpty() ? d.value("sentence").toString().left(20) : d.value("expression").toString()), QStringLiteral("teal"));
        });
    });
}

void Cards::addMediaToLast(const QVariantMap &draftIn)
{
    const QVariantMap d = plain(draftIn).toMap();
    const QVariantMap fields = Settings::v("ankiFields").toMap();
    busyInc(1);
    (void)QtConcurrent::run([this, d, fields, c = conn()] {
        // The newest note added today, whatever made it (often Yomitan).
        const anki::Result found = anki::invoke(c, QStringLiteral("findNotes"), {{"query", "added:1"}});
        qlonglong last = 0;
        for (const QJsonValue &v : found.value.toArray()) last = std::max<qlonglong>(last, v.toVariant().toLongLong());
        if (!found.ok() || !last) {
            QMetaObject::invokeMethod(this, [this, found] {
                busyInc(-1);
                emit failed(found.ok() ? tr("No note added in Anki today.") : found.error);
            });
            return;
        }
        // Media fields by template; on a different note type, fall back to field names.
        const anki::Result info = anki::invoke(c, QStringLiteral("notesInfo"), {{"notes", QJsonArray{last}}});
        const QJsonObject note = info.value.toArray().at(0).toObject();
        const QJsonObject noteFields = note.value("fields").toObject();
        QStringList audioFields, imageFields;
        for (auto it = noteFields.begin(); it != noteFields.end(); ++it) {
            const QString tpl = fields.value(it.key(), anki::guessField(it.key())).toString();
            if (tpl.contains(u"{sentence-audio}")) audioFields << it.key();
            if (tpl.contains(u"{image}")) imageFields << it.key();
        }
        const qint64 stamp = QDateTime::currentMSecsSinceEpoch();
        QJsonObject upd{{"id", last}, {"fields", QJsonObject{}}};
        QJsonArray audio, picture;
        const QString sa = d.value("sentenceAudio").toString();
        if (!sa.isEmpty() && !audioFields.isEmpty())
            audio.append(QJsonObject{{"path", sa}, {"filename", QStringLiteral("togu_%1_sentence.%2").arg(stamp).arg(QFileInfo(sa).suffix())},
                                     {"fields", QJsonArray::fromStringList(audioFields)}});
        for (const QVariant &img : d.value("images").toList())
            if (!imageFields.isEmpty())
                picture.append(QJsonObject{{"path", img.toString()}, {"filename", QStringLiteral("togu_%1_image.%2").arg(stamp).arg(QFileInfo(img.toString()).suffix())},
                                           {"fields", QJsonArray::fromStringList(imageFields)}});
        if (!audio.isEmpty()) upd.insert("audio", audio);
        if (!picture.isEmpty()) upd.insert("picture", picture);
        const anki::Result r = anki::invoke(c, QStringLiteral("updateNoteFields"), {{"note", upd}});
        const QString expr = anki::plainField(noteFields.value(noteFields.keys().value(0)).toObject().value("value").toString());
        QMetaObject::invokeMethod(this, [this, r, expr, audioFields, imageFields] {
            busyInc(-1);
            if (!r.ok()) emit failed(r.error);
            else if (audioFields.isEmpty() && imageFields.isEmpty()) emit failed(tr("The last note has no audio or picture field Togu recognises."));
            else emit toast(tr("Media added to %1").arg(expr), QStringLiteral("teal"));
        });
    });
}

void Cards::browse(qlonglong noteId)
{
    (void)QtConcurrent::run([c = conn(), noteId] { anki::invoke(c, QStringLiteral("guiBrowse"), {{"query", QStringLiteral("nid:%1").arg(noteId)}}); });
}

QVariantList Cards::history(int limit) const
{
    QVariantList out;
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT note, expression, sentence, time FROM cards ORDER BY time DESC LIMIT ?"));
    q.addBindValue(limit);
    db::exec(q);
    while (q.next())
        out << QVariantMap{{"note", q.value(0)}, {"expression", q.value(1)}, {"sentence", q.value(2)}, {"time", q.value(3)}};
    return out;
}

// Which words already have a note: the word field of every note of the card note type.
void Cards::syncCardMarks()
{
    const QString model = Settings::v("ankiModel").toString();
    if (model.isEmpty()) return;
    const QVariantMap fields = Settings::v("ankiFields").toMap();
    QString wordField, readingField;
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        const QString t = it.value().toString().trimmed();
        if (t == u"{expression}" && wordField.isEmpty()) wordField = it.key();
        if (t == u"{reading}" && readingField.isEmpty()) readingField = it.key();
    }
    if (wordField.isEmpty()) return;
    (void)QtConcurrent::run([this, model, wordField, readingField, c = conn()] {
        const anki::Result ids = anki::invoke(c, QStringLiteral("findNotes"), {{"query", QStringLiteral("\"note:%1\"").arg(model)}});
        if (!ids.ok()) return;
        QSet<QString> keys;
        const QJsonArray all = ids.value.toArray();
        for (qsizetype i = 0; i < all.size(); i += 500) {
            QJsonArray chunk;
            for (qsizetype j = i; j < std::min(all.size(), i + 500); ++j) chunk.append(all[j]);
            const anki::Result info = anki::invoke(c, QStringLiteral("notesInfo"), {{"notes", chunk}});
            for (const QJsonValue &n : info.value.toArray()) {
                const QJsonObject f = n.toObject().value("fields").toObject();
                const QString word = anki::plainField(f.value(wordField).toObject().value("value").toString());
                const QString reading = readingField.isEmpty() ? QString() : anki::plainField(f.value(readingField).toObject().value("value").toString());
                if (!word.isEmpty()) keys.insert(jp::wordKey(word, reading.isEmpty() || reading == word ? QString() : reading));
                if (!word.isEmpty() && reading.isEmpty()) keys.insert(word);
            }
        }
        QMetaObject::invokeMethod(this, [keys] { Words::instance().setCards(keys); });
    });
}
