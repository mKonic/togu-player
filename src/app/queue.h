#pragma once
// Lines put aside while watching, to turn into cards later.

#include <QObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class Queue : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    static Queue *instance();
    static Queue *create(QQmlEngine *, QJSEngine *);
    Q_INVOKABLE QVariantList list() const;
    Q_INVOKABLE void remove(int id);
    Q_INVOKABLE void clear();
    // Asks the window to play the line and open the Card Creator on it.
    Q_INVOKABLE void open(int id);
    void notifyChanged() { emit changed(); }
signals:
    void changed();
    void openRequested(const QVariantMap &entry);
private:
    using QObject::QObject;
};
