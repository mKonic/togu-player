#pragma once
// Card media cut straight from the file with ffmpeg.

#include <QString>

namespace media {

struct AudioCut {
    QString path;
    double start = 0, end = 0;  // seconds, already shifted to the video's clock
    int audioTrack = -1;        // ffmpeg 0:a:N, -1 for the first
    QString format = QStringLiteral("mp3"); // mp3, opus
};
// Each returns the written file in the media cache, or an empty string.
QString cutAudio(const AudioCut &c);
// image format: webp, jpg
QString stillFrame(const QString &path, double t, int height, const QString &format = QStringLiteral("webp"));
QString animatedImage(const QString &path, double start, double end, int height, double maxSeconds);
QString videoClip(const QString &path, double start, double end, int height, int audioTrack, double maxSeconds);
// Condensed audio: only the given spans, joined.
QString condensedAudio(const QString &path, const QList<std::pair<double, double>> &spans, int audioTrack,
                       const QString &format, const QString &outPath, double padding = 0.2);

QString mediaCacheDir();

} // namespace media
