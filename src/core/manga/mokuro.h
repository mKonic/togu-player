#pragma once
// mokuro output: page images plus OCR'd text boxes.
//   current: <dir>/<volume>.mokuro (JSON with every page), images in <dir>/<volume>/
//   older:   <dir>/_ocr/<volume>/<page>.json per page, images in <dir>/<volume>/

#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

namespace mokuro {

struct Box {
    QRectF rect; // in image pixels
    bool vertical = true;
    double fontSize = 0;
    QStringList lines;
    QString text() const { return lines.join(QString()); }
};

struct Page {
    QString image;
    int width = 0, height = 0;
    QList<Box> boxes;
};

struct Volume {
    QString path, title, volume;
    QList<Page> pages;
    QString error;
};

Volume load(const QString &path); // a .mokuro file or an _ocr/<volume> folder
// Volumes under a folder, for scanning.
QStringList findVolumes(const QString &root);
// Cover: the first page's image.
QString cover(const QString &path);

} // namespace mokuro
