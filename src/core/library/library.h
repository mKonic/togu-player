#pragma once
// The media library: folders, series, items, the subtitle line index and the
// comprehension scores built from it.

#include <QList>
#include <QString>
#include <QVariant>

#include <atomic>
#include <functional>

namespace library {

struct Root {
    int id = 0;
    QString path, kind; // kind: video, audio, book, manga
};

QList<Root> roots(const QString &kind = {});
int addRoot(const QString &path, const QString &kind);
void removeRoot(int id);

struct ScanResult {
    int added = 0, removed = 0, series = 0;
};
// Walks a root and brings series and items up to date. Safe to run on a worker thread.
ScanResult scan(const Root &root, const std::atomic_bool *cancel = nullptr);

// Subtitle index: lines of the item's Japanese subtitle, tokenized. Returns lines indexed.
int indexItem(int itemId);
// Items whose subtitles are not indexed yet (or changed).
QList<int> itemsToIndex();
// Comprehension from the indexed lines with the current word statuses.
void scoreItems(const QList<int> &itemIds = {});
void scoreSeries();

struct LineHit {
    int item = 0, series = 0;
    QString path, seriesTitle, itemTitle, text;
    double start = 0, end = 0, episode = -1;
};
QList<LineHit> linesWithWord(const QString &key, int limit = 30);

// The Japanese subtitle file chosen for an item: its saved choice, a sidecar, or an embedded text track.
QString subtitleFileFor(int itemId);

// Encoded words of a line for scoring: keys separated by spaces, sentence ends as "。".
QString encodeLine(const QString &text);

} // namespace library
