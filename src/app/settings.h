#pragma once
// App settings as a QML property map, stored in the settings table.

#include <QQmlPropertyMap>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class Settings : public QQmlPropertyMap {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    static Settings *instance();
    static Settings *create(QQmlEngine *, QJSEngine *);

    Q_INVOKABLE QVariant get(const QString &key) const { return value(key); }
    Q_INVOKABLE void set(const QString &key, const QVariant &v);
    Q_INVOKABLE void reset(const QString &key);
    Q_INVOKABLE QVariant defaultValue(const QString &key) const;

    // C++ side helpers.
    static QVariant v(const QString &key) { return instance()->value(key); }

protected:
    QVariant updateValue(const QString &key, const QVariant &input) override;

private:
    explicit Settings(QObject *parent = nullptr);
    void store(const QString &key, const QVariant &v);
};
