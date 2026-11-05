#pragma once
// YouTube through yt-dlp: subtitles for a video and stream URLs for cutting card media.

#include <QList>
#include <QString>

namespace yt {

bool isYouTube(const QString &url);
QString videoId(const QString &url);

struct Subtitle {
    QString path, lang, label;
    bool automatic = false;
};
// Japanese (by hand, else YouTube's automatic one for Japanese audio) and English, as json3 files in the
// cache. Blocking; run it on a worker thread.
QList<Subtitle> fetchSubtitles(const QString &url, QString *error = nullptr, QString *title = nullptr);

// Direct media URLs for ffmpeg (audio, video), cached for a few hours. Blocking.
struct Streams {
    QString audio, video;
};
Streams streams(const QString &url);

} // namespace yt
