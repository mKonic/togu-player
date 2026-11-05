#include "ytdlp.h"

#include "core/media/cut.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QProcess>
#include <QRegularExpression>
#include <QUrlQuery>

namespace yt {

namespace {

// yt-dlp prefers the original audio track over YouTube's auto-dubs with this.
const QString kFormat = QStringLiteral("bv*[height<=?1080]+(ba[format_note*=original]/ba[language^=ja]/ba)/b");

bool runYtdlp(const QStringList &args, QByteArray *out, QString *error, int timeoutMs = 120000)
{
    QProcess p;
    p.start(QStringLiteral("yt-dlp"), QStringList{"--no-warnings", "--force-ipv4", "--socket-timeout", "20"} + args);
    if (!p.waitForStarted(5000)) {
        if (error) *error = QObject::tr("yt-dlp is not installed.");
        return false;
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        if (error) *error = QObject::tr("yt-dlp took too long.");
        return false;
    }
    if (out) *out = p.readAllStandardOutput();
    if (p.exitCode() != 0) {
        if (error) *error = QString::fromUtf8(p.readAllStandardError()).trimmed().split(u'\n').last();
        return false;
    }
    return true;
}

} // namespace

bool isYouTube(const QString &url)
{
    static const QRegularExpression re(QStringLiteral("^https?://(www\\.|m\\.|music\\.)?(youtube\\.com|youtu\\.be)/"));
    return re.match(url).hasMatch();
}

QString videoId(const QString &url)
{
    const QUrl u(url);
    if (u.host().endsWith(u"youtu.be")) return u.path().mid(1);
    const QString v = QUrlQuery(u).queryItemValue(QStringLiteral("v"));
    if (!v.isEmpty()) return v;
    static const QRegularExpression re(QStringLiteral("/(shorts|live|embed)/([A-Za-z0-9_-]{11})"));
    const auto m = re.match(u.path());
    return m.hasMatch() ? m.captured(2) : QString();
}

namespace {
QMutex g_mutex;
QHash<QString, std::pair<qint64, Streams>> g_streams;

void remember(const QString &id, const Streams &st)
{
    QMutexLocker lock(&g_mutex);
    g_streams.insert(id, {QDateTime::currentSecsSinceEpoch(), st});
}
} // namespace

Resolved resolve(const QString &url)
{
    Resolved r;
    const QString id = videoId(url);
    const QString dir = media::mediaCacheDir() + QStringLiteral("/youtube/") + id;
    QDir().mkpath(dir);
    QByteArray out;
    QString err;
    // Japanese only: YouTube's machine-translated tracks ("en" from Japanese audio) get refused with 429
    // and are poor anyway. English is fetched below when it's a real track.
    const QStringList args{"-j", "--no-simulate", "--skip-download", "-f", kFormat,
                           "--write-subs", "--write-auto-subs", "--sub-langs", "ja,ja-orig", "--sub-format", "json3",
                           "-o", dir + QStringLiteral("/%(id)s.%(ext)s"), url};
    runYtdlp(args, &out, &err);
    const QJsonObject info = QJsonDocument::fromJson(out).object();
    if (info.isEmpty()) {
        r.error = err.isEmpty() ? QObject::tr("YouTube didn't answer.") : err;
        return r;
    }
    r.title = info.value("title").toString();

    const QJsonArray formats = info.value("requested_formats").toArray();
    auto ua = [](const QJsonObject &f) { return f.value("http_headers").toObject().value("User-Agent").toString(); };
    for (const QJsonValue &v : formats) {
        const QJsonObject f = v.toObject();
        if (f.value("vcodec").toString() != u"none") r.streams.video = f.value("url").toString();
        else r.streams.audio = f.value("url").toString();
        r.streams.userAgent = ua(f);
    }
    if (formats.isEmpty() && info.contains("url")) {
        r.streams.video = info.value("url").toString();
        r.streams.userAgent = ua(info);
    }
    if (r.streams.valid()) remember(id, r.streams);

    const QJsonObject manual = info.value("subtitles").toObject();
    if (manual.contains(QStringLiteral("en")) && !QFileInfo::exists(dir + u'/' + id + QStringLiteral(".en.json3")))
        runYtdlp({"--skip-download", "--write-subs", "--sub-langs", "en", "--sub-format", "json3",
                  "-o", dir + QStringLiteral("/%(id)s.%(ext)s"), url}, nullptr, nullptr);
    auto add = [&](const QString &lang, const QString &label, bool automatic) {
        const QString path = dir + u'/' + id + u'.' + lang + QStringLiteral(".json3");
        if (QFileInfo(path).size() > 0) r.subtitles.append({path, lang.startsWith(u"ja") ? QStringLiteral("ja") : lang, label, automatic});
    };
    // yt-dlp writes "ja" from the hand-made track when there is one, else from the automatic one.
    if (manual.contains(QStringLiteral("ja"))) add(QStringLiteral("ja"), QObject::tr("Japanese"), false);
    else if (QFileInfo(dir + u'/' + id + QStringLiteral(".ja-orig.json3")).size() > 0) add(QStringLiteral("ja-orig"), QObject::tr("Japanese · automatic"), true);
    else add(QStringLiteral("ja"), QObject::tr("Japanese · automatic"), true);
    if (manual.contains(QStringLiteral("en"))) add(QStringLiteral("en"), QObject::tr("English"), false);
    return r;
}

Streams streams(const QString &url)
{
    const QString id = videoId(url);
    {
        QMutexLocker lock(&g_mutex);
        const auto it = g_streams.constFind(id);
        // Stream URLs expire after about six hours.
        if (it != g_streams.constEnd() && QDateTime::currentSecsSinceEpoch() - it->first < 4 * 3600) return it->second;
    }
    QByteArray out;
    if (!runYtdlp({"-j", "-f", kFormat, url}, &out, nullptr)) return {};
    const QJsonObject info = QJsonDocument::fromJson(out).object();
    Streams st;
    for (const QJsonValue &v : info.value("requested_formats").toArray()) {
        const QJsonObject f = v.toObject();
        if (f.value("vcodec").toString() != u"none") st.video = f.value("url").toString();
        else st.audio = f.value("url").toString();
        st.userAgent = f.value("http_headers").toObject().value("User-Agent").toString();
    }
    if (st.valid()) remember(id, st);
    return st;
}

} // namespace yt
