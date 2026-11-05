#include "probe.h"

#include "core/util/db.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace media {

QString ffmpeg()
{
    static QString p = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    return p;
}

QString ffprobe()
{
    static QString p = QStandardPaths::findExecutable(QStringLiteral("ffprobe"));
    return p;
}

bool Stream::textSub() const
{
    static const QStringList text = {"ass", "ssa", "subrip", "srt", "webvtt", "mov_text", "text"};
    return type == u"subtitle" && text.contains(codec);
}

bool isJapaneseLang(const QString &lang, const QString &title)
{
    const QString l = lang.toLower();
    if (l == u"ja" || l == u"jpn" || l == u"jp" || l == u"jap") return true;
    const QString t = title.toLower();
    return t.contains(u"japanese") || t.contains(u"日本語") || t.contains(u"jpn");
}

Probe probe(const QString &path)
{
    Probe p;
    QProcess proc;
    proc.start(ffprobe(), {"-v", "error", "-print_format", "json", "-show_streams", "-show_format", path});
    if (!proc.waitForFinished(30000)) {
        p.error = QStringLiteral("ffprobe timed out");
        proc.kill();
        return p;
    }
    if (proc.exitCode() != 0) {
        p.error = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        return p;
    }
    const QJsonObject root = QJsonDocument::fromJson(proc.readAllStandardOutput()).object();
    const QJsonObject fmt = root.value("format").toObject();
    p.duration = fmt.value("duration").toString().toDouble();
    p.title = fmt.value("tags").toObject().value("title").toString();
    QHash<QString, int> perType;
    for (const QJsonValue &v : root.value("streams").toArray()) {
        const QJsonObject o = v.toObject();
        Stream s;
        s.index = o.value("index").toInt();
        s.type = o.value("codec_type").toString();
        s.codec = o.value("codec_name").toString();
        const QJsonObject tags = o.value("tags").toObject();
        s.lang = tags.value("language").toString();
        s.title = tags.value("title").toString();
        const QJsonObject disp = o.value("disposition").toObject();
        s.isDefault = disp.value("default").toInt();
        s.forced = disp.value("forced").toInt();
        // Cover art counts as a video stream in ffprobe, not as one ffmpeg maps with 0:v.
        if (s.type == u"video" && disp.value("attached_pic").toInt()) s.type = QStringLiteral("cover");
        s.typeIndex = perType[s.type]++;
        p.streams.append(s);
    }
    return p;
}

QString extractSubtitle(const QString &path, const Stream &s)
{
    const QFileInfo fi(path);
    const QByteArray id = QCryptographicHash::hash((path + QString::number(fi.size()) + QString::number(fi.lastModified().toSecsSinceEpoch()) +
                                                    u'#' + QString::number(s.index)).toUtf8(),
                                                   QCryptographicHash::Sha1).toHex().left(20);
    const bool ass = s.codec == u"ass" || s.codec == u"ssa";
    const QString dir = db::cacheDir() + QStringLiteral("/subs");
    QDir().mkpath(dir);
    const QString out = dir + u'/' + QString::fromLatin1(id) + (ass ? QStringLiteral(".ass") : QStringLiteral(".srt"));
    if (QFileInfo(out).size() > 0) return out;
    QProcess proc;
    proc.start(ffmpeg(), {"-v", "error", "-y", "-i", path, "-map", QStringLiteral("0:s:%1").arg(s.typeIndex),
                          "-c:s", ass ? "copy" : "srt", out});
    if (!proc.waitForFinished(120000) || proc.exitCode() != 0) {
        QFile::remove(out);
        return {};
    }
    return out;
}

QByteArray decodePcm16k(const QString &path, int audioTypeIndex)
{
    QProcess proc;
    QStringList args = {"-v", "error", "-i", path};
    if (audioTypeIndex >= 0) args << "-map" << QStringLiteral("0:a:%1").arg(audioTypeIndex);
    else args << "-map" << "0:a:0";
    args << "-vn" << "-sn" << "-ac" << "1" << "-ar" << "16000" << "-f" << "s16le" << "-";
    proc.start(ffmpeg(), args);
    QByteArray out;
    while (proc.state() != QProcess::NotRunning || proc.bytesAvailable()) {
        proc.waitForReadyRead(1000);
        out += proc.readAllStandardOutput();
        if (proc.state() == QProcess::NotRunning && !proc.bytesAvailable()) break;
    }
    out += proc.readAllStandardOutput();
    return out;
}

} // namespace media
