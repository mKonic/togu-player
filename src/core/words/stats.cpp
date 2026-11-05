#include "stats.h"

#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

bool endsSentence(const QString &s)
{
    static const QString ends = QStringLiteral("。｡！？!?\n");
    for (QChar c : s)
        if (ends.contains(c)) return true;
    return false;
}

bool isUnknown(const ScoredToken &t) { return t.status == Status::Unknown || t.status == Status::Tracked; }

double clamp(double n, double lo, double hi) { return std::min(hi, std::max(lo, n)); }

struct SentenceScore {
    int wordCount = 0;
    double score = 0;
    QList<ScoredToken> uniqueUnknown;
};

SentenceScore scoreSentence(const QList<ScoredToken> &sentence)
{
    int words = 0, ignored = 0, known = 0, unknown = 0;
    QSet<QString> seen;
    SentenceScore s;
    for (const ScoredToken &t : sentence) {
        if (!t.word) continue;
        ++words;
        if (t.status == Status::Ignored) ++ignored;
        else if (t.status == Status::Known) ++known;
        if (isUnknown(t)) {
            ++unknown;
            if (!seen.contains(t.key)) {
                seen.insert(t.key);
                s.uniqueUnknown.append(t);
            }
        }
    }
    const int count = words - ignored;
    if (known == count) s.score = count > 0 ? 100 : 0;
    else if (unknown == count) s.score = 0;
    else {
        // More than two different unknown words in one sentence costs extra.
        const double u = s.uniqueUnknown.size();
        const double penalty = u <= 2 ? 1 : 1 - clamp((u / count) * 2, 0, 1);
        s.score = (double(known) / count) * 100 * penalty;
    }
    s.wordCount = count;
    return s;
}

// Song lines: ones that open or close with a music note.
bool isSong(const QString &text)
{
    static const QRegularExpression re(QStringLiteral("^〜?[♪♫♬]+〜?|〜?[♪♫♬]+〜?$"));
    return re.match(text).hasMatch();
}

const ScoredToken *recommendedWord(const QList<ScoredToken> &sentence, const SentenceScore &s, int knownWords)
{
    int length = 0;
    QString text;
    for (const ScoredToken &t : sentence) {
        if (t.word) ++length;
        text += t.surface;
    }
    if (length < 5 || length > 20 || s.uniqueUnknown.size() != 1) return nullptr;
    const ScoredToken &word = s.uniqueUnknown.first();
    if (word.card || isSong(text.trimmed())) return nullptr;
    const double ceiling = recommendCeiling(knownWords);
    if (!std::isinf(ceiling) && !(word.rank && word.rank < ceiling)) return nullptr;
    return &word;
}

} // namespace

QList<QList<ScoredToken>> splitSentences(const QList<ScoredToken> &tokens)
{
    QList<QList<ScoredToken>> out;
    QList<ScoredToken> cur;
    for (const ScoredToken &t : tokens) {
        cur.append(t);
        if (!t.word && endsSentence(t.surface)) {
            out.append(cur);
            cur.clear();
        }
    }
    if (!cur.isEmpty()) out.append(cur);
    out.removeIf([](const QList<ScoredToken> &s) { return std::none_of(s.begin(), s.end(), [](auto &t) { return t.word; }); });
    return out;
}

// The rank an unknown word may have at most to be recommended. It grows with
// the number of words the learner knows. Past 20,000 known words anything goes.
double recommendCeiling(int knownWords)
{
    static const int LEVELS[][2] = {{1500, 1000}, {5000, 3000}, {15000, 5000}, {30000, 10000}, {60000, 1000000}};
    constexpr int n = 5;
    if (knownWords >= 20000) return std::numeric_limits<double>::infinity();
    int i = 0;
    while (i < n && knownWords >= LEVELS[i][0]) ++i;
    if (i >= n) i = n - 1;
    const int level = LEVELS[i][0], head = LEVELS[i][1];
    if (i == n - 1) return knownWords + head;
    const int extra = level < 30000 ? int(std::floor((LEVELS[i + 1][1] - head) * (double(knownWords) / level))) : 0;
    return knownWords + head + extra;
}

