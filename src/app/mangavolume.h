#pragma once
// A mokuro volume open in the manga viewer.

#include "core/manga/mokuro.h"

#include <QObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class MangaVolume : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString title READ title NOTIFY opened)
    Q_PROPERTY(QVariantList pages READ pages NOTIFY opened)
    Q_PROPERTY(int startPage READ startPage NOTIFY opened)
public:
    using QObject::QObject;
    QString title() const;
    QVariantList pages() const { return m_pages; }
    int startPage() const { return m_start; }

    Q_INVOKABLE void open(const QString &path);
    Q_INVOKABLE void savePage(int page);
    // A crop of a page around a text box (with some room), for a card picture.
    Q_INVOKABLE QString crop(int page, double x, double y, double w, double h) const;
    // Comprehension of the whole volume's text.
    Q_INVOKABLE QVariantMap stats() const;

signals:
    void opened();
    void failed(const QString &message);

private:
    mokuro::Volume m_vol;
    QVariantList m_pages;
    int m_start = 0;
};
