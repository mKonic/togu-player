#pragma once
// Comprehension and recommended sentences, as Migaku computes them (ported from togu).

#include "words.h"

#include <QList>
#include <QString>

struct ScoredToken {
    QString surface;
    QString key;
    bool word = false;
    Status status = Status::Unknown;
    int rank = 0;      // frequency rank, 0 when unlisted
    bool card = false; // Anki has a note for this word
};

struct PageStats {
    int words = 0;
    int comprehension = 0; // 0-100
    int uniqueKnown = 0, uniqueLearning = 0, uniqueUnknown = 0, uniqueIgnored = 0;
    int sentences = 0, allKnown = 0, oneUnknown = 0, multiUnknown = 0;
    int recommended = 0;
    int avgRank = 0;
};

struct Analysis {
    PageStats stats;
    QList<ScoredToken> recommended; // the unknown word of each recommended sentence
    QList<int> recommendedGroups;   // group index for each of those
};

QList<QList<ScoredToken>> splitSentences(const QList<ScoredToken> &tokens);
double recommendCeiling(int knownWords);
// `groups` are independent runs of text (one per subtitle line).
Analysis analyze(const QList<QList<ScoredToken>> &groups, int knownWords = 0);
int starsForRank(int rank);

struct Difficulty {
    QString label;
    QString tone; // teal, blue, purple, orange, pink
};
Difficulty difficulty(int comprehension);
