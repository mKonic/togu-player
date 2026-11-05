#include "book.h"

#include "core/subs/subs.h"
#include "core/util/arc.h"
#include "core/util/db.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QXmlStreamReader>

namespace book {

namespace {

QString cacheFor(const QString &path)
{
    const QFileInfo fi(path);
    const QString id = QString::fromLatin1(QCryptographicHash::hash((path + QString::number(fi.size())).toUtf8(), QCryptographicHash::Sha1).toHex().left(16));
    const QString dir = db::cacheDir() + QStringLiteral("/books/") + id;
    QDir().mkpath(dir);
    return dir;
}

QString resolve(const QString &base, const QString &href)
{
    QString h = QUrl::fromPercentEncoding(href.section(u'#', 0, 0).toUtf8());
    if (h.isEmpty()) return {};
    const QString dir = base.contains(u'/') ? base.section(u'/', 0, -2) + u'/' : QString();
    return QDir::cleanPath(dir + h);
}

struct Opf {
    QString title, author, coverHref, opfPath;
    QStringList spine; // hrefs, resolved
    QHash<QString, QString> mediaType;
};

Opf readOpf(const QHash<QString, QByteArray> &files)
{
    Opf o;
    QXmlStreamReader c(files.value(QStringLiteral("META-INF/container.xml")));
    while (!c.atEnd())
        if (c.readNext() == QXmlStreamReader::StartElement && c.name() == u"rootfile") o.opfPath = c.attributes().value("full-path").toString();
    QXmlStreamReader x(files.value(o.opfPath));
    QHash<QString, QString> idHref;
    QString coverId;
    QStringList spineIds;
    while (!x.atEnd()) {
        if (x.readNext() != QXmlStreamReader::StartElement) continue;
        const auto n = x.name();
        const auto a = x.attributes();
        if (n == u"title" && o.title.isEmpty()) o.title = x.readElementText().trimmed();
        else if (n == u"creator" && o.author.isEmpty()) o.author = x.readElementText().trimmed();
        else if (n == u"meta" && a.value("name") == u"cover") coverId = a.value("content").toString();
        else if (n == u"item") {
            const QString href = resolve(o.opfPath, a.value("href").toString());
            idHref.insert(a.value("id").toString(), href);
            o.mediaType.insert(href, a.value("media-type").toString());
            if (a.value("properties").toString().contains(u"cover-image")) o.coverHref = href;
        } else if (n == u"itemref") spineIds << a.value("idref").toString();
    }
    if (o.coverHref.isEmpty() && !coverId.isEmpty()) o.coverHref = idHref.value(coverId);
    for (const QString &id : spineIds)
        if (idHref.contains(id)) o.spine << idHref.value(id);
    return o;
}

QString saveImage(const QString &dir, const QString &href, const QByteArray &data)
{
    if (data.isEmpty()) return {};
    const QString out = dir + u'/' + QString::fromLatin1(QCryptographicHash::hash(href.toUtf8(), QCryptographicHash::Md5).toHex().left(12)) + u'.' + QFileInfo(href).suffix();
    if (!QFileInfo::exists(out)) {
        QFile f(out);
        if (f.open(QIODevice::WriteOnly)) f.write(data);
    }
    return out;
}

// XHTML to blocks: paragraph-level elements become blocks, ruby readings are dropped.
Chapter readXhtml(const QString &href, const QByteArray &xml, const QHash<QString, QByteArray> &files, const QString &imgDir)
{
    Chapter ch;
    QXmlStreamReader x(xml);
    QString cur;
    bool heading = false;
    int skip = 0; // inside rt/rp/script/style
    static const QSet<QString> blocks = {"p", "div", "li", "blockquote", "h1", "h2", "h3", "h4", "h5", "h6", "dt", "dd", "td"};
    auto flush = [&] {
        QString t = cur.trimmed();
        t.replace(QRegularExpression(QStringLiteral("[ \\t]*\\n[ \\t]*")), QStringLiteral("\n"));
        if (!t.isEmpty()) {
            Block b;
            b.kind = heading ? Block::Heading : Block::Paragraph;
            b.text = t;
            if (heading && ch.title.isEmpty()) ch.title = t.left(60);
            ch.blocks.append(b);
        }
        cur.clear();
        heading = false;
    };
    while (!x.atEnd()) {
        const auto tok = x.readNext();
        if (tok == QXmlStreamReader::StartElement) {
            const QString n = x.name().toString().toLower();
            if (n == u"rt" || n == u"rp" || n == u"script" || n == u"style" || n == u"head") ++skip;
            else if (blocks.contains(n)) {
                flush();
                heading = n.startsWith(u'h') && n.size() == 2;
            } else if (n == u"br") cur += u'\n';
            else if (n == u"img" || n == u"image") {
                QString src = x.attributes().value("src").toString();
                if (src.isEmpty()) src = x.attributes().value("http://www.w3.org/1999/xlink", "href").toString();
                if (src.isEmpty()) src = x.attributes().value("href").toString();
                const QString path = resolve(href, src);
                const QString saved = saveImage(imgDir, path, files.value(path));
                if (!saved.isEmpty()) {
                    flush();
                    Block b;
                    b.kind = Block::Image;
                    b.image = saved;
                    ch.blocks.append(b);
                }
            }
        } else if (tok == QXmlStreamReader::EndElement) {
            const QString n = x.name().toString().toLower();
            if ((n == u"rt" || n == u"rp" || n == u"script" || n == u"style" || n == u"head") && skip > 0) --skip;
            else if (blocks.contains(n)) flush();
        } else if (tok == QXmlStreamReader::Characters && !skip) {
            cur += x.text();
        }
    }
    flush();
    return ch;
}

QHash<QString, QByteArray> readAll(const QString &path)
{
    QHash<QString, QByteArray> files;
    arc::each(path, [](const QString &) { return true; }, [&](const QString &n, const QByteArray &d) {
        files.insert(n, d);
        return true;
    });
    return files;
}

Book loadText(const QString &path)
{
    Book b;
    b.path = path;
    b.title = QFileInfo(path).completeBaseName();
    QString text = subs::readText(path);
    text.remove(u'\r');
    // Aozora ruby markup: ｜漢字《かんじ》 -> 漢字
    text.remove(QRegularExpression(QStringLiteral("《[^》]*》")));
    text.remove(u'｜');
    text.remove(QRegularExpression(QStringLiteral("［＃[^］]*］")));
    Chapter ch;
    int n = 0;
    for (const QString &line : text.split(u'\n')) {
        const QString t = line.trimmed();
        if (t.isEmpty()) continue;
        ch.blocks.append({Block::Paragraph, t, {}});
        // Long texts split into parts so a page stays light.
        if (++n % 300 == 0) {
            ch.title = QObject::tr("Part %1").arg(b.chapters.size() + 1);
            b.chapters.append(ch);
            ch = {};
        }
    }
    if (!ch.blocks.isEmpty()) {
        ch.title = b.chapters.isEmpty() ? b.title : QObject::tr("Part %1").arg(b.chapters.size() + 1);
        b.chapters.append(ch);
    }
    return b;
}

} // namespace

bool isBook(const QString &path)
{
    const QString s = QFileInfo(path).suffix().toLower();
    return s == u"epub" || s == u"txt";
}

Book load(const QString &path)
{
    if (QFileInfo(path).suffix().compare(u"txt", Qt::CaseInsensitive) == 0) return loadText(path);
    Book b;
    b.path = path;
    const auto files = readAll(path);
    const Opf o = readOpf(files);
    if (o.spine.isEmpty()) {
        b.error = QObject::tr("This EPUB has no readable chapters.");
        return b;
    }
    const QString dir = cacheFor(path);
    b.title = o.title.isEmpty() ? QFileInfo(path).completeBaseName() : o.title;
    b.author = o.author;
    if (!o.coverHref.isEmpty()) b.cover = saveImage(dir, o.coverHref, files.value(o.coverHref));
    for (const QString &href : o.spine) {
        Chapter ch = readXhtml(href, files.value(href), files, dir);
        if (ch.blocks.isEmpty()) continue;
        if (ch.title.isEmpty()) ch.title = QObject::tr("Chapter %1").arg(b.chapters.size() + 1);
        b.chapters.append(ch);
    }
    return b;
}

Book info(const QString &path)
{
    Book b;
    b.path = path;
    b.title = QFileInfo(path).completeBaseName();
    if (QFileInfo(path).suffix().compare(u"epub", Qt::CaseInsensitive) != 0) return b;
    QHash<QString, QByteArray> files;
    // Only the small files: container, OPF; the cover after we know its name.
    arc::each(path, [](const QString &n) { return n.endsWith(u".xml") || n.endsWith(u".opf"); }, [&](const QString &n, const QByteArray &d) {
        files.insert(n, d);
        return true;
    });
    const Opf o = readOpf(files);
    if (!o.title.isEmpty()) b.title = o.title;
    b.author = o.author;
    if (!o.coverHref.isEmpty()) b.cover = saveImage(cacheFor(path), o.coverHref, arc::read(path, o.coverHref));
    return b;
}

} // namespace book
