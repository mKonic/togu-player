#include "dict.h"

#include "core/jp/jp.h"
#include "core/util/arc.h"
#include "core/util/db.h"
#include "core/words/stats.h"
#include "glossary.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

Dict &Dict::instance()
{
    static Dict d;
    return d;
}

ParsedFreq parseFreq(const QJsonValue &v)
{
    ParsedFreq p;
    QJsonValue f = v;
    if (f.isObject() && f.toObject().contains("frequency")) {
        p.reading = f.toObject().value("reading").toString();
        f = f.toObject().value("frequency");
    }
    if (f.isDouble()) {
        p.value = f.toDouble();
        p.display = QString::number(p.value);
        p.ok = true;
    } else if (f.isString()) {
        bool ok = false;
        // parseFloat semantics: leading number of the string.
        auto m = QRegularExpression(QStringLiteral("^\\s*[-+]?\\d*\\.?\\d+")).match(f.toString());
        if (m.hasMatch()) p.value = m.captured().toDouble(&ok);
        p.display = f.toString();
        p.ok = ok;
    } else if (f.isObject() && f.toObject().contains("value")) {
        const QJsonObject o = f.toObject();
        p.value = o.value("value").toDouble();
        p.display = o.contains("displayValue") ? o.value("displayValue").toString() : QString::number(p.value);
        p.ok = true;
    }
    return p;
}

QList<DictInfo> Dict::dictsLocked()
{
    if (m_loaded) return m_dicts;
    m_dicts.clear();
    QSqlQuery q(db::dict());
    q.exec(QStringLiteral("SELECT id, title, revision, kind, ord, enabled, terms, metas FROM dicts ORDER BY ord"));
    while (q.next()) {
        DictInfo d;
        d.id = q.value(0).toInt();
        d.title = q.value(1).toString();
        d.revision = q.value(2).toString();
        d.kind = q.value(3).toString();
        d.order = q.value(4).toInt();
        d.enabled = q.value(5).toBool();
        d.terms = q.value(6).toInt();
        d.metas = q.value(7).toInt();
        m_dicts.append(d);
    }
    m_loaded = true;
    return m_dicts;
}

QList<DictInfo> Dict::list()
{
    QMutexLocker l(&m_mutex);
    return dictsLocked();
}

bool Dict::empty()
{
    QMutexLocker l(&m_mutex);
    return dictsLocked().isEmpty();
}

void Dict::invalidate()
{
    QMutexLocker l(&m_mutex);
    m_loaded = false;
    m_info.clear();
    m_gloss.clear();
}

void Dict::setEnabled(int id, bool on)
{
    db::exec(db::dict(), QStringLiteral("UPDATE dicts SET enabled=? WHERE id=?"), {on ? 1 : 0, id});
    invalidate();
}

void Dict::move(int id, int delta)
{
    QList<DictInfo> ds = list();
    auto it = std::find_if(ds.begin(), ds.end(), [&](auto &d) { return d.id == id; });
    if (it == ds.end()) return;
    qsizetype i = it - ds.begin(), j = std::clamp<qsizetype>(i + delta, 0, ds.size() - 1);
    ds.move(i, j);
    auto d = db::dict();
    db::Tx tx(d);
    for (qsizetype k = 0; k < ds.size(); ++k) db::exec(d, QStringLiteral("UPDATE dicts SET ord=? WHERE id=?"), {int(k), ds[k].id});
    tx.commit();
    invalidate();
}

void Dict::remove(int id)
{
    auto d = db::dict();
    db::Tx tx(d);
    for (const char *t : {"terms", "meta", "tags", "kanji", "media"})
        db::exec(d, QStringLiteral("DELETE FROM %1 WHERE d=?").arg(QLatin1String(t)), {id});
    db::exec(d, QStringLiteral("DELETE FROM dicts WHERE id=?"), {id});
    tx.commit();
    invalidate();
}

