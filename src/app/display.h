#pragma once
// Tokens in the shape QML draws them: surface, status, furigana, pitch, stars, gloss.

#include "core/jp/tokenizer.h"

#include <QSet>
#include <QVariantList>

namespace display {

// One map per token:
//   s surface, k key, w is a word, st status name, segs [{t, r}] furigana runs,
//   r reading, rank, stars, p pitch class, g gloss, c has card, rec recommended stars (0 = no)
// `recommended` holds the keys of the one unknown word of recommended sentences.
QVariantList tokens(const QList<jp::Token> &toks, const QSet<QString> &recommended = {});
// Re-resolves statuses (and card marks) in an already built list.
void refresh(QVariantList &list);
// The keys of the recommended word in each sentence of these lines.
QSet<QString> recommendedKeys(const QList<QList<jp::Token>> &lines);

} // namespace display
