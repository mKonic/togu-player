#include "wordaudio.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QThread>

namespace wordaudio {

QString collectionDir(const QString &setting)
{
    if (!setting.isEmpty() && QFileInfo::exists(setting + QStringLiteral("/entries.db"))) return setting;
    const QString base = QDir::homePath() + QStringLiteral("/.local/share/Anki2/addons21");
    for (const QFileInfo &f : QDir(base).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString p = f.absoluteFilePath() + QStringLiteral("/user_files");
        if (QFileInfo::exists(p + QStringLiteral("/entries.db")) && QFileInfo::exists(p + QStringLiteral("/jpod_files"))) return p;
    }
    return {};
}

QList<Clip> find(const QString &dir, const QString &expression, const QString &reading)
{
    QList<Clip> out;
    if (dir.isEmpty()) return out;
    const QString name = QStringLiteral("wordaudio") + QString::number(quintptr(QThread::currentThreadId()));
    QSqlDatabase d = QSqlDatabase::contains(name) ? QSqlDatabase::database(name) : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    if (d.databaseName() != dir + QStringLiteral("/entries.db")) {
        d.close();
        d.setDatabaseName(dir + QStringLiteral("/entries.db"));
        d.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    }
    if (!d.isOpen() && !d.open()) return out;
    QSqlQuery q(d);
    q.prepare(QStringLiteral("SELECT source, speaker, file, reading FROM entries WHERE expression=? AND (reading IS NULL OR reading=? OR ?='')"));
    q.addBindValue(expression);
    q.addBindValue(reading);
    q.addBindValue(reading);
    q.exec();
    // Studio recordings first, then pitch-accent dictionaries, then Forvo.
    static const QStringList order = {"jpod", "nhk16", "shinmeikai8", "forvo"};
    while (q.next()) {
        const QString source = q.value(0).toString();
        const QString path = dir + u'/' + source + QStringLiteral("_files/") + q.value(2).toString();
        if (QFileInfo::exists(path)) out.append({path, source, q.value(1).toString()});
    }
    std::stable_sort(out.begin(), out.end(), [](const Clip &a, const Clip &b) {
        const auto ia = order.indexOf(a.source), ib = order.indexOf(b.source);
        return (ia < 0 ? 99 : ia) < (ib < 0 ? 99 : ib);
    });
    return out;
}

} // namespace wordaudio