QString Dict::import(const QString &path, const std::function<void(const Progress &)> &progress)
{
    auto report = [&](const QString &title, const QString &phase, int done, int total) {
        if (progress) progress({title, phase, done, total});
    };
    const QByteArray indexJson = arc::read(path, QStringLiteral("index.json"));
    if (indexJson.isEmpty()) return QStringLiteral("error:Not a Yomitan dictionary: index.json is missing");
    const QJsonObject index = QJsonDocument::fromJson(indexJson).object();
    const QString title = index.value("title").toString();
    if (title.isEmpty()) return QStringLiteral("error:Dictionary has no title");
    const int format = index.contains("format") ? index.value("format").toInt() : index.value("version").toInt(1);

    for (const DictInfo &d : list())
        if (d.title == title) {
            report(title, QStringLiteral("Replacing previous copy"), 0, 0);
            remove(d.id);
        }

    const QStringList names = arc::list(path);
    static const QRegularExpression bankRe(QStringLiteral("^(term|term_meta|tag|kanji|kanji_meta)_bank_\\d+\\.json$"));
    static const QSet<QString> imageExt = {"png", "jpg", "jpeg", "gif", "webp", "svg", "avif"};
    int total = 0;
    for (const QString &n : names)
        if (bankRe.match(n).hasMatch() || imageExt.contains(n.section(u'.', -1).toLower())) ++total;

    auto d = db::dict();
    db::exec(d, QStringLiteral("INSERT INTO dicts (title, revision, ord, enabled, source) VALUES (?, ?, (SELECT COALESCE(MAX(ord)+1, 0) FROM dicts), 1, ?)"),
             {title, index.value("revision").toVariant().toString(), path});
    const int id = db::scalar(d, QStringLiteral("SELECT id FROM dicts WHERE title=?"), {title}).toInt();

    int step = 0, terms = 0, metas = 0, freq = 0, pitch = 0, kanjis = 0;
    // Pitch for words written in kana only: the accent their kanji spellings agree on.
    QHash<QString, QString> byReading; // reading -> pitches json, or "" when spellings disagree
    QSet<QString> pitchExprs;
    QString styles;

    db::Tx tx(d);
    QSqlQuery insTerm(d), insMeta(d), insTag(d), insKanji(d), insMedia(d);
    insTerm.prepare(QStringLiteral("INSERT INTO terms (d, e, r, t, ru, s, g, q, tt) VALUES (?,?,?,?,?,?,?,?,?)"));
    insMeta.prepare(QStringLiteral("INSERT INTO meta (d, e, m, v) VALUES (?,?,?,?)"));
    insTag.prepare(QStringLiteral("INSERT INTO tags (d, name, category, ord, notes, score) VALUES (?,?,?,?,?,?)"));
    insKanji.prepare(QStringLiteral("INSERT INTO kanji (d, c, onyomi, kunyomi, tags, meanings, stats) VALUES (?,?,?,?,?,?,?)"));
    insMedia.prepare(QStringLiteral("INSERT OR REPLACE INTO media (d, path, data) VALUES (?,?,?)"));
    auto str = [](const QJsonValue &v) { return v.isString() ? v.toString() : v.isDouble() ? QString::number(v.toDouble()) : QString(); };

    arc::each(path, [&](const QString &n) {
        return bankRe.match(n).hasMatch() || imageExt.contains(n.section(u'.', -1).toLower()) || n == u"styles.css";
    }, [&](const QString &name, const QByteArray &data) {
        if (name == u"styles.css") {
            styles = QString::fromUtf8(data);
            return true;
        }
        report(title, imageExt.contains(name.section(u'.', -1).toLower()) ? QStringLiteral("Importing images") : QStringLiteral("Importing entries"), step++, total);
        if (!name.endsWith(u".json")) {
            insMedia.addBindValue(id);
            insMedia.addBindValue(name);
            insMedia.addBindValue(data);
            db::exec(insMedia);
            return true;
        }
        const QJsonArray rows = QJsonDocument::fromJson(data).array();
        if (name.startsWith(u"term_bank_")) {
            for (const QJsonValue &rv : rows) {
                const QJsonArray row = rv.toArray();
                const QString e = row.at(0).toString();
                const QString r = row.at(1).toString().isEmpty() ? e : row.at(1).toString();
                QJsonArray g;
                if (format >= 3) g = row.at(5).toArray();
                else for (qsizetype k = 5; k < row.size(); ++k) g.append(row.at(k));
                insTerm.addBindValue(id);
                insTerm.addBindValue(e);
                insTerm.addBindValue(r);
                insTerm.addBindValue(str(row.at(2)));
                insTerm.addBindValue(str(row.at(3)));
                insTerm.addBindValue(row.at(4).toDouble());
                insTerm.addBindValue(QString::fromUtf8(QJsonDocument(g).toJson(QJsonDocument::Compact)));
                insTerm.addBindValue(format >= 3 ? row.at(6).toInt() : 0);
                insTerm.addBindValue(format >= 3 ? str(row.at(7)) : QString());
                db::exec(insTerm);
                ++terms;
            }
        } else if (name.startsWith(u"term_meta_bank_") || name.startsWith(u"kanji_meta_bank_")) {
            for (const QJsonValue &rv : rows) {
                const QJsonArray row = rv.toArray();
                const QString e = row.at(0).toString();
                const QString mode = row.at(1).toString();
                const QJsonValue v = row.at(2);
                if (mode == u"freq") ++freq;
                else if (mode == u"pitch") {
                    ++pitch;
                    pitchExprs.insert(e);
                    const QString reading = v.toObject().value("reading").toString();
                    if (!reading.isEmpty() && reading != e && jp::hasKanji(e)) {
                        const QString sig = QString::fromUtf8(QJsonDocument(v.toObject().value("pitches").toArray()).toJson(QJsonDocument::Compact));
                        auto it = byReading.find(reading);
                        if (it == byReading.end()) byReading.insert(reading, sig);
                        else if (*it != sig) *it = QString();
                    }
                }
                insMeta.addBindValue(id);
                insMeta.addBindValue(e);
                insMeta.addBindValue(mode);
                insMeta.addBindValue(QString::fromUtf8(QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact)));
                db::exec(insMeta);
                ++metas;
            }
        } else if (name.startsWith(u"kanji_bank_")) {
            for (const QJsonValue &rv : rows) {
                const QJsonArray row = rv.toArray();
                insKanji.addBindValue(id);
                insKanji.addBindValue(row.at(0).toString());
                insKanji.addBindValue(row.at(1).toString());
                insKanji.addBindValue(row.at(2).toString());
                insKanji.addBindValue(row.at(3).toString());
                insKanji.addBindValue(QString::fromUtf8(QJsonDocument(row.at(4).toArray()).toJson(QJsonDocument::Compact)));
                insKanji.addBindValue(QString::fromUtf8(QJsonDocument(row.at(5).toObject()).toJson(QJsonDocument::Compact)));
                db::exec(insKanji);
                ++kanjis;
            }
        } else if (name.startsWith(u"tag_bank_")) {
            for (const QJsonValue &rv : rows) {
                const QJsonArray row = rv.toArray();
                insTag.addBindValue(id);
                insTag.addBindValue(row.at(0).toString());
                insTag.addBindValue(row.at(1).toString());
                insTag.addBindValue(row.at(2).toInt());
                insTag.addBindValue(row.at(3).toString());
                insTag.addBindValue(row.at(4).toDouble());
                db::exec(insTag);
            }
        }
        return true;
    });

    for (auto it = byReading.constBegin(); it != byReading.constEnd(); ++it) {
        if (it->isEmpty() || pitchExprs.contains(it.key())) continue;
        QJsonObject v{{"reading", it.key()}, {"pitches", QJsonDocument::fromJson(it->toUtf8()).array()}};
        insMeta.addBindValue(id);
        insMeta.addBindValue(it.key());
        insMeta.addBindValue(QStringLiteral("pitch"));
        insMeta.addBindValue(QString::fromUtf8(QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact)));
        db::exec(insMeta);
    }

    const QString kind = terms > 0 ? (metas > 0 ? "mixed" : "terms") : kanjis > 0 ? "kanji" : pitch > freq ? "pitch" : "freq";
    db::exec(d, QStringLiteral("UPDATE dicts SET terms=?, metas=?, kind=?, styles=? WHERE id=?"), {terms, metas, kind, styles, id});
    tx.commit();
    invalidate();
    report(title, QStringLiteral("Done"), total, total);
    return title;
}

