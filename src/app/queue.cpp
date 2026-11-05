#include "queue.h"

#include "core/util/db.h"

#include <QFileInfo>
#include <QQmlEngine>

Queue *Queue::instance()
{
    static Queue *q = new Queue;
    return q;
}

Queue *Queue::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

namespace {

QVariantMap row(QSqlQuery &q)
{
    QString source = q.value(8).toString().isEmpty() ? QFileInfo(q.value(7).toString()).completeBaseName() : q.value(8).toString();
    if (!q.value(9).isNull()) source += QStringLiteral(" · %1").arg(q.value(9).toDouble());
    const qint64 t = qint64(q.value(2).toDouble());
    source += QStringLiteral(" · %1:%2").arg(t / 60).arg(t % 60, 2, 10, QChar(u'0'));
    return {{"id", q.value(0)}, {"item", q.value(1)}, {"start", q.value(2)}, {"end", q.value(3)}, {"text", q.value(4)},
            {"text2", q.value(5)}, {"word", q.value(6)}, {"path", q.value(7)}, {"source", source}};
}

const char *COLS = "q.id, q.item, q.start, q.end, q.text, q.text2, q.word, i.path, COALESCE(s.meta_title, s.title), i.episode";

} // namespace

QVariantList Queue::list() const
{
    QVariantList out;
    QSqlQuery q(db::app());
    q.exec(QStringLiteral("SELECT %1 FROM queue q LEFT JOIN items i ON i.id=q.item LEFT JOIN series s ON s.id=i.series ORDER BY q.time").arg(QLatin1String(COLS)));
    while (q.next()) out << row(q);
    return out;
}

void Queue::remove(int id)
{
    db::exec(db::app(), QStringLiteral("DELETE FROM queue WHERE id=?"), {id});
    emit changed();
}

void Queue::clear()
{
    db::exec(db::app(), QStringLiteral("DELETE FROM queue"));
    emit changed();
}

void Queue::open(int id)
{
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT %1 FROM queue q LEFT JOIN items i ON i.id=q.item LEFT JOIN series s ON s.id=i.series WHERE q.id=?").arg(QLatin1String(COLS)));
    q.addBindValue(id);
    if (db::exec(q) && q.next()) emit openRequested(row(q));
}
