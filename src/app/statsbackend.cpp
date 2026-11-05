#include "statsbackend.h"

#include "core/util/db.h"
#include "core/words/words.h"
#include "settings.h"

#include <QDate>
#include <QQmlEngine>
#include <QTimer>

Stats *Stats::instance()
{
    static Stats *s = new Stats;
    return s;
}

Stats *Stats::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

Stats::Stats(QObject *parent) : QObject(parent)
{
    snapshotWords();
    auto *t = new QTimer(this);
    t->setSingleShot(true);
    t->setInterval(5000);
    connect(t, &QTimer::timeout, this, [this] { snapshotWords(); });
    connect(&Words::instance(), &Words::changed, t, qOverload<>(&QTimer::start));
}

void Stats::snapshotWords()
{
    const auto c = Words::instance().counts();
    db::exec(db::app(), QStringLiteral("INSERT OR REPLACE INTO known_history (day, known, learning) VALUES (?,?,?)"),
             {QDate::currentDate().toString(Qt::ISODate), c.value(Status::Known), c.value(Status::Learning)});
    ++m_rev;
    emit changed();
}

QVariantList Stats::daily(int days) const
{
    const QDate today = QDate::currentDate();
    QHash<QString, QVariantMap> byDay;
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT day, kind, SUM(seconds) FROM immersion WHERE day>=? GROUP BY day, kind"));
    q.addBindValue(today.addDays(-days + 1).toString(Qt::ISODate));
    db::exec(q);
    while (q.next()) {
        QVariantMap &m = byDay[q.value(0).toString()];
        m.insert(q.value(1).toString(), q.value(2).toDouble());
    }
    QVariantList out;
    for (int i = days - 1; i >= 0; --i) {
        const QDate d = today.addDays(-i);
        const QVariantMap kinds = byDay.value(d.toString(Qt::ISODate));
        double total = 0;
        for (const QVariant &v : kinds) total += v.toDouble();
        out << QVariantMap{{"day", d.toString(Qt::ISODate)}, {"label", d.toString(QStringLiteral("d MMM"))}, {"weekday", d.dayOfWeek()},
                           {"seconds", total}, {"kinds", kinds}};
    }
    return out;
}

QVariantList Stats::topSeries(int days, int limit) const
{
    QVariantList out;
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT COALESCE(s.meta_title, s.title, i.title), im.kind, SUM(im.seconds) AS secs FROM immersion im"
                             " LEFT JOIN items i ON i.id=im.item LEFT JOIN series s ON s.id=COALESCE(im.series, i.series)"
                             " WHERE im.day>=? GROUP BY COALESCE(s.id, -i.id) ORDER BY secs DESC LIMIT ?"));
    q.addBindValue(QDate::currentDate().addDays(-days + 1).toString(Qt::ISODate));
    q.addBindValue(limit);
    db::exec(q);
    while (q.next()) out << QVariantMap{{"title", q.value(0)}, {"kind", q.value(1)}, {"seconds", q.value(2).toDouble()}};
    return out;
}

QVariantMap Stats::totals() const
{
    const QDate today = QDate::currentDate();
    auto sum = [](const QString &where, const QVariantList &args) {
        return db::scalar(db::app(), QStringLiteral("SELECT COALESCE(SUM(seconds),0) FROM immersion WHERE ") + where, args).toDouble();
    };
    // Streak: days in a row with at least a minute, counting today only once something happened.
    int streak = 0;
    QSet<QString> active;
    QSqlQuery q(db::app());
    q.exec(QStringLiteral("SELECT day FROM immersion GROUP BY day HAVING SUM(seconds) >= 60"));
    while (q.next()) active.insert(q.value(0).toString());
    QDate d = active.contains(today.toString(Qt::ISODate)) ? today : today.addDays(-1);
    while (active.contains(d.toString(Qt::ISODate))) {
        ++streak;
        d = d.addDays(-1);
    }
    const auto c = Words::instance().counts();
    return {{"today", sum(QStringLiteral("day=?"), {today.toString(Qt::ISODate)})},
            {"week", sum(QStringLiteral("day>=?"), {today.addDays(-6).toString(Qt::ISODate)})},
            {"all", sum(QStringLiteral("1=1"), {})},
            {"streak", streak},
            {"goal", Settings::v("dailyGoalMinutes").toInt() * 60},
            {"known", c.value(Status::Known)},
            {"learning", c.value(Status::Learning)},
            {"cards", db::scalar(db::app(), QStringLiteral("SELECT COUNT(*) FROM cards")).toInt()},
            {"cardsWeek", db::scalar(db::app(), QStringLiteral("SELECT COUNT(*) FROM cards WHERE time>=?"),
                                     {QDateTime(today.addDays(-6), QTime()).toSecsSinceEpoch()}).toInt()}};
}

QVariantList Stats::knownHistory(int days) const
{
    QVariantList out;
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT day, known, learning FROM known_history WHERE day>=? ORDER BY day"));
    q.addBindValue(QDate::currentDate().addDays(-days + 1).toString(Qt::ISODate));
    db::exec(q);
    while (q.next())
        out << QVariantMap{{"day", q.value(0)}, {"label", QDate::fromString(q.value(0).toString(), Qt::ISODate).toString(QStringLiteral("d MMM"))},
                           {"known", q.value(1)}, {"learning", q.value(2)}};
    return out;
}

void Stats::addTime(const QString &kind, const QString &path, double seconds)
{
    if (seconds <= 0 || seconds > 600) return;
    const QVariant item = db::scalar(db::app(), QStringLiteral("SELECT id FROM items WHERE path=?"), {path});
    db::exec(db::app(), QStringLiteral("INSERT INTO immersion (day, item, series, kind, seconds) VALUES (?, ?, (SELECT series FROM items WHERE id=?), ?, ?)"
                                       " ON CONFLICT(day, item, kind) DO UPDATE SET seconds=seconds+excluded.seconds"),
             {QDate::currentDate().toString(Qt::ISODate), item.isValid() ? item : QVariant(0), item, kind, seconds});
    ++m_rev;
    emit changed();
}
