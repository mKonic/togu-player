#include "mokuro.h"

#include "core/library/names.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace mokuro {

namespace {

QList<Box> readBlocks(const QJsonArray &blocks)
{
    QList<Box> out;
    for (const QJsonValue &v : blocks) {
        const QJsonObject b = v.toObject();
        const QJsonArray box = b.value("box").toArray();
        if (box.size() < 4) continue;
        Box x;
        x.rect = QRectF(QPointF(box[0].toDouble(), box[1].toDouble()), QPointF(box[2].toDouble(), box[3].toDouble()));
        x.vertical = b.value("vertical").toBool(true);
        x.fontSize = b.value("font_size").toDouble();
        for (const QJsonValue &l : b.value("lines").toArray()) x.lines << l.toString();
        if (!x.lines.isEmpty()) out.append(x);
    }
    return out;
}

QString findImage(const QDir &dir, const QString &stem)
{
    for (const char *ext : {"jpg", "jpeg", "png", "webp", "avif", "gif", "bmp"}) {
        const QString p = dir.filePath(stem + u'.' + QLatin1String(ext));
        if (QFileInfo::exists(p)) return p;
    }
    return {};
}

} // namespace

Volume load(const QString &path)
{
    Volume v;
    v.path = path;
    const QFileInfo fi(path);
    if (fi.isFile()) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            v.error = QObject::tr("Can't open %1").arg(path);
            return v;
        }
        const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
        v.title = root.value("title").toString();
        v.volume = root.value("volume").toString(fi.completeBaseName());
        const QDir imgDir(fi.dir().filePath(fi.completeBaseName()));
        for (const QJsonValue &pv : root.value("pages").toArray()) {
            const QJsonObject p = pv.toObject();
            Page page;
            page.image = imgDir.filePath(p.value("img_path").toString());
            page.width = p.value("img_width").toInt();
            page.height = p.value("img_height").toInt();
            page.boxes = readBlocks(p.value("blocks").toArray());
            v.pages.append(page);
        }
    } else if (fi.isDir()) {
        // Older layout: <dir>/_ocr/<volume>/*.json with images in <dir>/<volume>/
        const QDir ocr(path);
        const QDir imgDir(QFileInfo(fi.dir().path()).dir().filePath(fi.fileName()));
        v.volume = fi.fileName();
        v.title = QFileInfo(QFileInfo(fi.dir().path()).dir().path()).fileName();
        QStringList jsons = ocr.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
        std::sort(jsons.begin(), jsons.end(), [](const QString &a, const QString &b) {
            return QString::localeAwareCompare(a, b) < 0;
        });
        for (const QString &j : jsons) {
            QFile f(ocr.filePath(j));
            if (!f.open(QIODevice::ReadOnly)) continue;
            const QJsonObject p = QJsonDocument::fromJson(f.readAll()).object();
            Page page;
            page.image = findImage(imgDir, QFileInfo(j).completeBaseName());
            page.width = p.value("img_width").toInt();
            page.height = p.value("img_height").toInt();
            page.boxes = readBlocks(p.value("blocks").toArray());
            if (!page.image.isEmpty()) v.pages.append(page);
        }
    }
    if (v.pages.isEmpty() && v.error.isEmpty()) v.error = QObject::tr("No pages found.");
    return v;
}

QStringList findVolumes(const QString &root)
{
    QStringList out;
    QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext()) {
        const QString p = it.next();
        const QFileInfo fi = it.fileInfo();
        if (fi.isFile() && fi.suffix().compare(u"mokuro", Qt::CaseInsensitive) == 0) out << p;
        else if (fi.isDir() && QFileInfo(fi.dir().path()).fileName() == u"_ocr") {
            // Skip old-layout volumes that also have a .mokuro file.
            const QString parent = QFileInfo(fi.dir().path()).dir().path();
            if (!QFileInfo::exists(parent + u'/' + fi.fileName() + QStringLiteral(".mokuro"))) out << p;
        }
    }
    std::sort(out.begin(), out.end(), [](const QString &a, const QString &b) { return QString::localeAwareCompare(a, b) < 0; });
    return out;
}

QString cover(const QString &path)
{
    const QFileInfo fi(path);
    QDir imgDir = fi.isFile() ? QDir(fi.dir().filePath(fi.completeBaseName())) : QDir(QFileInfo(fi.dir().path()).dir().filePath(fi.fileName()));
    QStringList imgs;
    for (const QFileInfo &f : imgDir.entryInfoList(QDir::Files, QDir::Name))
        if (names::isImage(f.fileName())) imgs << f.absoluteFilePath();
    std::sort(imgs.begin(), imgs.end(), [](const QString &a, const QString &b) { return QString::localeAwareCompare(a, b) < 0; });
    return imgs.value(0);
}

} // namespace mokuro
