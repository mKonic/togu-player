#pragma once
// Auto-sync: find where speech is in the audio, then the subtitle shift that lines
// the lines up with it (alass-style: one offset, optionally a second after a split).

#include "subs.h"

#include <QByteArray>
#include <QList>
#include <vector>

namespace sync {

constexpr double FRAME = 0.01; // seconds per voice-activity frame

// One flag per 10 ms frame of 16 kHz mono s16 PCM.
std::vector<uint8_t> voiceActivity(const QByteArray &pcm);

struct Result {
    double offset = 0;      // seconds to add to every line (before the split)
    int split = -1;         // first line index of the second part, -1 when none
    double offset2 = 0;     // shift for lines from `split` on
    double confidence = 0;  // 0..1
    double before = 0;      // share of subtitle time over speech before shifting
    double after = 0;       // ...and after
    bool ok() const { return confidence >= 0.5; }
};

Result align(const QList<SubLine> &lines, const std::vector<uint8_t> &speech, double maxShift = 60);
QList<SubLine> apply(QList<SubLine> lines, double offset, int split = -1, double offset2 = 0);

} // namespace sync
