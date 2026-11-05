#include "words.h"

#include "core/jp/tokenizer.h"
#include "core/util/db.h"

#include <QDateTime>

QString statusName(Status s)
{
    switch (s) {
    case Status::Tracked: return QStringLiteral("tracked");
    case Status::Learning: return QStringLiteral("learning");
    case Status::Known: return QStringLiteral("known");
    case Status::Ignored: return QStringLiteral("ignored");
    default: return QStringLiteral("unknown");
    }
}

Status statusFromName(const QString &s)
{
    if (s == u"tracked") return Status::Tracked;
    if (s == u"learning") return Status::Learning;
    if (s == u"known") return Status::Known;
    if (s == u"ignored") return Status::Ignored;
    return Status::Unknown;
}

Words &Words::instance()
{
    static Words w;
    return w;
}

void Words::load()
{
    QWriteLocker lock(&m_lock);
    m_rows.clear();
    m_cardMod.clear();
    QSqlQuery q(db::app());
    q.exec(QStringLiteral("SELECT w, s, manual, anki_mod FROM words"));
    while (q.next())
        m_rows.insert(q.value(0).toString(), {Status(q.value(1).toInt()), q.value(2).toBool(), q.value(3).toLongLong()});
    q.exec(QStringLiteral("SELECT word, MAX(mod) FROM anki_cards GROUP BY word"));
    while (q.next()) m_cardMod.insert(q.value(0).toString(), q.value(1).toLongLong());
    m_loaded = true;
}

Status Words::status(const QString &key) const
{
    QReadLocker lock(&m_lock);
    auto it = m_rows.constFind(key);
    if (it != m_rows.constEnd()) return it->s;
    it = m_rows.constFind(jp::splitKey(key).first);
    return it != m_rows.constEnd() ? it->s : Status::Unknown;
}

bool Words::manual(const QString &key) const
{
    QReadLocker lock(&m_lock);
    auto it = m_rows.constFind(key);
    return it != m_rows.constEnd() && it->manual;
}

void Words::setManual(const QList<std::pair<QString, Status>> &entries)
{
    QStringList keys;
    {
        QWriteLocker lock(&m_lock);
        auto d = db::app();
        db::Tx tx(d);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (const auto &[w, s] : entries) {
            if (w.isEmpty()) continue;
            keys << w;
            const qint64 mod = m_cardMod.value(w, 0);
            if (s == Status::Unknown && !mod) {
                m_rows.remove(w);
                db::exec(d, QStringLiteral("DELETE FROM words WHERE w=?"), {w});
            } else {
                // Unknown on an Anki word is stored too, so the override is remembered.
                m_rows.insert(w, {s, true, mod});
                db::exec(d, QStringLiteral("INSERT INTO words (w, s, t, c, manual, anki_mod) VALUES (?,?,?,?,1,?)"
                                           " ON CONFLICT(w) DO UPDATE SET s=excluded.s, t=excluded.t, manual=1, anki_mod=excluded.anki_mod"),
                         {w, int(s), now, now, mod ? QVariant(mod) : QVariant()});
            }
        }
        tx.commit();
    }
    if (!keys.isEmpty()) emit changed(keys);
}

void Words::applyAnki(const QList<AnkiState> &states)
{
    QStringList keys;
    {
        QWriteLocker lock(&m_lock);
        auto d = db::app();
        db::Tx tx(d);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        for (const AnkiState &a : states) {
            m_cardMod[a.key] = a.mod;
            auto it = m_rows.find(a.key);
            // A hand setting made against this exact card state stays.
            if (it != m_rows.end() && it->manual && it->ankiMod == a.mod) continue;
            if (it != m_rows.end() && !it->manual && it->s == a.status) continue;
            if (a.status == Status::Unknown) {
                m_rows.remove(a.key);
                db::exec(d, QStringLiteral("DELETE FROM words WHERE w=?"), {a.key});
            } else {
                m_rows.insert(a.key, {a.status, false, a.mod});
                db::exec(d, QStringLiteral("INSERT INTO words (w, s, t, c, manual, anki_mod) VALUES (?,?,?,?,0,?)"
                                           " ON CONFLICT(w) DO UPDATE SET s=excluded.s, t=excluded.t, manual=0, anki_mod=excluded.anki_mod"),
                         {a.key, int(a.status), now, now, a.mod});
            }
            keys << a.key;
        }
        tx.commit();
    }
    if (!keys.isEmpty()) emit changed(keys);
}

void Words::importList(const QStringList &keys, Status s)
{
    QList<std::pair<QString, Status>> e;
    for (const QString &k : keys)
        if (!k.trimmed().isEmpty()) e.append({k.trimmed(), s});
    setManual(e);
}

void Words::clear()
{
    {
        QWriteLocker lock(&m_lock);
        m_rows.clear();
        db::exec(db::app(), QStringLiteral("DELETE FROM words"));
    }
    emit changed({});
}

QHash<Status, int> Words::counts() const
{
    QReadLocker lock(&m_lock);
    QHash<Status, int> out;
    for (const Row &r : m_rows) out[r.s]++;
    return out;
}

int Words::knownCount() const { return counts().value(Status::Known); }

QList<std::pair<QString, Status>> Words::all() const
{
    QReadLocker lock(&m_lock);
    QList<std::pair<QString, Status>> out;
    out.reserve(m_rows.size());
    for (auto it = m_rows.constBegin(); it != m_rows.constEnd(); ++it) out.append({it.key(), it->s});
    return out;
}
