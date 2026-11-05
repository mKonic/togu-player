#pragma once
// Yomitan-format dictionaries in SQLite: import, lookup, frequency and pitch.

#include "core/jp/tokenizer.h"
#include "core/words/words.h"

#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QMutex>
#include <QString>

#include <functional>

struct DictInfo {
    int id = 0;
    QString title, revision, kind; // kind: terms, freq, pitch, mixed, kanji
    int order = 0;
    bool enabled = true;
    int terms = 0, metas = 0;
};

struct GlossBlock {
    int dictId = 0;
    QString dict;
    QStringList defTags, termTags;
    QJsonArray glossary;
};

struct FreqInfo {
    QString dict;
    double value = 0;
    QString display;
};

struct PitchInfo {
    QString dict, reading;
    QList<int> positions;
};

struct DictEntry {
    QString expression, reading, matched;
    int matchLen = 0;
    QList<GlossBlock> blocks;
    QList<FreqInfo> freqs;
    QList<PitchInfo> pitches;
    int rank = 0;
    int stars = 0;
};

struct KanjiEntry {
    QString character, dict;
    QStringList onyomi, kunyomi, meanings, tags;
};

struct WordInfo {
    int rank = 0;       // from the first frequency dictionary, 0 when unlisted
    QList<int> pitch;   // accent positions
};

class Dict {
public:
    static Dict &instance();

    struct Progress {
        QString title, phase;
        int done = 0, total = 0;
    };
    // Imports a Yomitan zip. Returns the title, or an error prefixed with "error:".
    QString import(const QString &path, const std::function<void(const Progress &)> &progress = {});
    QList<DictInfo> list();
    void setEnabled(int id, bool on);
    void move(int id, int delta);
    void remove(int id);
    bool empty();

    // Entries for the text starting at the hovered word, longest match first.
    QList<DictEntry> lookup(const QString &textFromWord, int max = 24);
    QList<KanjiEntry> kanji(const QString &character);
    // Frequency and pitch for many word keys at once (cached).
    QHash<QString, WordInfo> info(const QStringList &keys);
    QHash<QString, QString> gloss(const QStringList &keys);
    QByteArray media(int dictId, const QString &path);
    QString primaryFreqTitle();

    void invalidate();

private:
    QMutex m_mutex;
    QList<DictInfo> m_dicts;
    bool m_loaded = false;
    QHash<QString, WordInfo> m_info;
    QHash<QString, QString> m_gloss;
    QList<DictInfo> dictsLocked();
};

// Frequency values come in several shapes in Yomitan dictionaries.
struct ParsedFreq {
    QString reading;
    double value = 0;
    QString display;
    bool ok = false;
};
ParsedFreq parseFreq(const QJsonValue &v);
