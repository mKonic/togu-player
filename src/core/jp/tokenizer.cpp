#include "tokenizer.h"

#include "jp.h"

#include <QHash>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>

extern "C" {
struct Tok;
Tok *jptok_new();
void jptok_free(Tok *);
unsigned char *jptok_tokenize(Tok *, const char *text, size_t len, size_t *out_len);
void jptok_buf_free(unsigned char *, size_t);
}

namespace jp {

namespace {

const QChar SEP(0x1f);

QMutex g_mutex;
Tok *g_tok = nullptr;
QHash<QString, QString> g_lemmaReadings;

Tok *tok()
{
    if (!g_tok) g_tok = jptok_new();
    return g_tok;
}

// Analyzes one run of text with no whitespace in it. Caller holds g_mutex.
void analyze(const QString &text, QList<RawToken> &out)
{
    QByteArray utf8 = text.toUtf8();
    size_t len = 0;
    unsigned char *buf = jptok_tokenize(tok(), utf8.constData(), size_t(utf8.size()), &len);
    if (!buf) return;
    QString all = QString::fromUtf8(reinterpret_cast<const char *>(buf), qsizetype(len));
    jptok_buf_free(buf, len);
    for (QStringView t : QStringView(all).split(QChar(0x1e))) {
        auto f = t.split(SEP);
        auto at = [&](int i) { return i < f.size() ? f[i].toString() : QString(); };
        out.append({at(0), at(1), at(2), at(3), at(4), at(5), at(6)});
    }
}

QString lemmaReadingLocked(const QString &lemma)
{
    auto it = g_lemmaReadings.constFind(lemma);
    if (it != g_lemmaReadings.constEnd()) return *it;
    QList<RawToken> parts;
    analyze(lemma, parts);
    QString r;
    bool ok = !parts.isEmpty();
    for (const RawToken &p : parts) {
        if (p.reading.isEmpty() || p.reading == u"*") ok = false;
        r += p.reading;
    }
    r = ok ? toHiragana(r) : QString();
    g_lemmaReadings.insert(lemma, r);
    return r;
}

QString lookupLocked(const QString &lemma) { return lemmaReadingLocked(lemma); }

bool isInflecting(const RawToken &t) { return t.pos == u"動詞" || t.pos == u"形容詞" || t.pos == u"助動詞"; }

// Verbs that follow て and only add aspect; they stay attached to the head so
// 読んでいる is one unit, while しまう/おく/くる remain words of their own.
bool isTeAux(const QString &b) { return b == u"いる" || b == u"る" || b == u"おる"; }

// Whether `next` continues the inflection of the unit ending in `prev`.
bool attaches(const RawToken &head, const RawToken &prev, const RawToken &next)
{
    if (!isInflecting(head)) return false;
    if (next.pos == u"助動詞") {
        // だ/です after a plain form is a new clause element (行くだろう, 高いです).
        if ((next.baseForm == u"だ" || next.baseForm == u"です") && prev.cform == u"基本形") return false;
        if (next.baseForm == u"らしい" || next.baseForm == u"みたい") return false;
        return true;
    }
    if (next.pos == u"動詞" && next.pos1 == u"接尾") return true; // れる, られる, せる, させる
    if (next.pos == u"動詞" && next.pos1 == u"非自立" && isTeAux(next.baseForm))
        return prev.pos == u"助詞" && (prev.surface == u"て" || prev.surface == u"で");
    if (next.pos == u"助詞" && next.pos1 == u"接続助詞") {
        if (next.surface == u"て" || next.surface == u"で") return prev.cform.startsWith(u"連用");
        if (next.surface == u"ば") return prev.cform == u"仮定形";
        return false;
    }
    if (next.pos == u"助詞" && next.pos1 == u"並立助詞" && (next.surface == u"たり" || next.surface == u"だり"))
        return true;
    return false;
}

// As in Migaku, every piece of Japanese text is a word, particles, auxiliaries
// and counters included. Left out are symbols and anything with characters other
// than kana, kanji and full-width letters.
bool wordChars(const QString &s)
{
    if (s.isEmpty()) return false;
    for (QChar ch : s) {
        ushort c = ch.unicode();
        bool ok = (c >= 0x4e00 && c <= 0x9fef) || (c >= 0x3041 && c <= 0x3093) || (c >= 0x30a1 && c <= 0x30f3) ||
                  (c >= 0xff67 && c <= 0xff9f) || c == 0x30fc || (c >= 0xff21 && c <= 0xff3a) ||
                  (c >= 0xff41 && c <= 0xff5a) || (c >= 0xff10 && c <= 0xff19) || c == 0x3005;
        if (!ok) return false;
    }
    return true;
}

bool allDigits(const QString &s)
{
    if (s.isEmpty()) return false;
    for (QChar ch : s) {
        ushort c = ch.unicode();
        if (!((c >= '0' && c <= '9') || (c >= 0xff10 && c <= 0xff19))) return false;
    }
    return true;
}

bool isWord(const RawToken &t, const QString &surface)
{
    return t.pos != u"記号" && hasJapanese(surface) && wordChars(surface) && !allDigits(surface);
}

QString readingOf(const RawToken &t)
{
    if (t.reading.isEmpty() || t.reading == u"*") return {};
    return toHiragana(t.reading);
}

QString baseOf(const RawToken &t) { return !t.baseForm.isEmpty() && t.baseForm != u"*" ? t.baseForm : t.surface; }

} // namespace

QString wordKey(const QString &lemma, const QString &reading)
{
    return reading.isEmpty() ? lemma : lemma + SEP + toHiragana(reading);
}

std::pair<QString, QString> splitKey(const QString &key)
{
    qsizetype i = key.indexOf(SEP);
    if (i < 0) return {key, {}};
    return {key.left(i), key.mid(i + 1)};
}

QList<RawToken> tokenizeRaw(const QString &text)
{
    QMutexLocker lock(&g_mutex);
    QList<RawToken> out;
    // The analyzer drops whitespace, so each run is fed on its own and the gaps
    // are put back as symbol tokens to keep surfaces adding up to `text`.
    static const QRegularExpression ws(QStringLiteral("\\s+"), QRegularExpression::UseUnicodePropertiesOption);
    qsizetype pos = 0;
    auto it = ws.globalMatch(text);
    auto flush = [&](qsizetype end) {
        if (end > pos) analyze(text.mid(pos, end - pos), out);
    };
    while (it.hasNext()) {
        auto m = it.next();
        flush(m.capturedStart());
        QString gap = m.captured();
        out.append({gap, gap, {}, QStringLiteral("記号"), QStringLiteral("空白"), QStringLiteral("*"), QStringLiteral("*")});
        pos = m.capturedEnd();
    }
    flush(text.size());
    return out;
}

// The reading of a word's dictionary form, worked out from how the inflected
// form was read in this sentence: the shared part keeps its reading and the
// kana ending is swapped (辛かっ/つらかっ -> 辛い/つらい). Empty where that does
// not hold, as for 来 (き) -> 来る (くる).
QString lemmaReadingFrom(const RawToken &head)
{
    const QString reading = readingOf(head);
    const QString base = baseOf(head);
    if (reading.isEmpty()) return {};
    if (base == head.surface) return reading;
    if (head.ctype.startsWith(u"カ変") || head.ctype.startsWith(u"サ変")) return {};
    qsizetype n = 0;
    while (n < base.size() && n < head.surface.size() && base[n] == head.surface[n]) ++n;
    const QString tail = toHiragana(head.surface.mid(n));
    const QString baseTail = base.mid(n);
    if (!allKana(tail) || !allKana(baseTail) || !reading.endsWith(tail)) return {};
    return reading.left(reading.size() - tail.size()) + toHiragana(baseTail);
}

QList<Token> mergeTokens(const QList<RawToken> &raw, LemmaLookup lookup)
{
    QList<Token> out;
    qsizetype i = 0;
    while (i < raw.size()) {
        const RawToken &head = raw[i];
        QString surface = head.surface;
        QString reading = readingOf(head);
        bool readingOk = !reading.isEmpty() || !hasJapanese(head.surface);
        qsizetype j = i + 1;
        if (head.pos == u"名詞" && head.pos1 == u"数") {
            while (j < raw.size() && raw[j].pos == u"名詞" && raw[j].pos1 == u"数") {
                surface += raw[j].surface;
                reading += readingOf(raw[j]);
                ++j;
            }
        } else {
            while (j < raw.size() && attaches(head, raw[j - 1], raw[j])) {
                surface += raw[j].surface;
                QString r = readingOf(raw[j]);
                if (r.isEmpty() && hasJapanese(raw[j].surface)) readingOk = false;
                reading += r.isEmpty() ? raw[j].surface : r;
                ++j;
            }
        }
        Token t;
        t.surface = surface;
        t.lemma = baseOf(head);
        t.word = isWord(head, surface);
        if (t.word) {
            t.lemmaReading = lemmaReadingFrom(head);
            if (t.lemmaReading.isEmpty() && lookup) t.lemmaReading = lookup(t.lemma);
        }
        t.key = wordKey(t.lemma, t.lemmaReading);
        t.reading = readingOk ? reading : QString();
        t.pos = head.pos;
        out.append(t);
        i = j;
    }
    return out;
}

QList<Token> tokenize(const QString &text)
{
    QList<RawToken> raw = tokenizeRaw(text);
    QMutexLocker lock(&g_mutex);
    return mergeTokens(raw, lookupLocked);
}

} // namespace jp
