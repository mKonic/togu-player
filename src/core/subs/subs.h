#pragma once
// Subtitle parsing and clean-up (ported from togu), plus the ASS split: dialogue goes
// to Togu's own layer, signs and typesetting stay with mpv.

#include <QList>
#include <QString>

struct SubLine {
    double start = 0; // seconds
    double end = 0;
    QString text;
};

namespace subs {

QList<SubLine> parseSrt(const QString &src);
QList<SubLine> parseVtt(const QString &src);
QList<SubLine> parseAss(const QString &src);
QList<SubLine> parseLrc(const QString &src, double lastEnd = 0);
// YouTube's timed-text JSON (yt-dlp --sub-format json3).
QList<SubLine> parseJson3(const QString &src);
// Picks the parser from the file extension.
QList<SubLine> parse(const QString &name, const QString &src);
// Reads a file, guessing its encoding (UTF-8, UTF-16 BOM, else Shift_JIS).
QString readText(const QString &path);
QList<SubLine> load(const QString &path);

struct AssSplit {
    QList<SubLine> dialogue;
    QString signsAss; // header + styles + sign events only; empty when there are none
    int signs = 0;
};
AssSplit splitAss(const QString &src);

// Migaku's clean-up as a track loads: time order, no empty or music-only lines,
// repeats within 5 s merged, same-time lines joined, half-width kana widened.
QList<SubLine> normalize(QList<SubLine> lines);
// Index of the line showing at `time`, or -1.
int lineAt(const QList<SubLine> &lines, double time);
// Index of the last line starting at or before `time`, or -1.
int lineBefore(const QList<SubLine> &lines, double time);
// A line as shown: speaker dashes and bracketed asides taken out of lines that say
// something else too.
QString tidyLine(const QString &text, bool dropAsides = false);
// Text of the lines of another track spoken during `line`.
QString overlapText(const SubLine &line, const QList<SubLine> &others, double shift = 0, double share = 0.3);

QString toSrt(const QList<SubLine> &lines);
// Whether the text is mostly Japanese (to tell a Japanese track from a translation).
bool looksJapanese(const QList<SubLine> &lines);

} // namespace subs
