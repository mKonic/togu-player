#include "subs.h"

#include "core/jp/jp.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>

#include <algorithm>

namespace subs {

namespace {

double clock(const QString &h, const QString &m, const QString &s, const QString &frac)
{
    return h.toDouble() * 3600 + m.toDouble() * 60 + s.toDouble() + (u"0." + frac).toDouble();
}

QString clean(QString text)
{
    static const QRegularExpression tags(QStringLiteral("<[^>]+>"));
    static const QRegularExpression ass(QStringLiteral("\\{\\\\[^}]*\\}"));
    static const QRegularExpression trail(QStringLiteral("[ \\t]+\\n"));
    text.replace(tags, QString()).replace(ass, QString());
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" ")).replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<")).replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(trail, QStringLiteral("\n"));
    return text.trimmed();
}

QString prep(const QString &src)
{
    QString t = src;
    t.remove(u'\r');
    if (t.startsWith(QChar(0xfeff))) t.remove(0, 1);
    return t;
}

struct AssEvent {
    QString kind; // Dialogue or Comment
    QStringList cols;
    QString raw;
};

struct AssDoc {
    QStringList header; // everything before [Events], plus the Events section line and Format
    QStringList format;
    QList<AssEvent> events;
};

AssDoc readAss(const QString &src)
{
    AssDoc d;
    bool inEvents = false;
    for (const QString &line : prep(src).split(u'\n')) {
        const QString t = line.trimmed();
        if (t.startsWith(u'[') && t.endsWith(u']')) {
            inEvents = t.compare(u"[events]", Qt::CaseInsensitive) == 0;
            d.header << line;
            continue;
        }
        if (!inEvents) {
            d.header << line;
            continue;
        }
        if (line.startsWith(u"Format:")) {
            d.format.clear();
            for (const QString &f : line.mid(7).split(u',')) d.format << f.trimmed().toLower();
            d.header << line;
        } else if ((line.startsWith(u"Dialogue:") || line.startsWith(u"Comment:")) && !d.format.isEmpty()) {
            const qsizetype colon = line.indexOf(u':');
            const QStringList parts = line.mid(colon + 1).split(u',');
            QStringList cols = parts.mid(0, d.format.size() - 1);
            cols << parts.mid(d.format.size() - 1).join(u',');
            for (QString &c : cols.first(cols.size() - 1)) c = c.trimmed();
            d.events.append({line.left(colon), cols, line});
        }
    }
    return d;
}

double assTime(const QString &s)
{
    static const QRegularExpression re(QStringLiteral("(\\d+):(\\d+):(\\d+)\\.(\\d+)"));
    auto m = re.match(s);
    return m.hasMatch() ? clock(m.captured(1), m.captured(2), m.captured(3), m.captured(4)) : 0;
}

