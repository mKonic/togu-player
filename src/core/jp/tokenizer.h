#pragma once
// lindera (IPADIC) analysis plus togu's merge rules, which join a head word with
// its inflection so 読んでいる is one unit.

#include <QList>
#include <QString>

namespace jp {

struct RawToken {
    QString surface;
    QString baseForm;
    QString reading;
    QString pos;
    QString pos1;
    QString ctype; // conjugation class, e.g. 一段
    QString cform; // conjugated form, e.g. 連用形
};

struct Token {
    QString surface;
    QString lemma;
    QString lemmaReading; // hiragana reading of the dictionary form, empty when unknown
    QString key;          // identity for status: dictionary form plus its reading
    QString reading;      // hiragana reading of the surface
    QString pos;
    bool word = false;    // counts as vocabulary
};

// What makes two occurrences the same word: the dictionary form plus its reading.
QString wordKey(const QString &lemma, const QString &reading);
std::pair<QString, QString> splitKey(const QString &key);

// Thread-safe; the dictionary is embedded and loaded on first use.
QList<RawToken> tokenizeRaw(const QString &text);
QList<Token> tokenize(const QString &text);

using LemmaLookup = QString (*)(const QString &lemma);
QList<Token> mergeTokens(const QList<RawToken> &raw, LemmaLookup lookup = nullptr);
QString lemmaReadingFrom(const RawToken &head);

} // namespace jp
