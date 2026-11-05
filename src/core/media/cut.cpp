#include "cut.h"

#include "core/youtube/ytdlp.h"

#include "core/util/db.h"
#include "probe.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRandomGenerator>
#include <QTemporaryFile>

namespace media {

QString mediaCacheDir()
{
    const QString d = db::cacheDir() + QStringLiteral("/cardmedia");
    QDir().mkpath(d);
    return d;
}

namespace {

QString outName(const QString &ext)
{
    return mediaCacheDir() + QStringLiteral("/%1-%2.%3").arg(QDateTime::currentMSecsSinceEpoch()).arg(QRandomGenerator::global()->bounded(100000)).arg(ext);
}

bool run(const QStringList &args, int timeoutMs = 120000)
{
    QProcess p;
    p.start(ffmpeg(), QStringList{"-v", "error", "-y", "-nostdin"} + args);
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        return false;
    }
    return p.exitCode() == 0;
}

QString ts(double t) { return QString::number(std::max(0.0, t), 'f', 3); }

QStringList audioCodec(const QString &format)
{
    if (format == u"opus") return {"-c:a", "libopus", "-b:a", "64k", "-application", "voip"};
    return {"-c:a", "libmp3lame", "-q:a", "4"};
}

} // namespace

QString coverArt(const QString &path)
{
    const QString dir = db::cacheDir() + QStringLiteral("/covers");
    QDir().mkpath(dir);
    const QFileInfo fi(path);
    const QString out = dir + u'/' + QString::number(qHash(path + QString::number(fi.size()))) + QStringLiteral(".jpg");
    if (QFileInfo(out).size() > 0) return out;
    // Audio files keep their cover as an attached-picture video stream.
    if (run({"-i", path, "-an", "-map", "0:v:0", "-frames:v", "1", "-vf", "scale='min(800,iw)':-2", out}, 30000) && QFileInfo(out).size() > 0) return out;
    QFile::remove(out);
    return {};
}

// A YouTube video is cut from its direct stream URLs, sent with the user agent they were issued to;
// anything else is a file. Each returns the ffmpeg input arguments.
static QStringList audioInput(const QString &path)
{
    if (!yt::isYouTube(path)) return {"-i", path};
    const yt::Streams st = yt::streams(path);
    return {"-user_agent", st.userAgent, "-i", st.audio.isEmpty() ? st.video : st.audio};
}
static QStringList videoInput(const QString &path)
{
    if (!yt::isYouTube(path)) return {"-i", path};
    const yt::Streams st = yt::streams(path);
    return {"-user_agent", st.userAgent, "-i", st.video};
}

QString cutAudio(const AudioCut &c)
{
    const QString ext = c.format == u"opus" ? QStringLiteral("ogg") : QStringLiteral("mp3");
    const QString out = outName(ext);
    QStringList args{"-ss", ts(c.start), "-t", ts(c.end - std::max(0.0, c.start))};
    args << audioInput(c.path) << QStringList{
                     "-map", c.audioTrack >= 0 && !yt::isYouTube(c.path) ? QStringLiteral("0:a:%1").arg(c.audioTrack) : QStringLiteral("0:a:0"), "-vn", "-sn", "-ac", "1"};
    // Short fades keep the cut from clicking.
    const double dur = c.end - c.start;
    args << "-af" << QStringLiteral("afade=t=in:d=0.04,afade=t=out:st=%1:d=0.08").arg(ts(std::max(0.0, dur - 0.08)));
    args << audioCodec(c.format) << out;
    return run(args) && QFileInfo(out).size() > 0 ? out : QString();
}

QString stillFrame(const QString &path, double t, int height, const QString &format)
{
    const QString out = outName(format == u"jpg" ? QStringLiteral("jpg") : QStringLiteral("webp"));
    QStringList args{"-ss", ts(t)};
    args << videoInput(path) << QStringList{"-frames:v", "1", "-vf", QStringLiteral("scale=-2:%1").arg(height)};
    if (format == u"jpg") args << "-q:v" << "3";
    else args << "-c:v" << "libwebp" << "-quality" << "82";
    args << out;
    return run(args) && QFileInfo(out).size() > 0 ? out : QString();
}

QString animatedImage(const QString &path, double start, double end, int height, double maxSeconds)
{
    const QString out = outName(QStringLiteral("webp"));
    const double dur = std::min(end - start, maxSeconds);
    return run(QStringList{"-ss", ts(start), "-t", ts(dur)} + videoInput(path) + QStringList{"-an", "-sn",
                "-vf", QStringLiteral("fps=12,scale=-2:%1:flags=lanczos").arg(std::min(height, 360)),
                "-c:v", "libwebp_anim", "-loop", "0", "-quality", "55", "-compression_level", "4", out}) && QFileInfo(out).size() > 0 ? out : QString();
}

QString videoClip(const QString &path, double start, double end, int height, int audioTrack, double maxSeconds)
{
    const QString out = outName(QStringLiteral("webm"));
    const double dur = std::min(end - start, maxSeconds);
    QStringList in = QStringList{"-ss", ts(start), "-t", ts(dur)} + videoInput(path);
    QString audioMap = audioTrack >= 0 ? QStringLiteral("0:a:%1").arg(audioTrack) : QStringLiteral("0:a:0?");
    if (yt::isYouTube(path) && !yt::streams(path).audio.isEmpty()) {
        // YouTube serves picture and sound as separate streams.
        in << "-ss" << ts(start) << "-t" << ts(dur) << audioInput(path);
        audioMap = QStringLiteral("1:a:0");
    }
    return run(in + QStringList{"-sn",
                "-map", "0:v:0", "-map", audioMap,
                "-vf", QStringLiteral("scale=-2:%1").arg(std::min(height, 480)),
                "-c:v", "libvpx-vp9", "-b:v", "0", "-crf", "38", "-deadline", "realtime", "-cpu-used", "8", "-row-mt", "1",
                "-c:a", "libopus", "-b:a", "64k", out}) && QFileInfo(out).size() > 0 ? out : QString();
}

QString condensedAudio(const QString &path, const QList<std::pair<double, double>> &spans, int audioTrack,
                       const QString &format, const QString &outPath, double padding)
{
    if (spans.isEmpty()) return {};
    // Merge spans that touch once padded, then select them with aselect.
    QList<std::pair<double, double>> merged;
    for (auto [a, b] : spans) {
        a = std::max(0.0, a - padding);
        b += padding;
        if (!merged.isEmpty() && a <= merged.last().second) merged.last().second = std::max(merged.last().second, b);
        else merged.append({a, b});
    }
    QStringList parts;
    for (auto [a, b] : merged) parts << QStringLiteral("between(t,%1,%2)").arg(ts(a), ts(b));
    // Long filter expressions go through a script file.
    QTemporaryFile script(QDir::tempPath() + QStringLiteral("/togu-condense-XXXXXX.txt"));
    if (!script.open()) return {};
    script.write(QStringLiteral("aselect='%1',asetpts=N/SR/TB").arg(parts.join(u'+')).toUtf8());
    script.flush();
    QStringList args{"-i", path, "-map", audioTrack >= 0 ? QStringLiteral("0:a:%1").arg(audioTrack) : QStringLiteral("0:a:0"),
                     "-vn", "-sn", "-/filter:a", script.fileName()};
    args << audioCodec(format) << outPath;
    return run(args, 600000) && QFileInfo(outPath).size() > 0 ? outPath : QString();
}

} // namespace media
