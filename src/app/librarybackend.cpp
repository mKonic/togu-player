#include "librarybackend.h"

#include "backend.h"
#include "core/library/library.h"
#include "core/library/metadata.h"
#include "core/library/names.h"
#include "core/media/probe.h"
#include "core/util/db.h"
#include "core/words/words.h"
#include "settings.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include "core/media/cut.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QRegularExpression>
#include <QQmlEngine>
#include <QtConcurrent>

Library *Library::instance()
{
    static Library *l = new Library;
    return l;
}

Library *Library::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

Library::Library(QObject *parent) : QObject(parent)
{
    m_pool.setMaxThreadCount(1); // one pipeline at a time keeps SQLite writes simple
    m_watchDebounce.setSingleShot(true);
    m_watchDebounce.setInterval(3000);
    connect(&m_watchDebounce, &QTimer::timeout, this, [this] { runPipeline(); });
    connect(&m_watch, &QFileSystemWatcher::directoryChanged, this, [this] { m_watchDebounce.start(); });
    m_rescoreDebounce.setSingleShot(true);
    m_rescoreDebounce.setInterval(4000);
    connect(&m_rescoreDebounce, &QTimer::timeout, this, [this] {
        m_pool.start([this] {
            library::scoreItems();
            QMetaObject::invokeMethod(this, [this] { bump(); });
        });
    });
    connect(&Words::instance(), &Words::changed, this, [this] { m_rescoreDebounce.start(); });
    QTimer::singleShot(1500, this, [this] {
        updateWatches();
        runPipeline();
    });
}

void Library::bump()
{
    ++m_revision;
    emit changed();
}

void Library::setBusy(bool b, const QString &status)
{
    m_busy = b;
    m_status = status;
    emit busyChanged();
}

void Library::updateWatches()
{
    if (!m_watch.directories().isEmpty()) m_watch.removePaths(m_watch.directories());
    QStringList dirs;
    for (const auto &r : library::roots()) {
        if (r.kind != u"video" && r.kind != u"audio") continue;
        dirs << r.path;
        for (const QFileInfo &f : QDir(r.path).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) dirs << f.absoluteFilePath();
    }
    if (!dirs.isEmpty()) m_watch.addPaths(dirs);
}

QVariantList Library::roots(const QString &kind) const
{
    QVariantList out;
    for (const auto &r : library::roots(kind)) out << QVariantMap{{"id", r.id}, {"path", r.path}, {"kind", r.kind}};
    return out;
}

void Library::addRoot(const QUrl &folder, const QString &kind)
{
    const QString p = folder.isLocalFile() ? folder.toLocalFile() : folder.toString();
    const int id = library::addRoot(p, kind);
    updateWatches();
    bump();
    runPipeline(id);
}

void Library::removeRoot(int id)
{
    m_cancel = true;
    m_pool.waitForDone(5000);
    m_cancel = false;
    library::removeRoot(id);
    updateWatches();
    bump();
}

void Library::rescan() { runPipeline(); }

void Library::runPipeline(int rootId)
{
    if (m_busy) {
        m_pending = true;
        return;
    }
    setBusy(true, tr("Scanning folders…"));
    const bool meta = Settings::v("metadata").toBool();
    const QString tmdb = Settings::v("tmdbKey").toString();
    auto status = [this](const QString &s) { QMetaObject::invokeMethod(this, [this, s] { setBusy(true, s); }); };
    m_pool.start([this, rootId, meta, tmdb, status] {
        for (const auto &r : library::roots()) {
            if (m_cancel) break;
            if (rootId && r.id != rootId) continue;
            if (r.kind == u"video" || r.kind == u"audio") library::scan(r, &m_cancel);
            else library::scanReading(r, &m_cancel);
        }
        QMetaObject::invokeMethod(this, [this] { bump(); });
        if (meta) {
            QList<int> pending;
            QSqlQuery q(db::app());
            q.exec(QStringLiteral("SELECT id FROM series WHERE meta_id IS NULL AND kind IN ('video','audio')"));
            while (q.next()) pending << q.value(0).toInt();
            for (qsizetype i = 0; i < pending.size() && !m_cancel; ++i) {
                status(tr("Looking up titles… %1/%2").arg(i + 1).arg(pending.size()));
                metadata::autoMatch(pending[i], tmdb);
            }
            if (!pending.isEmpty()) QMetaObject::invokeMethod(this, [this] { bump(); });
        }
        const QList<int> toIndex = library::itemsToIndex();
        QList<int> indexed;
        for (qsizetype i = 0; i < toIndex.size() && !m_cancel; ++i) {
            status(tr("Reading subtitles… %1/%2").arg(i + 1).arg(toIndex.size()));
            if (library::indexItem(toIndex[i]) > 0) indexed << toIndex[i];
        }
        if (!indexed.isEmpty()) library::scoreItems(indexed);
        QMetaObject::invokeMethod(this, [this] {
            setBusy(false);
            bump();
            if (m_pending) {
                m_pending = false;
                runPipeline();
            }
        });
    });
}