QString Dict::primaryFreqTitle()
{
    QMutexLocker l(&m_mutex);
    const auto ds = dictsLocked();
    for (const auto &d : ds)
        if (d.enabled && d.kind == u"freq") return d.title;
    for (const auto &d : ds)
        if (d.enabled && d.metas > 0 && d.kind != u"pitch") return d.title;
    return {};
}

namespace {

struct Candidate {
    QString text;
    int len;
};

// Candidate dictionary keys for the text starting at the hovered word, longest
// first. The last analyzed piece is also tried in its dictionary form so
// inflected text still finds its entry.
QList<Candidate> candidates(const QList<jp::RawToken> &raw, int maxLen = 20)
{
    QList<Candidate> out;
    QSet<QString> seen;
    auto add = [&](const QString &text, int len) {
        if (text.trimmed().isEmpty() || seen.contains(text)) return;
        seen.insert(text);
        out.append({text, len});
    };
    QString prefix;
    struct Step {
        QString surface, base;
        int len;
    };
    QList<Step> steps;
    for (const jp::RawToken &t : raw) {
        if (t.surface.trimmed().isEmpty()) break;
        const int len = int(prefix.size() + t.surface.size());
        if (len > maxLen) break;
        steps.append({prefix + t.surface, prefix + (!t.baseForm.isEmpty() && t.baseForm != u"*" ? t.baseForm : t.surface), len});
        prefix += t.surface;
    }
    std::reverse(steps.begin(), steps.end());
    for (const Step &s : steps) {
        add(s.surface, s.len);
        add(s.base, s.len);
    }
    return out;
}

struct TermRow {
    int d;
    QString e, r, t, tt;
    double s;
    QString g;
    int q;
};

} // namespace

