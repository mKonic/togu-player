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

QList<Subtitle> fetchSubtitles(const QString &url, QString *error, QString *title)
{
    const QString id = videoId(url);
    const QString dir = media::mediaCacheDir() + QStringLiteral("/youtube/") + id;
    QDir().mkpath(dir);
    const QString infoPath = dir + QStringLiteral("/info.json");
    QJsonObject info;
    // Fresh for a day; subtitles rarely change after upload.
    if (QFileInfo(infoPath).exists() && QFileInfo(infoPath).lastModified().secsTo(QDateTime::currentDateTime()) < 86400) {
        QFile f(infoPath);
        if (f.open(QIODevice::ReadOnly)) info = QJsonDocument::fromJson(f.readAll()).object();
    }
    if (info.isEmpty()) {
        QByteArray out;
        QString err;
        // Japanese only here: YouTube's machine-translated tracks ("en" from Japanese audio) get refused
        // with 429 and are poor anyway. English is fetched below when it's a real track.
        const QStringList args{"-j", "--no-simulate", "--skip-download", "--write-subs", "--write-auto-subs",
                               "--sub-langs", "ja,ja-orig", "--sub-format", "json3",
                               "-o", dir + QStringLiteral("/%(id)s.%(ext)s"), url};
        runYtdlp(args, &out, &err);
        info = QJsonDocument::fromJson(out).object();
        if (info.isEmpty()) {
            if (error) *error = err.isEmpty() ? QObject::tr("YouTube didn't answer.") : err;
            return {};
        }
        if (info.value("subtitles").toObject().contains(QStringLiteral("en")))
            runYtdlp({"--skip-download", "--write-subs", "--sub-langs", "en", "--sub-format", "json3",
                      "-o", dir + QStringLiteral("/%(id)s.%(ext)s"), url}, nullptr, nullptr);
        // Only what Togu reads, so the cache stays small.
        QJsonObject keep;
        for (const char *k : {"id", "title", "channel", "channel_id", "duration", "subtitles", "language"}) keep.insert(QLatin1String(k), info.value(QLatin1String(k)));
        QJsonObject subs;
        const QJsonObject all = keep.value("subtitles").toObject();
        for (auto it = all.begin(); it != all.end(); ++it) subs.insert(it.key(), true);
        keep.insert("subtitles", subs);
        info = keep;
        QFile f(infoPath);
        if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(info).toJson(QJsonDocument::Compact));
    }
    if (title) *title = info.value("title").toString();
    const QJsonObject manual = info.value("subtitles").toObject();
    QList<Subtitle> out;
    auto add = [&](const QString &lang, const QString &label, bool automatic) {
        const QString path = dir + u'/' + id + u'.' + lang + QStringLiteral(".json3");
        if (QFileInfo(path).size() > 0) out.append({path, lang.startsWith(u"ja") ? QStringLiteral("ja") : lang, label, automatic});
    };
    // yt-dlp writes "ja" from the hand-made track when there is one, else from the automatic one.
    if (manual.contains(QStringLiteral("ja"))) add(QStringLiteral("ja"), QObject::tr("Japanese"), false);
    else if (QFileInfo(dir + u'/' + id + QStringLiteral(".ja-orig.json3")).size() > 0) add(QStringLiteral("ja-orig"), QObject::tr("Japanese · automatic"), true);
    else add(QStringLiteral("ja"), QObject::tr("Japanese · automatic"), true);
    if (manual.contains(QStringLiteral("en"))) add(QStringLiteral("en"), QObject::tr("English"), false);
    if (out.isEmpty() && error) *error = QObject::tr("This video has no Japanese or English subtitles.");
    return out;
}

Streams streams(const QString &url)
{
    static QMutex mutex;
    static QHash<QString, std::pair<qint64, Streams>> cache;
    const QString id = videoId(url);
    {
        QMutexLocker lock(&mutex);
        const auto it = cache.constFind(id);
        // Stream URLs expire after about six hours.
        if (it != cache.constEnd() && QDateTime::currentSecsSinceEpoch() - it->first < 4 * 3600) return it->second;
    }
    QByteArray out;
    if (!runYtdlp({"-g", "-f", kFormat, url}, &out, nullptr)) return {};
    const QStringList lines = QString::fromUtf8(out).trimmed().split(u'\n', Qt::SkipEmptyParts);
    Streams s;
    if (lines.size() >= 2) {
        s.video = lines[0];
        s.audio = lines[1];
    } else if (lines.size() == 1) {
        s.video = s.audio = lines[0];
    }
    QMutexLocker lock(&mutex);
    cache.insert(id, {QDateTime::currentSecsSinceEpoch(), s});
    return s;
}

} // namespace yt
