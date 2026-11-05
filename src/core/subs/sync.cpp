#include "sync.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace sync {

std::vector<uint8_t> voiceActivity(const QByteArray &pcm)
{
    const auto *s = reinterpret_cast<const int16_t *>(pcm.constData());
    const size_t n = size_t(pcm.size()) / 2;
    const size_t frameLen = 160; // 10 ms at 16 kHz
    const size_t frames = n / frameLen;
    std::vector<float> db(frames);
    // A first-order high-pass (~150 Hz) keeps hum and rumble out of the energy.
    float prevIn = 0, prevOut = 0;
    const float a = 0.944f;
    for (size_t f = 0; f < frames; ++f) {
        double sum = 0;
        for (size_t i = 0; i < frameLen; ++i) {
            const float x = s[f * frameLen + i] / 32768.f;
            const float y = a * (prevOut + x - prevIn);
            prevIn = x;
            prevOut = y;
            sum += double(y) * y;
        }
        db[f] = float(10 * std::log10(sum / frameLen + 1e-10));
    }
    if (frames == 0) return {};
    std::vector<float> sorted = db;
    std::sort(sorted.begin(), sorted.end());
    const float floor = sorted[frames / 10], high = sorted[frames * 9 / 10];
    const float threshold = floor + std::max(6.f, 0.35f * (high - floor));
    std::vector<uint8_t> out(frames);
    for (size_t f = 0; f < frames; ++f) out[f] = db[f] > threshold;
    // Hangover: speech carries on 200 ms past the last loud frame; drop blips under 80 ms.
    std::vector<uint8_t> smooth(frames);
    int hang = 0;
    for (size_t f = 0; f < frames; ++f) {
        if (out[f]) hang = 20;
        smooth[f] = hang > 0;
        if (hang > 0) --hang;
    }
    size_t i = 0;
    while (i < frames) {
        if (!smooth[i]) {
            ++i;
            continue;
        }
        size_t j = i;
        while (j < frames && smooth[j]) ++j;
        if (j - i < 8 + 20) std::fill(smooth.begin() + long(i), smooth.begin() + long(j), 0);
        i = j;
    }
    return smooth;
}

namespace {

struct Spans {
    std::vector<int> start, end; // frames
};

Spans spans(const QList<SubLine> &lines)
{
    Spans s;
    for (const SubLine &l : lines) {
        s.start.push_back(int(std::lround(l.start / FRAME)));
        s.end.push_back(int(std::lround(l.end / FRAME)));
    }
    return s;
}

// Speech frames inside [a, b) using prefix sums; frames outside the audio count as silence.
inline int count(const std::vector<int> &prefix, int a, int b)
{
    const int n = int(prefix.size()) - 1;
    a = std::clamp(a, 0, n);
    b = std::clamp(b, 0, n);
    return b > a ? prefix[b] - prefix[a] : 0;
}

} // namespace

Result align(const QList<SubLine> &lines, const std::vector<uint8_t> &speech, double maxShift)
{
    Result r;
    if (lines.isEmpty() || speech.empty()) return r;
    std::vector<int> prefix(speech.size() + 1, 0);
    for (size_t i = 0; i < speech.size(); ++i) prefix[i + 1] = prefix[i] + speech[i];
    const Spans sp = spans(lines);
    const int L = int(lines.size());
    const int range = int(maxShift / FRAME);
    const int W = 2 * range + 1;
    int totalLen = 0;
    for (int i = 0; i < L; ++i) totalLen += std::max(0, sp.end[i] - sp.start[i]);
    if (totalLen == 0) return r;

    // score(line, shift) = 2 * overlap - length: subtitle time over silence costs as much as over speech earns.
    auto lineScore = [&](int i, int o) {
        const int len = std::max(0, sp.end[i] - sp.start[i]);
        return 2 * count(prefix, sp.start[i] + o, sp.end[i] + o) - len;
    };
    std::vector<long> total(W, 0);
    for (int i = 0; i < L; ++i)
        for (int w = 0; w < W; ++w) total[w] += lineScore(i, w - range);
    const int best = int(std::max_element(total.begin(), total.end()) - total.begin());
    // Second-best peak at least 1 s away, for confidence.
    long second = std::numeric_limits<long>::min();
    for (int w = 0; w < W; ++w)
        if (std::abs(w - best) > 100) second = std::max(second, total[w]);
    r.offset = (best - range) * FRAME;
    const double margin = double(total[best] - second) / totalLen;
    r.confidence = std::clamp(margin / 0.15, 0.0, 1.0);
    auto overlapShare = [&](int o) {
        long ov = 0;
        for (int i = 0; i < L; ++i) ov += count(prefix, sp.start[i] + o, sp.end[i] + o);
        return double(ov) / totalLen;
    };
    r.before = overlapShare(0);
    r.after = overlapShare(best - range);

    // One split: best offset for lines before k plus best for lines from k, against a penalty.
    if (L >= 20) {
        std::vector<long> acc(W, 0);
        std::vector<long> preBest(L + 1, 0);
        std::vector<int> preArg(L + 1, range);
        for (int k = 1; k <= L; ++k) {
            for (int w = 0; w < W; ++w) acc[w] += lineScore(k - 1, w - range);
            const auto it = std::max_element(acc.begin(), acc.end());
            preBest[k] = *it;
            preArg[k] = int(it - acc.begin());
        }
        std::fill(acc.begin(), acc.end(), 0);
        std::vector<long> sufBest(L + 1, 0);
        std::vector<int> sufArg(L + 1, range);
        for (int k = L - 1; k >= 0; --k) {
            for (int w = 0; w < W; ++w) acc[w] += lineScore(k, w - range);
            const auto it = std::max_element(acc.begin(), acc.end());
            sufBest[k] = *it;
            sufArg[k] = int(it - acc.begin());
        }
        const long penalty = std::max<long>(300, totalLen / 20);
        long bestSplit = total[best] + penalty;
        int k0 = -1;
        for (int k = 5; k <= L - 5; ++k) {
            if (preBest[k] + sufBest[k] > bestSplit && std::abs(preArg[k] - sufArg[k]) > 50) {
                bestSplit = preBest[k] + sufBest[k];
                k0 = k;
            }
        }
        if (k0 > 0) {
            r.split = k0;
            r.offset = (preArg[k0] - range) * FRAME;
            r.offset2 = (sufArg[k0] - range) * FRAME;
            long ov = 0;
            for (int i = 0; i < L; ++i) {
                const int o = (i < k0 ? preArg[k0] : sufArg[k0]) - range;
                ov += count(prefix, sp.start[i] + o, sp.end[i] + o);
            }
            r.after = double(ov) / totalLen;
        }
    }
    return r;
}

QList<SubLine> apply(QList<SubLine> lines, double offset, int split, double offset2)
{
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const double o = split >= 0 && i >= split ? offset2 : offset;
        lines[i].start += o;
        lines[i].end += o;
    }
    return lines;
}

} // namespace sync
