#pragma once
// Word statuses from the learner's Anki decks: how each card is doing decides
// whether its word is tracked, learning or known.

#include "anki.h"
#include "core/words/words.h"

#include <QHash>
#include <QStringList>

namespace statussync {

struct CardState {
    QString type;        // new, learning, review, relearning
    bool suspended = false;
    double intervalDays = 0;
    double stability = -1; // FSRS stability in days, -1 when not available
    int lapses = 0;
};

struct Rules {
    double matureDays = 21; // Anki's young/mature line
    int leechLapses = 8;
};

// The status one card stands for. Returns Unknown for "no opinion" (suspended).
Status statusFor(const CardState &c, const Rules &r, Status previous = Status::Unknown);
// The weakest of several cards of one note decides.
Status combine(const QList<Status> &statuses);

struct ModelConfig {
    QString wordField, readingField;
    QList<int> templates; // counted card templates; empty: all
};

struct Config {
    QStringList decks;      // learning decks: statuses follow the cards
    QStringList knownDecks; // every word in these counts as known
    QHash<QString, ModelConfig> models;
    Rules rules;
};

struct Report {
    int cards = 0, changed = 0, words = 0;
    QString error;
};

// Blocking; call from a worker thread. Brings anki_cards and the word list up to date.
Report sync(const anki::Conn &c, const Config &cfg);

} // namespace statussync
