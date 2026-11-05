#pragma once
// YouTube through yt-dlp: one call per video gives its title, subtitles and stream URLs.

#include <QList>
#include <QString>

namespace yt {

bool isYouTube(const QString &url);
QString videoId(const QString &url);

struct Subtitle {
    QString path, lang, label;
    bool automatic = false;
};

// Direct media URLs. YouTube ties them to the user agent that asked for them (yt-dlp picks a new browser
// version per run), so whoever fetches them must send `userAgent`.
struct Streams {
    QString video, audio, userAgent;
    bool valid() const { return !video.isEmpty(); }
};

struct Resolved {
    QString title, error;
    QList<Subtitle> subtitles; // Japanese (by hand, else automatic for Japanese audio), English by hand
    Streams streams;
};
// Blocking (several seconds); run it on a worker thread. Stream URLs are kept for later calls.
Resolved resolve(const QString &url);

// The streams from the last resolve of this video, re-resolved when missing or old. Blocking.
Streams streams(const QString &url);

} // namespace yt
