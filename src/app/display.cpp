#include "display.h"

#include "core/dict/dict.h"
#include "core/jp/jp.h"
#include "core/words/stats.h"
#include "core/words/words.h"

namespace display {

QVariantList tokens(const QList<jp::Token> &toks, const QSet<QString> &recommended)
{
    QStringList keys;
    for (const jp::Token &t : toks)
        if (t.word) keys << t.key;
    const auto info = Dict::instance().info(keys);
    const auto gloss = Dict::instance().gloss(keys);
    const Words &words = Words::instance();
    QVariantList out;
    out.reserve(toks.size());
    for (const jp::Token &t : toks) {
        QVariantMap m;
        m.insert("s", t.surface);
        m.insert("w", t.word);
        if (t.word) {
            m.insert("k", t.key);
            m.insert("st", statusName(words.status(t.key)));
            m.insert("c", words.hasCard(t.key));
            m.insert("r", t.reading);
            m.insert("l", t.lemma);
            QVariantList segs;
            if (jp::hasKanji(t.surface))
                for (const jp::FuriSeg &f : jp::furiganaSegments(t.surface, t.reading)) segs << QVariantMap{{"t", f.text}, {"r", f.reading}};
            m.insert("segs", segs);
            const WordInfo wi = info.value(t.key);
            m.insert("rank", wi.rank);
            const int stars = starsForRank(wi.rank);
            m.insert("stars", stars);
            if (!wi.pitch.isEmpty()) m.insert("p", QString(jp::pitchClass(wi.pitch.first(), t.lemmaReading, t.pos)));
            m.insert("g", gloss.value(t.key));
            m.insert("rec", recommended.contains(t.key) ? std::max(1, stars) : 0);
        }
        out << m;
    }
    return out;
}

void refresh(QVariantList &list)
{
    const Words &words = Words::instance();
    for (QVariant &v : list) {
        QVariantMap m = v.toMap();
        if (!m.value("w").toBool()) continue;
        const QString k = m.value("k").toString();
        m.insert("st", statusName(words.status(k)));
        m.insert("c", words.hasCard(k));
        v = m;
    }
}

QSet<QString> recommendedKeys(const QList<QList<jp::Token>> &lines)
{
    QList<QList<ScoredToken>> groups;
    QStringList keys;
    for (const auto &line : lines)
        for (const jp::Token &t : line)
            if (t.word) keys << t.key;
    const auto info = Dict::instance().info(keys);
    const Words &words = Words::instance();
    for (const auto &line : lines) {
        QList<ScoredToken> g;
        for (const jp::Token &t : line)
            g.append({t.surface, t.key, t.word, t.word ? words.status(t.key) : Status::Unknown, info.value(t.key).rank,
                      t.word && words.hasCard(t.key)});
        groups << g;
    }
    QSet<QString> out;
    for (const ScoredToken &t : analyze(groups, words.knownCount()).recommended) out.insert(t.key);
    return out;
}

} // namespace display
