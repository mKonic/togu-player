#pragma once
// SQLite connections. Qt needs one connection per thread, so these hand out a
// connection owned by the calling thread, opening it on first use.

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

namespace db {

// Folder for databases, thumbnails and cut media (~/.local/share/togu-player).
QString dataDir();
QString cacheDir();
void setDataDir(const QString &dir); // tests

QSqlDatabase app();  // library, words, settings, history
QSqlDatabase dict(); // dictionaries

// Runs the schema for both databases. Call once from the main thread at start.
bool init();

// Prepares and runs; logs and returns false on error.
bool exec(QSqlQuery &q);
bool exec(QSqlDatabase d, const QString &sql, const QVariantList &args = {});
QVariant scalar(QSqlDatabase d, const QString &sql, const QVariantList &args = {});

struct Tx {
    explicit Tx(QSqlDatabase d) : d(d) { d.transaction(); }
    ~Tx() { if (!done) d.rollback(); }
    bool commit() { done = true; return d.commit(); }
    QSqlDatabase d;
    bool done = false;
};

} // namespace db