namespace {

QString fileUrl(const QString &p)
{
    if (p.isEmpty()) return {};
    return p.contains(u"://") ? p : QUrl::fromLocalFile(p).toString();
}

QVariantMap seriesRow(QSqlQuery &q)
{
    // id, title, meta_title, poster, meta_poster, meta_banner, comprehension, kind, count, watched, last_played, meta_year, folder
    const QString title = q.value(2).toString().isEmpty() ? q.value(1).toString() : q.value(2).toString();
    const QString poster = q.value(4).toString().isEmpty() ? q.value(3).toString() : q.value(4).toString();
    return {{"id", q.value(0)}, {"title", title}, {"folderTitle", q.value(1)}, {"poster", fileUrl(poster)},
            {"banner", fileUrl(q.value(5).toString())}, {"comprehension", q.value(6).isNull() ? -1 : q.value(6).toInt()},
            {"kind", q.value(7)}, {"count", q.value(8)}, {"watched", q.value(9)}, {"lastPlayed", q.value(10)},
            {"year", q.value(11)}, {"folder", q.value(12)}};
}

const char *SERIES_COLS =
    "s.id, s.title, s.meta_title, s.poster, s.meta_poster, s.meta_banner, s.comprehension, s.kind,"
    " (SELECT COUNT(*) FROM items i WHERE i.series=s.id), (SELECT COUNT(*) FROM items i WHERE i.series=s.id AND i.watched=1),"
    " (SELECT MAX(last_played) FROM items i WHERE i.series=s.id), s.meta_year, s.folder";

QVariantMap itemRow(QSqlQuery &q)
{
    // id, path, title, season, episode, special, duration, position, watched, comprehension, words, last_played, series, sub_path
    return {{"id", q.value(0)}, {"path", q.value(1)}, {"title", q.value(2)}, {"season", q.value(3)},
            {"episode", q.value(4).isNull() ? -1 : q.value(4).toDouble()}, {"special", q.value(5).toBool()},
            {"duration", q.value(6).toDouble()}, {"position", q.value(7).toDouble()}, {"watched", q.value(8).toBool()},
            {"comprehension", q.value(9).isNull() ? -1 : q.value(9).toInt()}, {"words", q.value(10).toInt()},
            {"lastPlayed", q.value(11)}, {"series", q.value(12)}, {"hasSubs", !q.value(13).toString().isEmpty()},
            {"name", QFileInfo(q.value(1).toString()).completeBaseName()}};
}

const char *ITEM_COLS = "i.id, i.path, i.title, i.season, i.episode, i.special, i.duration, i.position, i.watched, i.comprehension,"
                        " i.words, i.last_played, i.series, COALESCE(NULLIF(i.indexed_sub,''), i.sub_path)";

} // namespace

QVariantList Library::series(const QString &kind, const QString &sort, const QString &filter) const
{
    QString order;
    if (sort == u"title") order = QStringLiteral("COALESCE(s.meta_title, s.title) COLLATE NOCASE");
    else if (sort == u"comprehension") order = QStringLiteral("COALESCE(s.comprehension, -1) DESC, COALESCE(s.meta_title, s.title) COLLATE NOCASE");
    else if (sort == u"added") order = QStringLiteral("s.id DESC");
    else order = QStringLiteral("COALESCE((SELECT MAX(last_played) FROM items i WHERE i.series=s.id), 0) DESC, s.id DESC");
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT %1 FROM series s WHERE s.kind=? AND (?='' OR COALESCE(s.meta_title,'') LIKE ? OR s.title LIKE ?) ORDER BY %2")
                  .arg(QLatin1String(SERIES_COLS), order));
    const QString like = u'%' + filter + u'%';
    q.addBindValue(kind);
    q.addBindValue(filter);
    q.addBindValue(like);
    q.addBindValue(like);
    db::exec(q);
    QVariantList out;
    while (q.next()) out << seriesRow(q);
    return out;
}

