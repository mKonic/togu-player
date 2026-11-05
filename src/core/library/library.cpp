#include "library.h"

#include "core/dict/dict.h"
#include "core/jp/tokenizer.h"
#include "core/media/probe.h"
#include "core/subs/subs.h"
#include "core/util/db.h"
#include "core/words/stats.h"
#include "core/words/words.h"
#include "names.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QSet>

namespace library {

QList<Root> roots(const QString &kind)
{
    QList<Root> out;
    QSqlQuery q(db::app());
    if (kind.isEmpty()) q.exec(QStringLiteral("SELECT id, path, kind FROM roots ORDER BY path"));
    else {
        q.prepare(QStringLiteral("SELECT id, path, kind FROM roots WHERE kind=? ORDER BY path"));
        q.addBindValue(kind);
        db::exec(q);
    }
    while (q.next()) out.append({q.value(0).toInt(), q.value(1).toString(), q.value(2).toString()});
    return out;
}

int addRoot(const QString &path, const QString &kind)
{
    const QString p = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    db::exec(db::app(), QStringLiteral("INSERT OR IGNORE INTO roots (path, kind) VALUES (?, ?)"), {p, kind});
    return db::scalar(db::app(), QStringLiteral("SELECT id FROM roots WHERE path=?"), {p}).toInt();
}

void removeRoot(int id)
{
    auto d = db::app();
    db::Tx tx(d);
    db::exec(d, QStringLiteral("DELETE FROM lines WHERE item IN (SELECT i.id FROM items i JOIN series s ON s.id=i.series WHERE s.root=?)"), {id});
    db::exec(d, QStringLiteral("DELETE FROM items WHERE series IN (SELECT id FROM series WHERE root=?)"), {id});
    db::exec(d, QStringLiteral("DELETE FROM series WHERE root=?"), {id});
    db::exec(d, QStringLiteral("DELETE FROM roots WHERE id=?"), {id});
    tx.commit();
}

namespace {

QString findPoster(const QDir &dir)
{
    static const QStringList names = {"poster", "folder", "cover", "show", "season", "front", "album", "fanart", "banner"};
    const auto files = dir.entryInfoList(QDir::Files);
    for (const QString &n : names)
        for (const QFileInfo &f : files)
            if (names::isImage(f.fileName()) && f.completeBaseName().compare(n, Qt::CaseInsensitive) == 0) return f.absoluteFilePath();
    // A single image in the folder is probably its cover.
    QStringList images;
    for (const QFileInfo &f : files)
        if (names::isImage(f.fileName())) images << f.absoluteFilePath();
    return images.size() == 1 ? images.first() : QString();
}

bool isNoise(const QString &dirName)
{
    static const QRegularExpression re(QStringLiteral("^(subs?|subtitles?|extras?|featurettes?|samples?|scans?|fonts?|attachments?|ncop|nced|menu)$"),
                                       QRegularExpression::CaseInsensitiveOption);
    return re.match(dirName).hasMatch() || dirName.startsWith(u'.');
}

bool isSeasonDir(const QString &name)
{
    static const QRegularExpression re(QStringLiteral("^(season|series|s|第)\\s*\\d+|^specials?$|^\\d+(st|nd|rd|th)\\s+season"), QRegularExpression::CaseInsensitiveOption);
    return re.match(name.trimmed()).hasMatch();
}

} // namespace

ScanResult scan(const Root &root, const std::atomic_bool *cancel)
{
    ScanResult r;
    const QDir base(root.path);
    if (!base.exists()) return r;
    const bool audio = root.kind == u"audio";
    auto d = db::app();

    struct Found {
        QString path, seriesFolder, seriesTitle;
        names::Parsed parsed;
        qint64 size, mtime;
    };
    QList<Found> found;
    QDirIterator it(root.path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext()) {
        if (cancel && *cancel) return r;
        const QString p = it.next();
        const QFileInfo fi = it.fileInfo();
        if (audio ? !names::isAudio(p) : !names::isVideo(p)) continue;
        const QString rel = base.relativeFilePath(p);
        QStringList parts = rel.split(u'/');
        parts.removeLast();
        bool skip = false;
        for (const QString &part : parts)
            if (isNoise(part)) skip = true;
        if (skip) continue;
        Found f;
        f.path = p;
        f.size = fi.size();
        f.mtime = fi.lastModified().toSecsSinceEpoch();
        f.parsed = names::parse(fi.fileName());
        if (!parts.isEmpty()) {
            // root/Show[/Season 1]/file: the first folder is the series.
            f.seriesFolder = parts.first();
            f.seriesTitle = names::cleanTitle(parts.first());
            if (parts.size() >= 2 && isSeasonDir(parts[1]) && !f.parsed.season) {
                static const QRegularExpression num(QStringLiteral("(\\d+)"));
                auto m = num.match(parts[1]);
                if (m.hasMatch()) f.parsed.season = m.captured(1).toInt();
            }
        } else {
            // Loose files: grouped by the title in their names.
            f.seriesTitle = f.parsed.title.isEmpty() ? fi.completeBaseName() : f.parsed.title;
            f.seriesFolder = QString();
        }
        if (f.seriesTitle.isEmpty()) f.seriesTitle = fi.completeBaseName();
        found.append(f);
    }

    db::Tx tx(d);
    QHash<QString, int> seriesIds; // folder + title -> id
    QSet<int> touchedSeries;
    QSet<QString> paths;
    for (const Found &f : found) {
        paths.insert(f.path);
        const QString skey = f.seriesFolder.isEmpty() ? u'\x01' + names::normalizedKey(f.seriesTitle) : f.seriesFolder;
        int sid = seriesIds.value(skey);
        if (!sid) {
            sid = db::scalar(d, QStringLiteral("SELECT id FROM series WHERE root=? AND folder=? AND (folder<>'' OR title=?)"),
                             {root.id, f.seriesFolder, f.seriesTitle}).toInt();
            const QString poster = f.seriesFolder.isEmpty() ? QString() : findPoster(QDir(base.filePath(f.seriesFolder)));
            if (!sid) {
                db::exec(d, QStringLiteral("INSERT INTO series (root, kind, title, folder, poster) VALUES (?,?,?,?,?)"),
                         {root.id, root.kind, f.seriesTitle, f.seriesFolder, poster});
                sid = db::scalar(d, QStringLiteral("SELECT last_insert_rowid()")).toInt();
                ++r.series;
            } else if (!poster.isEmpty()) {
                db::exec(d, QStringLiteral("UPDATE series SET poster=? WHERE id=?"), {poster, sid});
            }
            seriesIds.insert(skey, sid);
        }
        touchedSeries.insert(sid);
        const QVariant existing = db::scalar(d, QStringLiteral("SELECT id FROM items WHERE path=?"), {f.path});
        const QVariant ep = f.parsed.episode >= 0 ? QVariant(f.parsed.episode) : QVariant();
        const QString title = QFileInfo(f.path).completeBaseName();
        if (existing.isValid()) {
            db::exec(d, QStringLiteral("UPDATE items SET series=?, kind=?, season=?, episode=?, special=?, size=?, mtime=? WHERE id=?"),
                     {sid, root.kind, f.parsed.season, ep, f.parsed.special ? 1 : 0, f.size, f.mtime, existing});
        } else {
            db::exec(d, QStringLiteral("INSERT INTO items (series, path, kind, title, season, episode, special, size, mtime) VALUES (?,?,?,?,?,?,?,?,?)"),
                     {sid, f.path, root.kind, title, f.parsed.season, ep, f.parsed.special ? 1 : 0, f.size, f.mtime});
            ++r.added;
        }
    }
    // Items under this root that are gone from disk.
    QSqlQuery q(d);
    q.prepare(QStringLiteral("SELECT i.id, i.path FROM items i JOIN series s ON s.id=i.series WHERE s.root=?"));
    q.addBindValue(root.id);
    db::exec(q);
    QList<int> gone;
    while (q.next())
        if (!paths.contains(q.value(1).toString())) gone << q.value(0).toInt();
    for (int id : gone) {
        db::exec(d, QStringLiteral("DELETE FROM lines WHERE item=?"), {id});
        db::exec(d, QStringLiteral("DELETE FROM items WHERE id=?"), {id});
    }
    r.removed = int(gone.size());
    db::exec(d, QStringLiteral("DELETE FROM series WHERE root=? AND id NOT IN (SELECT DISTINCT series FROM items WHERE series IS NOT NULL)"), {root.id});
    tx.commit();
    return r;
}

QString subtitleFileFor(int itemId)
{
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT path, sub_path FROM items WHERE id=?"));
    q.addBindValue(itemId);
    if (!db::exec(q) || !q.next()) return {};
    const QString path = q.value(0).toString();
    const QString chosen = q.value(1).toString();
    if (chosen.startsWith(u"file:") && QFileInfo::exists(chosen.mid(5))) return chosen.mid(5);
    const auto matches = names::subtitlesFor(path);
    for (const auto &m : matches)
        if (m.lang == names::Lang::Japanese) return m.path;
    // Embedded Japanese text track.
    const media::Probe p = media::probe(path);
    const media::Stream *pick = nullptr;
    for (const media::Stream &s : p.streams) {
        if (!s.textSub()) continue;
        if (chosen == QStringLiteral("track:%1").arg(s.index)) { pick = &s; break; }
        if (!pick && media::isJapaneseLang(s.lang, s.title)) pick = &s;
    }
    if (pick) return media::extractSubtitle(path, *pick);
    for (const auto &m : matches)
        if (m.lang == names::Lang::Unknown) return m.path;
    return {};
}

QString encodeLine(const QString &text)
{
    QStringList out;
    static const QString ends = QStringLiteral("。｡！？!?");
    for (const jp::Token &t : jp::tokenize(subs::tidyLine(text))) {
        if (t.word) out << QString(t.key).replace(u' ', QChar(0x3000));
        else if (std::any_of(t.surface.begin(), t.surface.end(), [&](QChar c) { return ends.contains(c); })) out << QStringLiteral("。");
    }
    return out.join(u' ');
}

int indexItem(int itemId)
{
    const QString sub = subtitleFileFor(itemId);
    auto d = db::app();
    if (sub.isEmpty()) {
        db::exec(d, QStringLiteral("UPDATE items SET indexed_sub='' WHERE id=?"), {itemId});
        return 0;
    }
    const QString text = subs::readText(sub);
    const QString ext = QFileInfo(sub).suffix().toLower();
    const QList<SubLine> lines = subs::normalize(ext == u"ass" || ext == u"ssa" ? subs::splitAss(text).dialogue : subs::parse(sub, text));
    if (!subs::looksJapanese(lines)) {
        db::exec(d, QStringLiteral("UPDATE items SET indexed_sub='' WHERE id=?"), {itemId});
        return 0;
    }
    QStringList encoded;
    for (const SubLine &l : lines) encoded << encodeLine(l.text);
    db::Tx tx(d);
    db::exec(d, QStringLiteral("DELETE FROM lines WHERE item=?"), {itemId});
    QSqlQuery ins(d);
    ins.prepare(QStringLiteral("INSERT INTO lines (item, start, end, text, keys) VALUES (?,?,?,?,?)"));
    for (qsizetype i = 0; i < lines.size(); ++i) {
        ins.addBindValue(itemId);
        ins.addBindValue(lines[i].start);
        ins.addBindValue(lines[i].end);
        ins.addBindValue(lines[i].text);
        ins.addBindValue(encoded[i]);
        db::exec(ins);
    }
    const QFileInfo fi(sub);
    db::exec(d, QStringLiteral("UPDATE items SET indexed_sub=? WHERE id=?"),
             {sub + u'|' + QString::number(fi.size()) + u'|' + QString::number(fi.lastModified().toSecsSinceEpoch()), itemId});
    tx.commit();
    return int(lines.size());
}

QList<int> itemsToIndex()
{
    QList<int> out;
    QSqlQuery q(db::app());
    q.exec(QStringLiteral("SELECT id FROM items WHERE series IS NOT NULL AND kind IN ('video','audio') AND indexed_sub IS NULL ORDER BY id"));
    while (q.next()) out << q.value(0).toInt();
    return out;
}

void scoreItems(const QList<int> &itemIds)
{
    auto d = db::app();
    QList<int> ids = itemIds;
    if (ids.isEmpty()) {
        QSqlQuery q(d);
        q.exec(QStringLiteral("SELECT DISTINCT item FROM lines"));
        while (q.next()) ids << q.value(0).toInt();
    }
    const Words &words = Words::instance();
    const int known = words.knownCount();
    QSqlQuery q(d);
    q.prepare(QStringLiteral("SELECT keys FROM lines WHERE item=? ORDER BY start"));
    for (int id : ids) {
        q.addBindValue(id);
        db::exec(q);
        QList<QStringList> raw;
        QStringList allKeys;
        while (q.next()) {
            QStringList k = q.value(0).toString().split(u' ', Qt::SkipEmptyParts);
            for (QString &x : k) x.replace(QChar(0x3000), u' ');
            raw << k;
            allKeys << k;
        }
        if (raw.isEmpty()) continue;
        allKeys.removeAll(QStringLiteral("。"));
        allKeys.removeDuplicates();
        const auto info = Dict::instance().info(allKeys);
        QList<QList<ScoredToken>> groups;
        for (const QStringList &line : raw) {
            QList<ScoredToken> g;
            for (const QString &k : line) {
                if (k == u"。") g.append({k, {}, false});
                else g.append({k, k, true, words.status(k), info.value(k).rank, words.hasCard(k)});
            }
            groups << g;
        }
        const Analysis a = analyze(groups, known);
        db::exec(d, QStringLiteral("UPDATE items SET comprehension=?, words=? WHERE id=?"), {a.stats.comprehension, a.stats.words, id});
    }
    scoreSeries();
}

void scoreSeries()
{
    // A series scores as its episodes do, weighted by how many words each has.
    db::exec(db::app(), QStringLiteral("UPDATE series SET comprehension=(SELECT CAST(ROUND(SUM(comprehension*words)*1.0/SUM(words)) AS INTEGER)"
                                       " FROM items WHERE items.series=series.id AND comprehension IS NOT NULL AND words>0)"));
}

QList<LineHit> linesWithWord(const QString &key, int limit)
{
    QList<LineHit> out;
    auto [lemma, reading] = jp::splitKey(key);
    QString phrase = reading.isEmpty() ? lemma : lemma + u' ' + reading;
    phrase.replace(u'"', QString());
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT l.item, i.series, i.path, COALESCE(s.meta_title, s.title), i.title, l.text, l.start, l.end, i.episode, l.keys"
                             " FROM lines_fts f JOIN lines l ON l.id=f.rowid JOIN items i ON i.id=l.item LEFT JOIN series s ON s.id=i.series"
                             " WHERE lines_fts MATCH ? LIMIT ?"));
    q.addBindValue(u'"' + phrase + u'"');
    q.addBindValue(limit * 3);
    if (!db::exec(q)) return out;
    QSet<QString> seen;
    while (q.next() && out.size() < limit) {
        // The FTS phrase can match across word boundaries; keep lines that hold the exact key.
        const QString text = q.value(5).toString();
        const QStringList keys = q.value(9).toString().split(u' ');
        QString want = key;
        want.replace(u' ', QChar(0x3000));
        if (seen.contains(text) || !(keys.contains(want) || (reading.isEmpty() && std::any_of(keys.begin(), keys.end(), [&](const QString &k) { return jp::splitKey(k).first == lemma; }))))
            continue;
        seen.insert(text);
        out.append({q.value(0).toInt(), q.value(1).toInt(), q.value(2).toString(), q.value(3).toString(), q.value(4).toString(), text,
                    q.value(6).toDouble(), q.value(7).toDouble(), q.value(8).isNull() ? -1 : q.value(8).toDouble()});
    }
    return out;
}

} // namespace library