QList<DictEntry> Dict::lookup(const QString &textFromWord, int max)
{
    QList<DictInfo> ds;
    {
        QMutexLocker l(&m_mutex);
        ds = dictsLocked();
    }
    QHash<int, int> order;
    QHash<int, QString> names;
    for (const auto &d : ds)
        if (d.enabled) {
            order.insert(d.id, d.order);
            names.insert(d.id, d.title);
        }
    if (order.isEmpty()) return {};
    const QString freqTitle = primaryFreqTitle();

    struct Acc {
        DictEntry entry;
        QList<TermRow> rows;
        bool viaReading = true;
    };
    QHash<QString, Acc> entries;
    QList<QString> orderKeys;
    auto d = db::dict();
    QSqlQuery byE(d), byR(d);
    byE.prepare(QStringLiteral("SELECT d, e, r, t, tt, s, g, q FROM terms WHERE e=?"));
    byR.prepare(QStringLiteral("SELECT d, e, r, t, tt, s, g, q FROM terms WHERE r=?"));

    for (const Candidate &c : candidates(jp::tokenizeRaw(textFromWord.left(40)))) {
        QSet<QString> variants{c.text};
        if (!jp::hasKanji(c.text)) {
            variants.insert(jp::toHiragana(c.text));
            variants.insert(jp::toKatakana(c.text));
        }
        for (const QString &v : variants) {
            auto run = [&](QSqlQuery &q, bool viaReading) {
                q.addBindValue(v);
                if (!db::exec(q)) return;
                while (q.next()) {
                    TermRow row{q.value(0).toInt(), q.value(1).toString(), q.value(2).toString(), q.value(3).toString(),
                                q.value(4).toString(), q.value(5).toDouble(), q.value(6).toString(), q.value(7).toInt()};
                    if (!order.contains(row.d)) continue;
                    const QString key = row.e + QChar(0) + row.r;
                    auto it = entries.find(key);
                    if (it == entries.end()) {
                        Acc a;
                        a.entry.expression = row.e;
                        a.entry.reading = row.r;
                        a.entry.matched = c.text;
                        a.entry.matchLen = c.len;
                        a.viaReading = viaReading;
                        it = entries.insert(key, a);
                        orderKeys.append(key);
                    } else if (c.len > it->entry.matchLen) {
                        it->entry.matchLen = c.len;
                        it->entry.matched = c.text;
                    }
                    if (!viaReading) it->viaReading = false;
                    bool dup = std::any_of(it->rows.begin(), it->rows.end(), [&](const TermRow &r) {
                        return r.d == row.d && r.q == row.q && r.g == row.g;
                    });
                    if (!dup) it->rows.append(row);
                }
            };
            run(byE, false);
            if (!jp::hasKanji(v)) run(byR, true);
        }
    }

    QSqlQuery metaQ(d);
    metaQ.prepare(QStringLiteral("SELECT d, m, v FROM meta WHERE e=?"));
    QList<Acc> list;
    for (const QString &k : orderKeys) list.append(entries.value(k));
    for (Acc &a : list) {
        DictEntry &e = a.entry;
        metaQ.addBindValue(e.expression);
        db::exec(metaQ);
        int rank = 0;
        while (metaQ.next()) {
            const int did = metaQ.value(0).toInt();
            if (!order.contains(did)) continue;
            const QString m = metaQ.value(1).toString();
            const QJsonValue v = QJsonDocument::fromJson(metaQ.value(2).toByteArray()).array().at(0);
            if (m == u"freq") {
                ParsedFreq f = parseFreq(v);
                if (!f.ok || (!f.reading.isEmpty() && !e.reading.isEmpty() && f.reading != e.reading)) continue;
                e.freqs.append({names.value(did), f.value, f.display});
                if (names.value(did) == freqTitle && (rank == 0 || f.value < rank)) rank = int(f.value);
            } else if (m == u"pitch") {
                const QJsonObject p = v.toObject();
                if (!e.reading.isEmpty() && p.value("reading").toString() != e.reading) continue;
                PitchInfo pi{names.value(did), p.value("reading").toString(), {}};
                for (const QJsonValue &x : p.value("pitches").toArray()) pi.positions.append(x.toObject().value("position").toInt());
                e.pitches.append(pi);
            }
        }
        std::stable_sort(e.freqs.begin(), e.freqs.end(), [&](auto &x, auto &y) {
            auto ox = std::find_if(ds.begin(), ds.end(), [&](auto &dd) { return dd.title == x.dict; });
            auto oy = std::find_if(ds.begin(), ds.end(), [&](auto &dd) { return dd.title == y.dict; });
            return ox - ds.begin() < oy - ds.begin();
        });
        e.rank = rank;
        e.stars = starsForRank(rank);
        std::stable_sort(a.rows.begin(), a.rows.end(), [&](const TermRow &x, const TermRow &y) {
            if (order.value(x.d) != order.value(y.d)) return order.value(x.d) < order.value(y.d);
            return x.s > y.s;
        });
        for (const TermRow &r : a.rows) {
            GlossBlock b;
            b.dictId = r.d;
            b.dict = names.value(r.d);
            b.defTags = r.t.split(u' ', Qt::SkipEmptyParts);
            b.termTags = r.tt.split(u' ', Qt::SkipEmptyParts);
            b.glossary = QJsonDocument::fromJson(r.g.toUtf8()).array();
            e.blocks.append(b);
        }
    }
    auto rankOf = [&](const DictEntry &e) {
        for (const auto &f : e.freqs)
            if (f.dict == freqTitle) return f.value;
        return 1e18;
    };
    std::stable_sort(list.begin(), list.end(), [&](const Acc &a, const Acc &b) {
        if (a.entry.matchLen != b.entry.matchLen) return a.entry.matchLen > b.entry.matchLen;
        if (a.viaReading != b.viaReading) return !a.viaReading;
        if (rankOf(a.entry) != rankOf(b.entry)) return rankOf(a.entry) < rankOf(b.entry);
        int oa = order.value(a.rows.value(0).d), ob = order.value(b.rows.value(0).d);
        if (oa != ob) return oa < ob;
        return a.rows.value(0).s > b.rows.value(0).s;
    });
    QList<DictEntry> out;
    for (qsizetype i = 0; i < list.size() && i < max; ++i) out.append(list[i].entry);
    return out;
}