QVariantMap Library::seriesInfo(int id) const
{
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT %1, s.meta_desc, s.meta_id, s.meta_manual FROM series s WHERE s.id=?").arg(QLatin1String(SERIES_COLS)));
    q.addBindValue(id);
    if (!db::exec(q) || !q.next()) return {};
    QVariantMap m = seriesRow(q);
    m.insert("description", q.value(13));
    m.insert("metaId", q.value(14));
    m.insert("metaManual", q.value(15).toBool());
    return m;
}

QVariantList Library::items(int seriesId) const
{
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT %1 FROM items i WHERE i.series=? ORDER BY i.special, COALESCE(i.season, 0), COALESCE(i.episode, 1e9), i.path COLLATE NOCASE")
                  .arg(QLatin1String(ITEM_COLS)));
    q.addBindValue(seriesId);
    db::exec(q);
    // Subfolders of the series folder are groups (Season 1, Specials, ...), named after the folder.
    QSqlQuery r(db::app());
    r.prepare(QStringLiteral("SELECT r.path, s.folder FROM series s JOIN roots r ON r.id=s.root WHERE s.id=?"));
    r.addBindValue(seriesId);
    QString base;
    if (db::exec(r) && r.next() && !r.value(1).toString().isEmpty())
        base = QDir::cleanPath(r.value(0).toString() + u'/' + r.value(1).toString()) + u'/';
    QList<QVariantMap> loose;
    QMap<QString, QList<QVariantMap>> groups;
    while (q.next()) {
        QVariantMap m = itemRow(q);
        const QString path = m.value("path").toString();
        QString group;
        if (!base.isEmpty() && path.startsWith(base)) {
            const QString rel = path.mid(base.size());
            if (rel.contains(u'/')) group = rel.section(u'/', 0, -2).replace(u'/', QStringLiteral(" / "));
        }
        m.insert("group", group);
        if (group.isEmpty()) loose << m;
        else groups[group] << m;
    }
    // Loose files first, then groups in natural order (Season 2 before Season 10).
    QStringList names = groups.keys();
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(names.begin(), names.end(), [&](const QString &a, const QString &b) { return collator.compare(a, b) < 0; });
    QVariantList out;
    for (const QVariantMap &m : loose) out << m;
    for (const QString &n : names)
        for (const QVariantMap &m : groups[n]) out << m;
    return out;
}

QVariantList Library::continueItems(const QString &kind, int limit) const
{
    // The episode in progress per series, or the next one after the last watched.
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT s.id FROM series s WHERE s.kind=? AND EXISTS(SELECT 1 FROM items i WHERE i.series=s.id AND i.last_played IS NOT NULL)"
                             " ORDER BY (SELECT MAX(last_played) FROM items i WHERE i.series=s.id) DESC LIMIT ?"));
    q.addBindValue(kind);
    q.addBindValue(limit);
    db::exec(q);
    QVariantList out;
    while (q.next()) {
        const int sid = q.value(0).toInt();
        const QVariantList eps = items(sid);
        QVariantMap pick;
        int lastIdx = -1;
        qlonglong lastTime = 0;
        for (int i = 0; i < eps.size(); ++i) {
            const QVariantMap e = eps[i].toMap();
            if (e.value("lastPlayed").toLongLong() > lastTime) {
                lastTime = e.value("lastPlayed").toLongLong();
                lastIdx = i;
            }
        }
        if (lastIdx < 0) continue;
        QVariantMap last = eps[lastIdx].toMap();
        if (!last.value("watched").toBool() && last.value("position").toDouble() > 5) pick = last;
        else
            for (int i = lastIdx + 1; i < eps.size(); ++i)
                if (!eps[i].toMap().value("watched").toBool()) {
                    pick = eps[i].toMap();
                    break;
                }
        if (pick.isEmpty()) continue;
        const QVariantMap s = seriesInfo(sid);
        pick.insert("seriesTitle", s.value("title"));
        pick.insert("poster", s.value("poster"));
        pick.insert("banner", s.value("banner"));
        out << pick;
    }
    return out;
}

QVariantMap Library::item(int id) const
{
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT %1 FROM items i WHERE i.id=?").arg(QLatin1String(ITEM_COLS)));
    q.addBindValue(id);
    if (db::exec(q) && q.next()) return itemRow(q);
    return {};
}

int Library::nextItem(int itemId) const
{
    const int sid = db::scalar(db::app(), QStringLiteral("SELECT series FROM items WHERE id=?"), {itemId}).toInt();
    if (!sid) return 0;
    const QVariantList eps = items(sid);
    for (int i = 0; i + 1 < eps.size(); ++i)
        if (eps[i].toMap().value("id").toInt() == itemId) return eps[i + 1].toMap().value("id").toInt();
    return 0;
}

