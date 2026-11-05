#include "db.h"

#include <QCoreApplication>
#include <QDir>
#include <QSqlError>
#include <QStandardPaths>
#include <QThread>
#include <QDebug>

namespace db {

namespace {

QString g_dataDir;

QSqlDatabase open(const QString &kind, const QString &file)
{
    const QString name = kind + QString::number(quintptr(QThread::currentThreadId()));
    if (QSqlDatabase::contains(name)) return QSqlDatabase::database(name);
    QSqlDatabase d = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    d.setDatabaseName(dataDir() + u'/' + file);
    d.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=10000"));
    if (!d.open()) qWarning() << "db: cannot open" << file << d.lastError().text();
    QSqlQuery q(d);
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    q.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    q.exec(QStringLiteral("PRAGMA foreign_keys=ON"));
    // Connections die with their thread; drop the name so it can't be reused stale.
    if (QThread *t = QThread::currentThread(); QCoreApplication::instance() && t != QCoreApplication::instance()->thread())
        QObject::connect(t, &QThread::finished, [name] { QSqlDatabase::removeDatabase(name); });
    return d;
}

const char *APP_SCHEMA[] = {
    // Word statuses. s: 1 tracked, 2 learning, 3 known, 4 ignored (unknown is no row).
    // manual: set by hand; anki_mod: the Anki card's modification time when the hand
    // setting was made, so a later review hands the word back to Anki.
    "CREATE TABLE IF NOT EXISTS words (w TEXT PRIMARY KEY, s INTEGER NOT NULL, t INTEGER NOT NULL,"
    " c INTEGER NOT NULL, manual INTEGER NOT NULL DEFAULT 1, anki_mod INTEGER)",
    "CREATE TABLE IF NOT EXISTS settings (k TEXT PRIMARY KEY, v TEXT)",
    // Library.
    "CREATE TABLE IF NOT EXISTS roots (id INTEGER PRIMARY KEY, path TEXT UNIQUE NOT NULL, kind TEXT NOT NULL)",
    "CREATE TABLE IF NOT EXISTS series (id INTEGER PRIMARY KEY, root INTEGER NOT NULL REFERENCES roots(id) ON DELETE CASCADE,"
    " kind TEXT NOT NULL, title TEXT NOT NULL, folder TEXT NOT NULL, poster TEXT, meta_id TEXT, meta_title TEXT,"
    " meta_poster TEXT, meta_banner TEXT, meta_desc TEXT, meta_year INTEGER, meta_manual INTEGER DEFAULT 0,"
    " comprehension INTEGER, UNIQUE(root, folder, title))",
    "CREATE TABLE IF NOT EXISTS items (id INTEGER PRIMARY KEY, series INTEGER REFERENCES series(id) ON DELETE CASCADE,"
    " path TEXT UNIQUE NOT NULL, kind TEXT NOT NULL, title TEXT, season INTEGER, episode REAL, special INTEGER DEFAULT 0,"
    " size INTEGER, mtime INTEGER, duration REAL, position REAL DEFAULT 0, watched INTEGER DEFAULT 0, last_played INTEGER,"
    " sub_path TEXT, sub_track INTEGER, sub2_path TEXT, sub2_track INTEGER, audio_track INTEGER,"
    " sub_offset REAL DEFAULT 0, sub_pinned INTEGER DEFAULT 0, sub_synced INTEGER DEFAULT 0, sub2_offset REAL DEFAULT 0,"
    " comprehension INTEGER, words INTEGER, indexed_sub TEXT, extra TEXT)",
    "CREATE INDEX IF NOT EXISTS items_series ON items(series)",
    // Subtitle lines of the library, for search and scoring.
    "CREATE TABLE IF NOT EXISTS lines (id INTEGER PRIMARY KEY, item INTEGER NOT NULL REFERENCES items(id) ON DELETE CASCADE,"
    " start REAL, end REAL, text TEXT, keys TEXT)",
    "CREATE INDEX IF NOT EXISTS lines_item ON lines(item)",
    "CREATE VIRTUAL TABLE IF NOT EXISTS lines_fts USING fts5(keys, content='lines', content_rowid='id', tokenize='unicode61')",
    "CREATE TRIGGER IF NOT EXISTS lines_ai AFTER INSERT ON lines BEGIN INSERT INTO lines_fts(rowid, keys) VALUES (new.id, new.keys); END",
    "CREATE TRIGGER IF NOT EXISTS lines_ad AFTER DELETE ON lines BEGIN INSERT INTO lines_fts(lines_fts, rowid, keys) VALUES('delete', old.id, old.keys); END",
    // Cards sent to Anki and lines marked for later.
    "CREATE TABLE IF NOT EXISTS cards (id INTEGER PRIMARY KEY, note INTEGER, expression TEXT, sentence TEXT, item INTEGER, time INTEGER)",
    "CREATE TABLE IF NOT EXISTS queue (id INTEGER PRIMARY KEY, item INTEGER, start REAL, end REAL, text TEXT, text2 TEXT,"
    " word TEXT, time INTEGER)",
    "CREATE TABLE IF NOT EXISTS bookmarks (id INTEGER PRIMARY KEY, item INTEGER, start REAL, end REAL, text TEXT, time INTEGER)",
    // Immersion time: one row per (day, item, kind) with seconds.
    "CREATE TABLE IF NOT EXISTS immersion (day TEXT NOT NULL, item INTEGER, series INTEGER, kind TEXT NOT NULL,"
    " seconds REAL NOT NULL DEFAULT 0, PRIMARY KEY(day, item, kind))",
    // Daily snapshot of known-word counts for the progress chart.
    "CREATE TABLE IF NOT EXISTS known_history (day TEXT PRIMARY KEY, known INTEGER, learning INTEGER)",
    // Anki card state cache for status sync.
    "CREATE TABLE IF NOT EXISTS anki_cards (card INTEGER PRIMARY KEY, note INTEGER, word TEXT, mod INTEGER,"
    " status INTEGER, strength REAL, lapses INTEGER, type TEXT, queue TEXT)",
    "CREATE INDEX IF NOT EXISTS anki_cards_word ON anki_cards(word)",
    // Reader / manga positions.
    "CREATE TABLE IF NOT EXISTS positions (path TEXT PRIMARY KEY, pos TEXT, time INTEGER)",
};

const char *DICT_SCHEMA[] = {
    "CREATE TABLE IF NOT EXISTS dicts (id INTEGER PRIMARY KEY, title TEXT UNIQUE NOT NULL, revision TEXT, ord INTEGER,"
    " enabled INTEGER DEFAULT 1, terms INTEGER DEFAULT 0, metas INTEGER DEFAULT 0, kind TEXT, styles TEXT, source TEXT)",
    "CREATE TABLE IF NOT EXISTS terms (d INTEGER NOT NULL, e TEXT NOT NULL, r TEXT, t TEXT, ru TEXT, s REAL, g TEXT, q INTEGER, tt TEXT)",
    "CREATE INDEX IF NOT EXISTS terms_e ON terms(e)",
    "CREATE INDEX IF NOT EXISTS terms_r ON terms(r)",
    "CREATE TABLE IF NOT EXISTS meta (d INTEGER NOT NULL, e TEXT NOT NULL, m TEXT, v TEXT)",
    "CREATE INDEX IF NOT EXISTS meta_e ON meta(e)",
    "CREATE TABLE IF NOT EXISTS tags (d INTEGER NOT NULL, name TEXT, category TEXT, ord INTEGER, notes TEXT, score REAL)",
    "CREATE TABLE IF NOT EXISTS kanji (d INTEGER NOT NULL, c TEXT NOT NULL, onyomi TEXT, kunyomi TEXT, tags TEXT, meanings TEXT, stats TEXT)",
    "CREATE INDEX IF NOT EXISTS kanji_c ON kanji(c)",
    "CREATE TABLE IF NOT EXISTS media (d INTEGER NOT NULL, path TEXT NOT NULL, data BLOB, PRIMARY KEY(d, path))",
};

} // namespace

QString dataDir()
{
    if (g_dataDir.isEmpty()) {
        g_dataDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/togu-player");
        QDir().mkpath(g_dataDir);
    }
    return g_dataDir;
}

QString cacheDir()
{
    QString d = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/togu-player");
    if (!g_dataDir.isEmpty() && !g_dataDir.contains(QStringLiteral("/togu-player"))) d = g_dataDir + QStringLiteral("/cache");
    QDir().mkpath(d);
    return d;
}

void setDataDir(const QString &dir)
{
    g_dataDir = dir;
    QDir().mkpath(dir);
}

QSqlDatabase app() { return open(QStringLiteral("app"), QStringLiteral("togu.db")); }
QSqlDatabase dict() { return open(QStringLiteral("dict"), QStringLiteral("dict.db")); }

bool init()
{
    bool ok = true;
    for (const char *s : APP_SCHEMA) ok &= exec(app(), QString::fromUtf8(s));
    for (const char *s : DICT_SCHEMA) ok &= exec(dict(), QString::fromUtf8(s));
    return ok;
}

bool exec(QSqlQuery &q)
{
    if (q.exec()) return true;
    qWarning().noquote() << "db:" << q.lastError().text() << "in" << q.lastQuery().left(200);
    return false;
}

bool exec(QSqlDatabase d, const QString &sql, const QVariantList &args)
{
    QSqlQuery q(d);
    if (!q.prepare(sql)) {
        qWarning().noquote() << "db: prepare" << q.lastError().text() << "in" << sql.left(200);
        return false;
    }
    for (const QVariant &a : args) q.addBindValue(a);
    return exec(q);
}

QVariant scalar(QSqlDatabase d, const QString &sql, const QVariantList &args)
{
    QSqlQuery q(d);
    q.prepare(sql);
    for (const QVariant &a : args) q.addBindValue(a);
    if (exec(q) && q.next()) return q.value(0);
    return {};
}

} // namespace db
