#pragma once
// ffprobe/ffmpeg helpers: stream lists and pulling text subtitle tracks out of a file.

#include <QList>
#include <QString>

namespace media {

struct Stream {
    int index = 0;     // container index
    int typeIndex = 0; // index among streams of its type (ffmpeg's 0:s:N)
    QString type;      // video, audio, subtitle, attachment
    QString codec, lang, title;
    bool isDefault = false, forced = false;
    bool textSub() const;
};

struct Probe {
    double duration = 0;
    QList<Stream> streams;
    QString title;
    QString error;
};

Probe probe(const QString &path);
// Extracts subtitle stream `typeIndex` to the cache as .ass (ASS/SSA) or .srt; returns its path.
// Cached by file path + size + mtime, so a second call is instant.
QString extractSubtitle(const QString &path, const Stream &s);
// 16 kHz mono signed 16-bit PCM of one audio stream (-1: default), for voice detection.
QByteArray decodePcm16k(const QString &path, int audioTypeIndex = -1);

QString ffmpeg();
QString ffprobe();
bool isJapaneseLang(const QString &lang, const QString &title = {});

} // namespace media
