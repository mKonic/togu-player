#pragma once
// Reading series, season and episode out of file names, and pairing videos with
// subtitle files.

#include <QList>
#include <QString>
#include <QStringList>

namespace names {

struct Parsed {
    QString title;      // cleaned series title, may be empty
    int season = 0;     // 0 when not stated
    double episode = -1; // -1 when not found
    bool special = false; // OVA, SP, NCOP...
    QString group;      // release group in leading brackets
};

Parsed parse(const QString &fileName);
// Title for display and grouping from a folder name ("Show (2019) [BD 1080p]" -> "Show").
QString cleanTitle(const QString &name);
QString normalizedKey(const QString &title); // for comparing titles loosely

bool isVideo(const QString &path);
bool isAudio(const QString &path);
bool isSubtitle(const QString &path);
bool isImage(const QString &path);

enum class Lang { Unknown, Japanese, English, Other };
// Language hinted by a subtitle file name (.ja.srt, [jpn], 日本語...).
Lang subtitleLang(const QString &fileName);

struct SubMatch {
    QString path;
    Lang lang = Lang::Unknown;
    int score = 0; // higher is a closer match
};
// Subtitle files that belong to `videoPath`: same folder, subtitle subfolders, by name or episode.
// `siblingsVideos` is how many videos share the folder (episode matching only needed then).
QList<SubMatch> subtitlesFor(const QString &videoPath);

} // namespace names
