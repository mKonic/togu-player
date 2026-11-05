#pragma once
// Immersion time and word-count history for the Stats page.

#include <QObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class Stats : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(int revision READ revision NOTIFY changed)
public:
    static Stats *instance();
    static Stats *create(QQmlEngine *, QJSEngine *);
    int revision() const { return m_rev; }

    // Seconds per day for the last `days` days (oldest first), split by kind.
    Q_INVOKABLE QVariantList daily(int days) const;
    Q_INVOKABLE QVariantList topSeries(int days, int limit = 8) const;
    Q_INVOKABLE QVariantMap totals() const;
    Q_INVOKABLE QVariantList knownHistory(int days) const;
    // Reading and manga time, credited while the page is in use.
    Q_INVOKABLE void addTime(const QString &kind, const QString &path, double seconds);
    void snapshotWords();

signals:
    void changed();

private:
    explicit Stats(QObject *parent = nullptr);
    int m_rev = 0;
};
