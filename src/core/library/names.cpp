#include "names.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace names {

namespace {

const QSet<QString> &videoExt()
{
    static const QSet<QString> s = {"mkv", "mp4", "m4v", "webm", "avi", "mov", "wmv", "flv", "ts", "m2ts", "mpg", "mpeg", "ogv", "3gp"};
    return s;
}
const QSet<QString> &audioExt()
{
    static const QSet<QString> s = {"mp3", "m4a", "m4b", "aac", "flac", "ogg", "opus", "wav", "wma", "aiff", "ape", "alac", "mka"};
    return s;
}
const QSet<QString> &subExt()
{
    static const QSet<QString> s = {"ass", "ssa", "srt", "vtt", "lrc", "json3"};
    return s;
}

QString ext(const QString &p) { return QFileInfo(p).suffix().toLower(); }

// Bracketed bits that are release noise rather than title.
bool isNoise(const QString &inner)
{
    static const QRegularExpression noise(QStringLiteral(
        "\\b(\\d{3,4}p|\\d{3,4}x\\d{3,4}|x26[45]|h\\.?26[45]|hevc|avc|aac|flac|opus|ac3|dts|eac3|truehd|bd(rip)?|blu-?ray|"
        "web(-?dl|rip)?|hdtv|dvd(rip)?|10.?bit|8.?bit|hi10p?|dual.?audio|multi.?subs?|raw|batch|complete|v\\d|"
        "[0-9a-f]{8}|remux|uncensored|eng|jpn|sub(bed|s)?|end|fin)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return noise.match(inner).hasMatch() || QRegularExpression(QStringLiteral("^\\d{4}$")).match(inner.trimmed()).hasMatch();
}

QString stripBrackets(QString s, QString *group = nullptr)
{
    static const QRegularExpression lead(QStringLiteral("^\\s*\\[([^\\]]*)\\]"));
    if (auto m = lead.match(s); m.hasMatch()) {
        if (group) *group = m.captured(1).trimmed();
        s = s.mid(m.capturedEnd());
    }
    static const QRegularExpression br(QStringLiteral("[\\[\\(【（]([^\\]\\)】）]*)[\\]\\)】）]"));
    QString out;
    qsizetype last = 0;
    auto it = br.globalMatch(s);
    while (it.hasNext()) {
        auto m = it.next();
        out += s.mid(last, m.capturedStart() - last);
        if (!isNoise(m.captured(1))) out += u' ' + m.captured(1) + u' ';
        last = m.capturedEnd();
    }
    return out + s.mid(last);
}

QString tidy(QString s)
{
    s.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    s = s.trimmed();
    while (!s.isEmpty() && QStringLiteral("-–_~:|.,").contains(s.back())) s.chop(1), s = s.trimmed();
    while (!s.isEmpty() && QStringLiteral("-–_~:|.,").contains(s.front())) s.remove(0, 1), s = s.trimmed();
    return s;
}

} // namespace

bool isVideo(const QString &p) { return videoExt().contains(ext(p)); }
bool isAudio(const QString &p) { return audioExt().contains(ext(p)); }
bool isSubtitle(const QString &p) { return subExt().contains(ext(p)); }
bool isImage(const QString &p)
{
    static const QSet<QString> s = {"jpg", "jpeg", "png", "webp", "avif", "gif", "bmp"};
    return s.contains(ext(p));
}

Parsed parse(const QString &fileName)
{
    Parsed p;
    QString s = QFileInfo(fileName).completeBaseName();
    if (s.isEmpty()) s = fileName;
    s = stripBrackets(s, &p.group);
    // Dots and underscores as separators when the name has no spaces.
    if (!s.contains(u' ')) s.replace(u'.', u' ');
    s.replace(u'_', u' ');

    static const QRegularExpression special(QStringLiteral("\\b(OVA|OAD|ONA|SP|Specials?|NCOP\\d*|NCED\\d*|Creditless|Movie|Extra|Bonus|PV|CM|Preview)\\b"),
                                            QRegularExpression::CaseInsensitiveOption);
    struct Pat {
        QRegularExpression re;
        int season, episode; // capture groups, 0 = none
    };
    static const QList<Pat> pats = {
        {QRegularExpression(QStringLiteral("\\bS(\\d{1,2})\\s*E(\\d{1,4}(?:\\.\\d)?)"), QRegularExpression::CaseInsensitiveOption), 1, 2},
        {QRegularExpression(QStringLiteral("\\b(\\d{1,2})x(\\d{2,3})\\b")), 1, 2},
        {QRegularExpression(QStringLiteral("Season\\s*(\\d{1,2})\\D{0,6}Ep(?:isode)?\\.?\\s*(\\d{1,4})"), QRegularExpression::CaseInsensitiveOption), 1, 2},
        {QRegularExpression(QStringLiteral("第\\s*(\\d{1,4})\\s*[話回]")), 0, 1},
        {QRegularExpression(QStringLiteral("\\bE(?:p(?:isode)?)?\\.?\\s*(\\d{1,4}(?:\\.\\d)?)(?:v\\d)?\\b"), QRegularExpression::CaseInsensitiveOption), 0, 1},
        {QRegularExpression(QStringLiteral("\\s[-–]\\s*(\\d{1,4}(?:\\.\\d)?)(?:v\\d)?(?=\\s|$)")), 0, 1},
        {QRegularExpression(QStringLiteral("#(\\d{1,4})\\b")), 0, 1},
        {QRegularExpression(QStringLiteral("(?:^|\\s)(\\d{1,4}(?:\\.\\d)?)(?:v\\d)?\\s*$")), 0, 1},
        {QRegularExpression(QStringLiteral("(?:^|\\s)(\\d{1,3})(?:v\\d)?(?=\\s)")), 0, 1},
    };
    qsizetype cut = -1;
    for (const Pat &pat : pats) {
        auto m = pat.re.match(s);
        if (!m.hasMatch()) continue;
        const double ep = m.captured(pat.episode).toDouble();
        // A four-digit number is usually a year, not an episode.
        if (ep >= 1900 && ep <= 2100 && !pat.season) continue;
        p.episode = ep;
        if (pat.season) p.season = m.captured(pat.season).toInt();
        cut = m.capturedStart();
        break;
    }
    if (auto m = special.match(s); m.hasMatch()) {
        p.special = true;
        if (cut < 0 || m.capturedStart() < cut) cut = m.capturedStart();
    }
    QString title = cut >= 0 ? s.left(cut) : s;
    static const QRegularExpression seasonInTitle(
        QStringLiteral("\\b(?:S(\\d{1,2})|Season\\s*(\\d{1,2})|(\\d)(?:st|nd|rd|th)\\s+Season|第(\\d)期)\\b"), QRegularExpression::CaseInsensitiveOption);
    if (auto m = seasonInTitle.match(title); m.hasMatch()) {
        for (int g = 1; g <= 4; ++g)
            if (!m.captured(g).isEmpty() && !p.season) p.season = m.captured(g).toInt();
        title = title.left(m.capturedStart()) + title.mid(m.capturedEnd());
    }
    p.title = tidy(title);
    return p;
}

QString cleanTitle(const QString &name)
{
    QString s = stripBrackets(name);
    if (!s.contains(u' ')) s.replace(u'.', u' ');
    s.replace(u'_', u' ');
    s.remove(QRegularExpression(QStringLiteral("\\b(19|20)\\d{2}\\b")));
    return tidy(s);
}

QString normalizedKey(const QString &title)
{
    QString s = title.toLower().normalized(QString::NormalizationForm_KC);
    s.remove(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]")));
    return s;
}

Lang subtitleLang(const QString &fileName)
{
    const QString n = QFileInfo(fileName).completeBaseName().toLower();
    static const QRegularExpression ja(QStringLiteral("(^|[.\\[\\(_\\s-])(ja|jp|jpn|jap|japanese)([.\\]\\)_\\s-]|$)|日本語|日語|和文"));
    static const QRegularExpression en(QStringLiteral("(^|[.\\[\\(_\\s-])(en|eng|english)([.\\]\\)_\\s-]|$)"));
    static const QRegularExpression other(QStringLiteral("(^|[.\\[\\(_\\s-])(zh|chs|cht|chi|sc|tc|es|spa|fr|fre|de|ger|pt|por|ru|rus|ko|kor|it|ita|ar|ara)([.\\]\\)_\\s-]|$)|简|繁"));
    if (ja.match(n).hasMatch()) return Lang::Japanese;
    if (en.match(n).hasMatch()) return Lang::English;
    if (other.match(n).hasMatch()) return Lang::Other;
    return Lang::Unknown;
}

QList<SubMatch> subtitlesFor(const QString &videoPath)
{
    const QFileInfo vf(videoPath);
    const QDir dir = vf.dir();
    const QString base = vf.completeBaseName();
    const Parsed vp = parse(vf.fileName());
    QStringList candidates;
    int videos = 0;
    for (const QFileInfo &f : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (isSubtitle(f.fileName())) candidates << f.absoluteFilePath();
        else if (isVideo(f.fileName())) ++videos;
    }
    static const QRegularExpression subDir(QStringLiteral("^(subs?|subtitles?|字幕|japanese|jp|jpn|ja|eng?|english|ass|srt)$"), QRegularExpression::CaseInsensitiveOption);
    for (const QFileInfo &d : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!subDir.match(d.fileName()).hasMatch()) continue;
        QDir sd(d.absoluteFilePath());
        for (const QFileInfo &f : sd.entryInfoList(QDir::Files))
            if (isSubtitle(f.fileName())) candidates << f.absoluteFilePath();
        // subs/<video name>/*.srt (common in packs)
        QDir nested(sd.filePath(base));
        if (nested.exists())
            for (const QFileInfo &f : nested.entryInfoList(QDir::Files))
                if (isSubtitle(f.fileName())) candidates << f.absoluteFilePath();
    }
    QList<SubMatch> out;
    for (const QString &c : candidates) {
        const QFileInfo cf(c);
        int score = 0;
        const QString cb = cf.completeBaseName();
        if (cb == base) score = 100;
        else if (cb.startsWith(base)) score = 90;
        else if (QFileInfo(cf.dir().path()).fileName() == base) score = 85;
        else {
            const Parsed sp = parse(cf.fileName());
            if (vp.episode >= 0 && sp.episode == vp.episode && sp.special == vp.special &&
                (!vp.season || !sp.season || vp.season == sp.season))
                score = 60;
            else if (videos <= 1 && vp.episode < 0)
                score = 30; // the only video here: any subtitle is probably its own
        }
        if (score) out.append({c, subtitleLang(cf.fileName()), score});
    }
    std::sort(out.begin(), out.end(), [](const SubMatch &a, const SubMatch &b) { return a.score > b.score; });
    return out;
}

} // namespace names