QString assText(const QString &raw)
{
    QString t = raw;
    t.replace(QRegularExpression(QStringLiteral("\\\\N"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\n"));
    t.replace(QStringLiteral("\\h"), QStringLiteral(" "));
    return clean(t);
}

// Lines placed by hand, drawn, or styled as signs/songs belong to mpv.
bool isSign(const QString &style, const QString &text)
{
    static const QRegularExpression tags(QStringLiteral("\\\\(pos|move|clip|iclip|org|p[1-9]|frz?|frx|fry|fax|fay)\\b"));
    static const QRegularExpression styles(QStringLiteral("sign|title|typeset|^ts|screen|note|kara|^op|^ed|song|lyric|insert"),
                                           QRegularExpression::CaseInsensitiveOption);
    return tags.match(text).hasMatch() || styles.match(style).hasMatch();
}

} // namespace

QList<SubLine> parseSrt(const QString &src)
{
    QList<SubLine> out;
    static const QRegularExpression re(QStringLiteral(
        "(\\d+):(\\d+):(\\d+)[,.](\\d+)\\s*-->\\s*(\\d+):(\\d+):(\\d+)[,.](\\d+)[^\\n]*\\n([\\s\\S]*?)(?=\\n\\s*\\n|\\n*$)"));
    auto it = re.globalMatch(prep(src));
    while (it.hasNext()) {
        auto m = it.next();
        QString body = clean(m.captured(9));
        if (!body.isEmpty())
            out.append({clock(m.captured(1), m.captured(2), m.captured(3), m.captured(4)),
                        clock(m.captured(5), m.captured(6), m.captured(7), m.captured(8)), body});
    }
    return out;
}

QList<SubLine> parseVtt(const QString &src)
{
    QList<SubLine> out;
    static const QRegularExpression re(QStringLiteral(
        "(?:(\\d+):)?(\\d+):(\\d+)\\.(\\d+)\\s*-->\\s*(?:(\\d+):)?(\\d+):(\\d+)\\.(\\d+)[^\\n]*\\n([\\s\\S]*?)(?=\\n\\s*\\n|\\n*$)"));
    auto it = re.globalMatch(prep(src));
    while (it.hasNext()) {
        auto m = it.next();
        QString body = clean(m.captured(9));
        auto h = [](const QString &s) { return s.isEmpty() ? QStringLiteral("0") : s; };
        if (!body.isEmpty())
            out.append({clock(h(m.captured(1)), m.captured(2), m.captured(3), m.captured(4)),
                        clock(h(m.captured(5)), m.captured(6), m.captured(7), m.captured(8)), body});
    }
    return out;
}

QList<SubLine> parseAss(const QString &src)
{
    QList<SubLine> out;
    const AssDoc d = readAss(src);
    const qsizetype iStart = d.format.indexOf(u"start"), iEnd = d.format.indexOf(u"end"), iText = d.format.indexOf(u"text");
    for (const AssEvent &e : d.events) {
        if (e.kind != u"Dialogue") continue;
        QString body = assText(e.cols.value(iText));
        if (!body.isEmpty()) out.append({assTime(e.cols.value(iStart)), assTime(e.cols.value(iEnd)), body});
    }
    std::stable_sort(out.begin(), out.end(), [](auto &a, auto &b) { return a.start < b.start; });
    return out;
}

AssSplit splitAss(const QString &src)
{
    AssSplit s;
    const AssDoc d = readAss(src);
    const qsizetype iStart = d.format.indexOf(u"start"), iEnd = d.format.indexOf(u"end"), iText = d.format.indexOf(u"text"),
                    iStyle = d.format.indexOf(u"style");
    QStringList signLines;
    for (const AssEvent &e : d.events) {
        if (e.kind != u"Dialogue") continue;
        const QString rawText = e.cols.value(iText);
        if (isSign(e.cols.value(iStyle), rawText)) {
            signLines << e.raw;
            ++s.signs;
            continue;
        }
        QString body = assText(rawText);
        if (!body.isEmpty()) s.dialogue.append({assTime(e.cols.value(iStart)), assTime(e.cols.value(iEnd)), body});
    }
    std::stable_sort(s.dialogue.begin(), s.dialogue.end(), [](auto &a, auto &b) { return a.start < b.start; });
    if (!signLines.isEmpty()) s.signsAss = d.header.join(u'\n') + u'\n' + signLines.join(u'\n') + u'\n';
    return s;
}

// [mm:ss.xx] lines; each lasts until the next one starts.
QList<SubLine> parseLrc(const QString &src, double lastEnd)
{
    QList<SubLine> out;
    static const QRegularExpression stamp(QStringLiteral("\\[(\\d+):(\\d+(?:\\.\\d+)?)\\]"));
    for (const QString &line : prep(src).split(u'\n')) {
        QList<double> times;
        qsizetype pos = 0;
        auto it = stamp.globalMatch(line);
        while (it.hasNext()) {
            auto m = it.next();
            if (m.capturedStart() != pos) break;
            times << m.captured(1).toDouble() * 60 + m.captured(2).toDouble();
            pos = m.capturedEnd();
        }
        QString text = line.mid(pos);
        text.remove(QRegularExpression(QStringLiteral("<\\d+:\\d+(?:\\.\\d+)?>"))); // word timings
        text = text.trimmed();
        for (double t : times) out.append({t, t, text});
    }
    std::stable_sort(out.begin(), out.end(), [](auto &a, auto &b) { return a.start < b.start; });
    for (qsizetype i = 0; i < out.size(); ++i)
        out[i].end = i + 1 < out.size() ? out[i + 1].start : std::max(out[i].start + 5, lastEnd);
    out.removeIf([](const SubLine &l) { return l.text.isEmpty(); });
    return out;
}

QList<SubLine> parse(const QString &name, const QString &src)
{
    const QString ext = name.section(u'.', -1).toLower();
    if (ext == u"ass" || ext == u"ssa") return parseAss(src);
    if (ext == u"vtt") return parseVtt(src);
    if (ext == u"lrc") return parseLrc(src);
    return parseSrt(src);
}

QString readText(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray data = f.readAll();
    if (data.startsWith("\xff\xfe") || data.startsWith("\xfe\xff")) {
        QStringDecoder dec(data.startsWith("\xff\xfe") ? QStringDecoder::Utf16LE : QStringDecoder::Utf16BE);
        return QString(dec(data.mid(2)));
    }
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString s = utf8(data);
    if (!utf8.hasError()) return s;
    QStringDecoder sjis("Shift_JIS");
    if (sjis.isValid()) {
        QString t = sjis(data);
        if (!sjis.hasError()) return t;
    }
    return QString::fromUtf8(data);
}

QList<SubLine> load(const QString &path) { return normalize(parse(path, readText(path))); }

QList<SubLine> normalize(QList<SubLine> lines)
{
    static const QRegularExpression halfKana(QStringLiteral("[\\x{FF61}-\\x{FF9F}]+"));
    static const QRegularExpression music(QStringLiteral("^[♪♫♬〜]+$"));
    QList<SubLine> sorted;
    for (SubLine l : lines) {
        QString t;
        qsizetype last = 0;
        auto it = halfKana.globalMatch(l.text);
        while (it.hasNext()) {
            auto m = it.next();
            t += l.text.mid(last, m.capturedStart() - last) + m.captured().normalized(QString::NormalizationForm_KC);
            last = m.capturedEnd();
        }
        l.text = t + l.text.mid(last);
        const QString tr = l.text.trimmed();
        if (tr.isEmpty() || music.match(tr).hasMatch()) continue;
        sorted.append(l);
    }
    std::stable_sort(sorted.begin(), sorted.end(), [](auto &a, auto &b) { return a.start < b.start || (a.start == b.start && a.end < b.end); });
    QSet<qsizetype> absorbed;
    QList<SubLine> alike;
    for (qsizetype i = 0; i < sorted.size(); ++i) {
        if (absorbed.contains(i)) continue;
        double end = sorted[i].end;
        for (qsizetype j = i + 1; j <= i + 5 && j < sorted.size(); ++j) {
            if (!absorbed.contains(j) && sorted[j].text == sorted[i].text && std::abs(sorted[j].start - sorted[i].end) <= 5) {
                absorbed.insert(j);
                end = sorted[j].end;
            }
        }
        alike.append({sorted[i].start, end, sorted[i].text});
    }
    static const QStringList delims = {".", "?", "!", ":", "--", "。", "？", "！", "："};
    static const QStringList pairs = {"()", "[]", "（）", "【】"};
    auto endsSentence = [](const QString &text) {
        const QString t = text.trimmed();
        if (t.isEmpty()) return false;
        for (const QString &d : delims)
            if (t.endsWith(d)) return true;
        for (const QString &p : pairs)
            if (t.startsWith(p[0]) && t.endsWith(p[1])) return true;
        return false;
    };
    QList<SubLine> joined;
    for (const SubLine &l : alike) {
        if (!joined.isEmpty() && joined.last().start == l.start && joined.last().end == l.end && !endsSentence(joined.last().text))
            joined.last().text += u' ' + l.text;
        else
            joined.append(l);
    }
    int cased = 0, caps = 0;
    for (const SubLine &l : joined) {
        if (l.text == l.text.toLower()) continue;
        ++cased;
        if (l.text == l.text.toUpper()) ++caps;
    }
    if (cased && double(caps) / joined.size() > 0.7)
        for (SubLine &l : joined) l.text = l.text.left(1).toUpper() + l.text.mid(1).toLower();
    return joined;
}

int lineAt(const QList<SubLine> &lines, double time)
{
    const int best = lineBefore(lines, time);
    return best >= 0 && time < lines[best].end ? best : -1;
}

int lineBefore(const QList<SubLine> &lines, double time)
{
    int lo = 0, hi = int(lines.size()) - 1, best = -1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        if (lines[mid].start <= time) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return best;
}

QString tidyLine(const QString &text, bool dropAsides)
{
    static const QList<QRegularExpression> bracketed = [] {
        QList<QRegularExpression> out;
        const char16_t pairs[][2] = {{u'(', u')'}, {u'[', u']'}, {u'{', u'}'}, {u'＜', u'＞'}, {u'（', u'）'},
                                     {u'【', u'】'}, {u'〈', u'〉'}, {u'〖', u'〗'}, {u'〔', u'〕'}};
        for (auto &p : pairs) {
            const QString a = QRegularExpression::escape(QString(QChar(p[0]))), b = QRegularExpression::escape(QString(QChar(p[1])));
            out << QRegularExpression(a + u"[^" + b + u"]*" + b);
        }
        return out;
    }();
    static const QRegularExpression dash(QStringLiteral("^-\\s*(?=[^-])"));
    static const QRegularExpression spaces(QStringLiteral("[ \\t\\x{3000}]+"));
    QStringList rows;
    for (QString r : text.split(QRegularExpression(QStringLiteral("\\n+")))) rows << r.replace(dash, QString());
    QStringList bare;
    for (QString r : rows) {
        for (const auto &re : bracketed) r.replace(re, QString());
        bare << r.replace(spaces, QStringLiteral(" ")).trimmed();
    }
    const bool allAside = std::all_of(bare.begin(), bare.end(), [](auto &r) { return r.isEmpty(); });
    if (!dropAsides && allAside) {
        QStringList keep;
        for (const QString &r : rows)
            if (!r.trimmed().isEmpty()) keep << r.trimmed();
        return keep.join(u'\n');
    }
    bare.removeAll(QString());
    return bare.join(u'\n');
}

QString overlapText(const SubLine &line, const QList<SubLine> &others, double shift, double share)
{
    QStringList out;
    for (const SubLine &o : others) {
        const double start = o.start + shift, end = o.end + shift;
        if (end <= line.start) continue;
        if (start >= line.end) break;
        const double len = end - start;
        if (len > 0 && (std::min(line.end, end) - std::max(line.start, start)) / len >= share)
            out << o.text.simplified();
    }
    out.removeAll(QString());
    return out.join(u' ');
}

QString toSrt(const QList<SubLine> &lines)
{
    auto ts = [](double t) {
        const qint64 ms = qint64(std::llround(std::max(0.0, t) * 1000));
        return QStringLiteral("%1:%2:%3,%4")
            .arg(ms / 3600000, 2, 10, QChar(u'0'))
            .arg(ms / 60000 % 60, 2, 10, QChar(u'0'))
            .arg(ms / 1000 % 60, 2, 10, QChar(u'0'))
            .arg(ms % 1000, 3, 10, QChar(u'0'));
    };
    QString out;
    for (qsizetype i = 0; i < lines.size(); ++i)
        out += QStringLiteral("%1\n%2 --> %3\n%4\n\n").arg(i + 1).arg(ts(lines[i].start), ts(lines[i].end), lines[i].text);
    return out;
}

bool looksJapanese(const QList<SubLine> &lines)
{
    int jp = 0, n = 0;
    for (qsizetype i = 0; i < lines.size(); i += std::max<qsizetype>(1, lines.size() / 200)) {
        ++n;
        if (jp::hasJapanese(lines[i].text)) ++jp;
    }
    return n && jp * 2 > n;
}

} // namespace subs
