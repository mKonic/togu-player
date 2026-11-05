#pragma once
// Optional online metadata: AniList for anime, TMDB (with the user's key) for the rest.

#include <QList>
#include <QString>

namespace metadata {

struct Candidate {
    QString source; // anilist, tmdb
    QString id, title, titleNative, titleEnglish, poster, banner, description;
    int year = 0;
    QStringList synonyms;
};

// Blocking; call from a worker thread.
QList<Candidate> search(const QString &title, const QString &tmdbKey = {});
// Downloads the images and stores the match. `manual` marks a hand-picked match, kept on rescans.
bool apply(int seriesId, const Candidate &c, bool manual = false);
// Finds and applies a match when the titles clearly agree. Returns true when one was applied.
bool autoMatch(int seriesId, const QString &tmdbKey = {});
// How alike two titles are, 0..1.
double similarity(const QString &a, const QString &b);

} // namespace metadata
