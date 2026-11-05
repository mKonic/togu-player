#include "jp.h"

#include <QRegularExpression>

namespace jp {

bool isKanji(char32_t c)
{
    return (c >= 0x4e00 && c <= 0x9fff) || (c >= 0x3400 && c <= 0x4dbf) || c == 0x3005 || c == 0x3006 ||
           (c >= 0xf900 && c <= 0xfaff) || (c >= 0x20000 && c <= 0x2ffff);
}

bool isHiragana(char32_t c) { return c >= 0x3041 && c <= 0x309f; }

bool isKatakana(char32_t c) { return (c >= 0x30a1 && c <= 0x30ff) || (c >= 0xff66 && c <= 0xff9f); }

bool hasJapanese(const QString &s)
{
    for (char32_t c : s.toUcs4()) {
        if ((c >= 0x3041 && c <= 0x30ff) || (c >= 0x3400 && c <= 0x4dbf) || (c >= 0x4e00 && c <= 0x9fff) ||
            (c >= 0xff66 && c <= 0xff9f))
            return true;
    }
    return false;
}

bool hasKanji(const QString &s)
{
    for (char32_t c : s.toUcs4())
        if (isKanji(c)) return true;
    return false;
}

bool allKana(const QString &s)
{
    for (char32_t c : s.toUcs4())
        if (!isKana(c)) return false;
    return true;
}

QString toHiragana(const QString &s)
{
    QString out = s;
    for (QChar &ch : out) {
        ushort c = ch.unicode();
        if (c >= 0x30a1 && c <= 0x30f6) ch = QChar(c - 0x60);
    }
    return out;
}

QString toKatakana(const QString &s)
{
    QString out = s;
    for (QChar &ch : out) {
        ushort c = ch.unicode();
        if (c >= 0x3041 && c <= 0x3096) ch = QChar(c + 0x60);
    }
    return out;
}

QList<FuriSeg> furiganaSegments(const QString &surface, const QString &reading)
{
    if (reading.isEmpty() || !hasKanji(surface)) return {{surface, {}}};
    const QString hira = toHiragana(reading);
    struct Run {
        QString text;
        bool kanji;
    };
    QList<Run> runs;
    for (char32_t c : surface.toUcs4()) {
        bool kanji = !isKana(c) && c != U'ー';
        QString ch = QString::fromUcs4(&c, 1);
        if (!runs.isEmpty() && runs.last().kanji == kanji) runs.last().text += ch;
        else runs.append({ch, kanji});
    }
    QString pattern = QStringLiteral("^");
    for (const Run &r : runs)
        pattern += r.kanji ? QStringLiteral("(.+)") : u'(' + QRegularExpression::escape(toHiragana(r.text)) + u')';
    pattern += u'$';
    auto m = QRegularExpression(pattern).match(hira);
    if (!m.hasMatch()) return {{surface, hira}};
    QList<FuriSeg> out;
    for (qsizetype i = 0; i < runs.size(); ++i)
        out.append(runs[i].kanji ? FuriSeg{runs[i].text, m.captured(i + 1)} : FuriSeg{runs[i].text, {}});
    return out;
}

QString furiganaPlain(const QList<FuriSeg> &segs)
{
    QString out;
    for (const FuriSeg &s : segs) {
        if (!s.reading.isEmpty()) {
            if (!out.isEmpty() && !out.endsWith(u' ')) out += u' ';
            out += s.text + u'[' + s.reading + u']';
        } else {
            out += s.text;
        }
    }
    return out;
}

QString furiganaHtml(const QList<FuriSeg> &segs)
{
    QString out;
    for (const FuriSeg &s : segs) {
        if (!s.reading.isEmpty())
            out += QStringLiteral("<ruby>%1<rt>%2</rt></ruby>").arg(escapeHtml(s.text), escapeHtml(s.reading));
        else
            out += escapeHtml(s.text);
    }
    return out;
}

QString escapeHtml(const QString &s) { return s.toHtmlEscaped(); }

int moraCount(const QString &reading)
{
    static const QString small = QStringLiteral("ゃゅょぁぃぅぇぉゎャュョァィゥェォヮ");
    int n = 0;
    for (QChar c : reading)
        if (!small.contains(c)) ++n;
    return n;
}

} // namespace jp