Analysis analyze(const QList<QList<ScoredToken>> &groups, int knownWords)
{
    QHash<QString, Status> unique;
    QList<SentenceScore> scores;
    Analysis a;
    PageStats &st = a.stats;
    for (qsizetype g = 0; g < groups.size(); ++g) {
        for (const ScoredToken &t : groups[g])
            if (t.word) unique.insert(t.key, t.status);
        for (const auto &sentence : splitSentences(groups[g])) {
            SentenceScore s = scoreSentence(sentence);
            if (s.wordCount == 0) continue;
            st.sentences++;
            if (s.uniqueUnknown.isEmpty()) st.allKnown++;
            else if (s.uniqueUnknown.size() == 1) st.oneUnknown++;
            else st.multiUnknown++;
            if (const ScoredToken *w = recommendedWord(sentence, s, knownWords)) {
                a.recommended.append(*w);
                a.recommendedGroups.append(int(g));
            }
            scores.append(std::move(s));
        }
    }
    int words = 0;
    for (const auto &s : scores) words += s.wordCount;
    st.words = words;
    if (!scores.isEmpty() && std::all_of(scores.begin(), scores.end(), [](auto &s) { return s.score == 100; }))
        st.comprehension = 100;
    else if (words) {
        double sum = 0;
        for (const auto &s : scores) sum += s.score * (double(s.wordCount) / words);
        const double base = clamp(std::floor(sum), 0, 100);
        auto pct = [&](int n) { return double(n) / st.sentences * 100; };
        const double ease = 100 - (pct(st.multiUnknown) * 0.7 + pct(st.oneUnknown) * 0.3);
        st.comprehension = int(ease == 100 ? base : clamp(std::floor(base - ((base - ease) / 2) * (base * 0.01)), 0, 100));
    }
    for (Status s : unique) {
        switch (s) {
        case Status::Known: st.uniqueKnown++; break;
        case Status::Learning: st.uniqueLearning++; break;
        case Status::Ignored: st.uniqueIgnored++; break;
        default: st.uniqueUnknown++;
        }
    }
    QSet<QString> recKeys;
    QList<int> ranks;
    for (const auto &t : a.recommended) {
        recKeys.insert(t.key);
        if (t.rank > 0) ranks.append(t.rank);
    }
    st.recommended = int(recKeys.size());
    if (!ranks.isEmpty()) {
        std::sort(ranks.begin(), ranks.end());
        const auto m = ranks.size() / 2;
        st.avgRank = int(std::lround(ranks.size() % 2 ? ranks[m] : (ranks[m - 1] + ranks[m]) / 2.0));
    }
    return a;
}

int starsForRank(int rank)
{
    if (rank <= 0) return 0;
    if (rank <= 1500) return 5;
    if (rank <= 5000) return 4;
    if (rank <= 15000) return 3;
    if (rank <= 30000) return 2;
    if (rank <= 60000) return 1;
    return 0;
}

Difficulty difficulty(int c)
{
    if (c >= 99) return {QStringLiteral("Exceptional"), QStringLiteral("teal")};
    if (c >= 95) return {QStringLiteral("Excellent"), QStringLiteral("teal")};
    if (c >= 90) return {QStringLiteral("Great"), QStringLiteral("teal")};
    if (c >= 80) return {QStringLiteral("Good"), QStringLiteral("blue")};
    if (c >= 70) return {QStringLiteral("Approachable"), QStringLiteral("purple")};
    if (c >= 60) return {QStringLiteral("Challenging"), QStringLiteral("orange")};
    return {QStringLiteral("Ambitious"), QStringLiteral("pink")};
}