QList<KanjiEntry> Dict::kanji(const QString &character)
{
    QList<KanjiEntry> out;
    QHash<int, QString> names;
    for (const auto &d : list())
        if (d.enabled) names.insert(d.id, d.title);
    QSqlQuery q(db::dict());
    q.prepare(QStringLiteral("SELECT d, onyomi, kunyomi, tags, meanings FROM kanji WHERE c=?"));
    q.addBindValue(character);
    db::exec(q);
    while (q.next()) {
        if (!names.contains(q.value(0).toInt())) continue;
        KanjiEntry k;
        k.character = character;
        k.dict = names.value(q.value(0).toInt());
        k.onyomi = q.value(1).toString().split(u' ', Qt::SkipEmptyParts);
        k.kunyomi = q.value(2).toString().split(u' ', Qt::SkipEmptyParts);
        k.tags = q.value(3).toString().split(u' ', Qt::SkipEmptyParts);
        for (const QJsonValue &m : QJsonDocument::fromJson(q.value(4).toByteArray()).array()) k.meanings << m.toString();
        out.append(k);
    }
    return out;
}

QHash<QString, WordInfo> Dict::info(const QStringList &keys)
{
    QHash<QString, WordInfo> out;
    QStringList missing;
    QHash<int, QString> names;
    QString freqTitle = primaryFreqTitle();
    {
        QMutexLocker l(&m_mutex);
        bool any = false;
        for (const auto &d : dictsLocked())
            if (d.enabled) {
                names.insert(d.id, d.title);
                any |= d.metas > 0;
            }
        if (!any) return out;
        for (const QString &k : keys) {
            auto it = m_info.constFind(k);
            if (it != m_info.constEnd()) out.insert(k, *it);
            else missing << k;
        }
    }
    if (missing.isEmpty()) return out;
    QSqlQuery q(db::dict());
    q.prepare(QStringLiteral("SELECT d, m, v FROM meta WHERE e=?"));
    QHash<QString, WordInfo> fresh;
    for (const QString &k : missing) {
        if (fresh.contains(k)) continue;
        auto [lemma, reading] = jp::splitKey(k);
        q.addBindValue(lemma);
        db::exec(q);
        WordInfo info;
        while (q.next()) {
            const QString dname = names.value(q.value(0).toInt());
            if (dname.isEmpty()) continue;
            const QString m = q.value(1).toString();
            const QJsonValue v = QJsonDocument::fromJson(q.value(2).toByteArray()).array().at(0);
            if (m == u"freq" && dname == freqTitle) {
                ParsedFreq f = parseFreq(v);
                if (f.ok && (f.reading.isEmpty() || reading.isEmpty() || f.reading == reading) && (info.rank == 0 || f.value < info.rank))
                    info.rank = int(f.value);
            } else if (m == u"pitch" && info.pitch.isEmpty()) {
                const QJsonObject p = v.toObject();
                if (reading.isEmpty() || p.value("reading").toString() == reading)
                    for (const QJsonValue &x : p.value("pitches").toArray()) info.pitch.append(x.toObject().value("position").toInt());
            }
        }
        fresh.insert(k, info);
    }
    QMutexLocker l(&m_mutex);
    for (auto it = fresh.constBegin(); it != fresh.constEnd(); ++it) {
        m_info.insert(it.key(), *it);
        out.insert(it.key(), *it);
    }
    return out;
}