void Library::setWatched(int itemId, bool watched)
{
    db::exec(db::app(), QStringLiteral("UPDATE items SET watched=?, position=0 WHERE id=?"), {watched ? 1 : 0, itemId});
    bump();
}

void Library::exportCondensed(int seriesId, const QUrl &folder)
{
    const QString dir = folder.toLocalFile();
    const QString format = Settings::v("audioFormat").toString() == u"opus" ? QStringLiteral("opus") : QStringLiteral("mp3");
    QThreadPool::globalInstance()->start([this, seriesId, dir, format] {
        struct Ep {
            int id;
            QString path, title;
            double episode, offset;
            int track;
        };
        QList<Ep> eps;
        QString series;
        {
            QSqlQuery q(db::app());
            q.prepare(QStringLiteral("SELECT i.id, i.path, i.title, COALESCE(i.episode,-1), COALESCE(i.sub_offset,0), COALESCE(i.audio_track,-1),"
                                     " COALESCE(s.meta_title, s.title) FROM items i JOIN series s ON s.id=i.series WHERE i.series=?"
                                     " ORDER BY i.season, i.episode, i.path"));
            q.addBindValue(seriesId);
            db::exec(q);
            while (q.next()) {
                eps.append({q.value(0).toInt(), q.value(1).toString(), q.value(2).toString(), q.value(3).toDouble(), q.value(4).toDouble(), q.value(5).toInt()});
                series = q.value(6).toString();
            }
        }
        static const QRegularExpression bad(QStringLiteral("[/\\\\:*?\"<>|]"));
        series.replace(bad, QStringLiteral("_"));
        int written = 0, skipped = 0, done = 0;
        for (const Ep &e : eps) {
            QList<std::pair<double, double>> spans;
            QSqlQuery q(db::app());
            q.prepare(QStringLiteral("SELECT start, end FROM lines WHERE item=? ORDER BY start"));
            q.addBindValue(e.id);
            db::exec(q);
            while (q.next()) spans.append({q.value(0).toDouble() + e.offset, q.value(1).toDouble() + e.offset});
            QString name = e.episode >= 0 ? QStringLiteral("%1 - %2").arg(series).arg(e.episode, 2, 'f', e.episode == int(e.episode) ? 0 : 1, QChar(u'0'))
                                          : QFileInfo(e.path).completeBaseName();
            name.replace(bad, QStringLiteral("_"));
            const QString out = dir + u'/' + name + QStringLiteral(" (condensed).") + format;
            if (spans.isEmpty() || media::condensedAudio(e.path, spans, e.track, format, out).isEmpty()) ++skipped;
            else ++written;
            QMetaObject::invokeMethod(this, [this, d = ++done, n = int(eps.size())] { emit condensedProgress(d, n); });
        }
        QMetaObject::invokeMethod(this, [this, dir, written, skipped] { emit condensedDone(dir, written, skipped); });
    });
}

void Library::setSeriesWatched(int seriesId, bool watched)
{
    db::exec(db::app(), QStringLiteral("UPDATE items SET watched=?, position=0 WHERE series=?"), {watched ? 1 : 0, seriesId});
    bump();
}

QVariantList Library::linesWithWord(const QString &key, int limit) const
{
    QVariantList out;
    for (const auto &h : library::linesWithWord(key, limit))
        out << QVariantMap{{"item", h.item}, {"series", h.series}, {"path", h.path}, {"seriesTitle", h.seriesTitle},
                           {"itemTitle", h.itemTitle}, {"text", h.text}, {"start", h.start}, {"end", h.end}, {"episode", h.episode}};
    return out;
}

