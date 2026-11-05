#pragma once
// Books for the reader: EPUB (spine order, text and pictures) and plain text.

#include <QList>
#include <QString>
#include <QStringList>

namespace book {

struct Block {
    enum Kind { Paragraph, Heading, Image } kind = Paragraph;
    QString text;  // paragraph or heading text, ruby readings removed
    QString image; // extracted picture path for Image blocks
};

struct Chapter {
    QString title;
    QList<Block> blocks;
};

struct Book {
    QString path, title, author, cover; // cover: extracted image path
    QList<Chapter> chapters;
    QString error;
};

// Reads the whole book; pictures are extracted to the cache once.
Book load(const QString &path);
// Title, author and cover only, for the library grid.
Book info(const QString &path);
bool isBook(const QString &path);

} // namespace book
