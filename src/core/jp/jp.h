#pragma once
// Character-level helpers for Japanese text.

#include <QList>
#include <QString>

namespace jp {

bool isKanji(char32_t c);
bool isHiragana(char32_t c);
bool isKatakana(char32_t c);
inline bool isKana(char32_t c) { return isHiragana(c) || isKatakana(c); }

bool hasJapanese(const QString &s);
bool hasKanji(const QString &s);
bool allKana(const QString &s);

QString toHiragana(const QString &s);
QString toKatakana(const QString &s);

struct FuriSeg {
    QString text;
    QString reading; // empty for kana runs
};

// Splits a word into runs so each kanji run carries its own reading:
// 食べ物 + たべもの -> 食[た] べ 物[もの].
QList<FuriSeg> furiganaSegments(const QString &surface, const QString &reading);
// Anki's bracket notation: 食[た]べ 物[もの].
QString furiganaPlain(const QList<FuriSeg> &segs);
QString furiganaHtml(const QList<FuriSeg> &segs);

QString escapeHtml(const QString &s);

// The mora count of a kana reading (small kana join the one before).
int moraCount(const QString &reading);
// Accent pattern for a drop position: h heiban, a atamadaka, n nakadaka, o odaka,
// k kifuku (verbs and i-adjectives with a drop, since it moves as they inflect).
QChar pitchClass(int position, const QString &reading, const QString &pos);

} // namespace jp