QHash<QString, QString> Dict::gloss(const QStringList &keys)
{
    QHash<QString, QString> out;
    QHash<int, int> order;
    QStringList missing;
    {
        QMutexLocker l(&m_mutex);
        for (const auto &d : dictsLocked())
            if (d.enabled && d.terms > 0) order.insert(d.id, d.order);
        if (order.isEmpty()) return out;
        for (const QString &k : keys) {
            auto it = m_gloss.constFind(k);
            if (it != m_gloss.constEnd()) out.insert(k, *it);
            else missing << k;
        }
    }
    QSqlQuery q(db::dict());
    q.prepare(QStringLiteral("SELECT d, r, s, g FROM terms WHERE e=?"));
    QHash<QString, QString> fresh;
    for (const QString &k : missing) {
        auto [lemma, reading] = jp::splitKey(k);
        q.addBindValue(lemma);
        db::exec(q);
        int bestD = -1;
        bool bestR = false;
        double bestS = 0;
        QString bestG;
        while (q.next()) {
            const int did = q.value(0).toInt();
            if (!order.contains(did)) continue;
            const bool rMatch = q.value(1).toString() == reading;
            const double s = q.value(2).toDouble();
            bool better = bestD < 0 || (rMatch && !bestR) ||
                          (rMatch == bestR && (order.value(did) < order.value(bestD) || (did == bestD && s > bestS)));
            if (better) {
                bestD = did;
                bestR = rMatch;
                bestS = s;
                bestG = q.value(3).toString();
            }
        }
        fresh.insert(k, bestD < 0 ? QString() : glossary::shortGloss(QJsonDocument::fromJson(bestG.toUtf8()).array()));
    }
    QMutexLocker l(&m_mutex);
    for (auto it = fresh.constBegin(); it != fresh.constEnd(); ++it) {
        m_gloss.insert(it.key(), *it);
        out.insert(it.key(), *it);
    }
    return out;
}

QByteArray Dict::media(int dictId, const QString &path)
{
    return db::scalar(db::dict(), QStringLiteral("SELECT data FROM media WHERE d=? AND path=?"), {dictId, path}).toByteArray();
}