QVariantList Library::shelf(const QString &kind, const QString &filter) const
{
    QVariantList out;
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT i.id, i.path, i.title, i.extra, s.id, s.title, p.pos, p.time FROM items i JOIN series s ON s.id=i.series"
                             " LEFT JOIN positions p ON p.path=i.path WHERE i.kind=? AND (?='' OR i.title LIKE ? OR s.title LIKE ?)"
                             " ORDER BY s.title COLLATE NOCASE, i.title COLLATE NOCASE"));
    const QString like = u'%' + filter + u'%';
    q.addBindValue(kind);
    q.addBindValue(filter);
    q.addBindValue(like);
    q.addBindValue(like);
    db::exec(q);
    while (q.next()) {
        const QJsonObject extra = QJsonDocument::fromJson(q.value(3).toByteArray()).object();
        const QJsonObject pos = QJsonDocument::fromJson(q.value(6).toByteArray()).object();
        double progress = 0;
        if (pos.contains("page")) progress = (pos.value("page").toDouble() + 1) / std::max(1, pos.value("pages").toInt(1));
        else if (pos.contains("chapter"))
            progress = (pos.value("chapter").toDouble() + pos.value("fraction").toDouble()) / std::max(1, pos.value("chapters").toInt(1));
        out << QVariantMap{{"id", q.value(0)}, {"path", q.value(1)}, {"title", q.value(2)}, {"author", extra.value("author").toString()},
                           {"cover", fileUrl(extra.value("cover").toString())}, {"series", q.value(4)}, {"seriesTitle", q.value(5)},
                           {"progress", std::min(1.0, progress)}, {"lastRead", q.value(7)}};
    }
    return out;
}

void Library::searchMeta(int seriesId, const QString &query)
{
    const QString tmdb = Settings::v("tmdbKey").toString();
    (void)QtConcurrent::run([this, seriesId, query, tmdb] {
        QVariantList out;
        for (const metadata::Candidate &c : metadata::search(query, tmdb))
            out << QVariantMap{{"source", c.source}, {"id", c.id}, {"title", c.title}, {"titleNative", c.titleNative},
                               {"titleEnglish", c.titleEnglish}, {"poster", c.poster}, {"banner", c.banner},
                               {"description", c.description}, {"year", c.year}};
        QMetaObject::invokeMethod(this, [this, seriesId, out] { emit metaResults(seriesId, out); });
    });
}

void Library::applyMeta(int seriesId, const QVariantMap &m)
{
    metadata::Candidate c;
    c.source = m.value("source").toString();
    c.id = m.value("id").toString();
    c.title = m.value("title").toString();
    c.titleNative = m.value("titleNative").toString();
    c.titleEnglish = m.value("titleEnglish").toString();
    c.poster = m.value("poster").toString();
    c.banner = m.value("banner").toString();
    c.description = m.value("description").toString();
    c.year = m.value("year").toInt();
    (void)QtConcurrent::run([this, seriesId, c] {
        metadata::apply(seriesId, c, true);
        QMetaObject::invokeMethod(this, [this] { bump(); });
    });
}

void Library::clearMeta(int seriesId)
{
    db::exec(db::app(), QStringLiteral("UPDATE series SET meta_id='none', meta_title=NULL, meta_poster=NULL, meta_banner=NULL, meta_desc=NULL,"
                                       " meta_year=NULL, meta_manual=1 WHERE id=?"), {seriesId});
    bump();
}

void Library::refreshMeta(int seriesId)
{
    db::exec(db::app(), QStringLiteral("UPDATE series SET meta_id=NULL, meta_manual=0 WHERE id=?"), {seriesId});
    runPipeline();
}

// ---------------------------------------------------------------- thumbnails

QImage ThumbProvider::requestImage(const QString &id, QSize *size, const QSize &requested)
{
    const int itemId = id.toInt();
    const QString dir = db::cacheDir() + QStringLiteral("/thumbs");
    QDir().mkpath(dir);
    const QString out = dir + QStringLiteral("/%1.jpg").arg(itemId);
    if (!QFileInfo::exists(out)) {
        QSqlQuery q(db::app());
        q.prepare(QStringLiteral("SELECT path, duration, kind FROM items WHERE id=?"));
        q.addBindValue(itemId);
        if (db::exec(q) && q.next() && q.value(2).toString() == u"audio") {
            const QString cover = media::coverArt(q.value(0).toString());
            if (!cover.isEmpty()) QFile::copy(cover, out);
        } else if (q.isValid() && q.value(2).toString() == u"video") {
            const QString path = q.value(0).toString();
            double dur = q.value(1).toDouble();
            if (dur <= 0) dur = media::probe(path).duration;
            QProcess p;
            p.start(media::ffmpeg(), {"-v", "error", "-y", "-ss", QString::number(std::max(1.0, dur * 0.25)), "-i", path,
                                      "-frames:v", "1", "-vf", "scale=480:-2", "-q:v", "4", out});
            p.waitForFinished(20000);
        }
    }
    QImage img(out);
    if (requested.width() > 0 && !img.isNull()) img = img.scaledToWidth(requested.width(), Qt::SmoothTransformation);
    if (size) *size = img.size();
    return img;
}
