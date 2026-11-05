#include "reader.h"

#include "backend.h"
#include "core/dict/dict.h"
#include "core/jp/jp.h"
#include "core/util/db.h"
#include "core/words/stats.h"
#include "core/words/words.h"
#include "settings.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QtConcurrent>

namespace {

QChar statusLetter(Status s)
{
    switch (s) {
    case Status::Tracked: return u't';
    case Status::Learning: return u'l';
    case Status::Known: return u'k';
    case Status::Ignored: return u'i';
    default: return u'u';
    }
}

QString esc(const QString &s) { return s.toHtmlEscaped(); }

} // namespace

Reader::Reader(QObject *parent) : QObject(parent)
{
    connect(Togu::instance(), &Togu::statusesChanged, this, [this](const QStringList &keys) {
        QVariantMap up;
        const Words &w = Words::instance();
        if (keys.isEmpty()) {
            for (const auto &para : m_tokens.value(m_chapter))
                for (const jp::Token &t : para)
                    if (t.word) up.insert(t.key, QString(statusLetter(w.status(t.key))));
        } else {
            for (const QString &k : keys) up.insert(k, QString(statusLetter(w.status(k))));
        }
        emit statusUpdates(up);
        updateStats();
    });
}

QStringList Reader::chapterTitles() const
{
    QStringList out;
    for (const auto &c : m_book.chapters) out << c.title;
    return out;
}

void Reader::open(const QString &pathOrUrl)
{
    const QUrl u(pathOrUrl);
    const QString path = u.isLocalFile() ? u.toLocalFile() : pathOrUrl;
    const int gen = ++m_generation;
    m_loading = true;
    emit loadingChanged();
    auto *w = new QFutureWatcher<book::Book>(this);
    connect(w, &QFutureWatcherBase::finished, this, [this, w, gen] {
        w->deleteLater();
        if (gen != m_generation) return;
        m_book = w->result();
        m_tokens.clear();
        m_loading = false;
        emit loadingChanged();
        if (!m_book.error.isEmpty() || m_book.chapters.isEmpty()) {
            emit failed(m_book.error.isEmpty() ? tr("Nothing to read in this file.") : m_book.error);
            return;
        }
        emit opened();
        // Where you stopped.
        const QString pos = db::scalar(db::app(), QStringLiteral("SELECT pos FROM positions WHERE path=?"), {m_book.path}).toString();
        const QJsonObject o = QJsonDocument::fromJson(pos.toUtf8()).object();
        showChapter(std::clamp(o.value("chapter").toInt(), 0, int(m_book.chapters.size()) - 1), o.value("fraction").toDouble());
    });
    w->setFuture(QtConcurrent::run([path] { return book::load(path); }));
}

void Reader::showChapter(int index, double fraction)
{
    if (index < 0 || index >= m_book.chapters.size()) return;
    m_chapter = index;
    m_restore = fraction;
    emit chapterChanged();
    const int gen = m_generation;
    m_loading = true;
    emit loadingChanged();
    const book::Chapter ch = m_book.chapters[index];
    const bool cached = m_tokens.contains(index);
    auto *w = new QFutureWatcher<QList<QList<jp::Token>>>(this);
    connect(w, &QFutureWatcherBase::finished, this, [this, w, gen, index] {
        w->deleteLater();
        if (gen != m_generation || index != m_chapter) return;
        m_tokens.insert(index, w->result());
        m_loading = false;
        emit loadingChanged();
        emit chapterReady(bodyFor(index));
        updateStats();
        savePosition(m_restore);
    });
    const auto tokens = m_tokens.value(index);
    w->setFuture(QtConcurrent::run([ch, cached, tokens] {
        if (cached) return tokens;
        QList<QList<jp::Token>> out;
        for (const book::Block &b : ch.blocks) out << (b.kind == book::Block::Image ? QList<jp::Token>() : jp::tokenize(b.text));
        return out;
    }));
}

QString Reader::bodyFor(int index)
{
    const book::Chapter &ch = m_book.chapters[index];
    const auto &paras = m_tokens.value(index);
    QStringList keys;
    for (const auto &p : paras)
        for (const jp::Token &t : p)
            if (t.word) keys << t.key;
    const auto info = Dict::instance().info(keys);
    const auto gloss = Dict::instance().gloss(keys);
    const Words &words = Words::instance();
    QString html;
    html.reserve(200000);
    for (int p = 0; p < ch.blocks.size(); ++p) {
        const book::Block &b = ch.blocks[p];
        if (b.kind == book::Block::Image) {
            html += QStringLiteral("<figure><img src=\"%1\"></figure>").arg(QUrl::fromLocalFile(b.image).toString());
            continue;
        }
        html += b.kind == book::Block::Heading ? QStringLiteral("<h2>") : QStringLiteral("<p>");
        const auto toks = paras.value(p);
        for (int t = 0; t < toks.size(); ++t) {
            const jp::Token &tk = toks[t];
            if (!tk.word) {
                html += esc(tk.surface).replace(u'\n', QStringLiteral("<br>"));
                continue;
            }
            const WordInfo wi = info.value(tk.key);
            QString attrs = QStringLiteral(" data-k=\"%1\" data-s=\"%2\" data-i=\"%3,%4\"").arg(esc(tk.key)).arg(statusLetter(words.status(tk.key))).arg(p).arg(t);
            if (!wi.pitch.isEmpty()) attrs += QStringLiteral(" data-p=\"%1\"").arg(jp::pitchClass(wi.pitch.first(), tk.lemmaReading, tk.pos));
            const QString g = gloss.value(tk.key);
            if (!g.isEmpty()) attrs += QStringLiteral(" data-g=\"%1\"").arg(esc(g));
            QString inner;
            if (jp::hasKanji(tk.surface)) {
                for (const jp::FuriSeg &s : jp::furiganaSegments(tk.surface, tk.reading))
                    inner += s.reading.isEmpty() ? esc(s.text) : QStringLiteral("<ruby>%1<rt>%2</rt></ruby>").arg(esc(s.text), esc(s.reading));
            } else {
                inner = esc(tk.surface);
            }
            html += QStringLiteral("<span class=\"w\"%1>%2</span>").arg(attrs, inner);
        }
        html += b.kind == book::Block::Heading ? QStringLiteral("</h2>") : QStringLiteral("</p>");
    }
    return html;
}

void Reader::updateStats()
{
    QList<QList<ScoredToken>> groups;
    const Words &words = Words::instance();
    QStringList keys;
    for (const auto &p : m_tokens.value(m_chapter))
        for (const jp::Token &t : p)
            if (t.word) keys << t.key;
    const auto info = Dict::instance().info(keys);
    for (const auto &p : m_tokens.value(m_chapter)) {
        QList<ScoredToken> g;
        for (const jp::Token &t : p) g.append({t.surface, t.key, t.word, t.word ? words.status(t.key) : Status::Unknown, info.value(t.key).rank, t.word && words.hasCard(t.key)});
        groups << g;
    }
    const PageStats s = analyze(groups, words.knownCount()).stats;
    const Difficulty d = difficulty(s.comprehension);
    m_stats = {{"comprehension", s.comprehension}, {"label", d.label}, {"tone", d.tone}, {"words", s.words},
               {"unknown", s.uniqueUnknown}, {"recommended", s.recommended}};
    emit statsChanged();
}

QVariantMap Reader::token(int para, int tok) const
{
    const auto paras = m_tokens.value(m_chapter);
    if (para < 0 || para >= paras.size() || tok < 0 || tok >= paras[para].size()) return {};
    const jp::Token &t = paras[para][tok];
    return {{"s", t.surface}, {"k", t.key}, {"w", t.word}, {"l", t.lemma}, {"r", t.reading}};
}

QString Reader::textFrom(int para, int tok) const
{
    const auto paras = m_tokens.value(m_chapter);
    if (para < 0 || para >= paras.size()) return {};
    QString out;
    for (qsizetype i = tok; i < paras[para].size() && out.size() < 40; ++i) out += paras[para][i].surface;
    return out;
}

QString Reader::sentenceAt(int para, int tok) const
{
    const auto paras = m_tokens.value(m_chapter);
    if (para < 0 || para >= paras.size()) return {};
    const auto &toks = paras[para];
    static const QString ends = QStringLiteral("。！？!?」』\n");
    auto isEnd = [&](const jp::Token &t) { return !t.word && std::any_of(t.surface.begin(), t.surface.end(), [&](QChar c) { return ends.contains(c); }); };
    qsizetype a = tok, b = tok;
    while (a > 0 && !isEnd(toks[a - 1])) --a;
    while (b < toks.size() - 1 && !isEnd(toks[b])) ++b;
    QString out;
    for (qsizetype i = a; i <= b && i < toks.size(); ++i) out += toks[i].surface;
    return out.trimmed();
}

void Reader::savePosition(double fraction)
{
    if (m_book.path.isEmpty()) return;
    m_restore = fraction;
    const QString pos = QString::fromUtf8(QJsonDocument(QJsonObject{{"chapter", m_chapter}, {"fraction", fraction},
                                                                    {"chapters", int(m_book.chapters.size())}}).toJson(QJsonDocument::Compact));
    db::exec(db::app(), QStringLiteral("INSERT INTO positions (path, pos, time) VALUES (?,?,?) ON CONFLICT(path) DO UPDATE SET pos=excluded.pos, time=excluded.time"),
             {m_book.path, pos, QDateTime::currentSecsSinceEpoch()});
}

QString Reader::pageShell() const
{
    const bool dark = Settings::v("readerTheme").toString() != u"light";
    const bool vertical = Settings::v("readerVertical").toBool();
    const QString bg = dark ? QStringLiteral("#100536") : QStringLiteral("#fbf8f1");
    const QString fg = dark ? QStringLiteral("#ecebff") : QStringLiteral("#1d1b2e");
    const QString furi = Settings::v("furigana").toString();
    const QString pitch = Settings::v("pitchColors").toString();
    const QString gloss = Settings::v("wordGloss").toString();
    const bool status = Settings::v("statusColors").toBool();
    QString css = QStringLiteral(R"(
html { writing-mode: %1; background: %2; color: %3; }
body { margin: 0; padding: %4; font-family: '%5', 'Noto Serif CJK JP', serif; font-size: %6px; line-height: 1.9; }
p, h2 { margin: 0 0 0.9em 0; } h2 { font-size: 1.3em; }
figure { margin: 1em 0; text-align: center; } img { max-width: 100%%; max-height: 92vh; }
.w { cursor: pointer; border-radius: 3px; }
.status .w[data-s=u] { box-shadow: inset 0 -0.12em rgba(254,70,112,.55); }
.status .w[data-s=l] { box-shadow: inset 0 -0.12em rgba(255,147,69,.55); }
.status .w[data-s=t] { box-shadow: inset 0 -0.12em #c6a5fa; }
.status .w:hover, .status .w.active { background: rgba(178,114,255,.18); }
.vertical.status .w[data-s=u] { box-shadow: inset -0.12em 0 rgba(254,70,112,.55); }
.vertical.status .w[data-s=l] { box-shadow: inset -0.12em 0 rgba(255,147,69,.55); }
.vertical.status .w[data-s=t] { box-shadow: inset -0.12em 0 #c6a5fa; }
rt { font-size: 0.5em; opacity: .85; user-select: none; }
.furi-off rt, .furi-hover .w:not(:hover):not(.active) rt { visibility: hidden; }
.furi-unknown .w:not([data-s=u]):not([data-s=t]) rt { display: none; }
.pitch-all .w[data-p=h], .pitch-hover .w[data-p=h]:hover { color: #2880ff; }
.pitch-all .w[data-p=a], .pitch-hover .w[data-p=a]:hover { color: #fe4670; }
.pitch-all .w[data-p=n], .pitch-hover .w[data-p=n]:hover { color: #fba335; }
.pitch-all .w[data-p=o], .pitch-hover .w[data-p=o]:hover { color: #38b8a1; }
.pitch-all .w[data-p=k], .pitch-hover .w[data-p=k]:hover { color: #8d4bf6; }
.gloss-unknown .w[data-g]:is([data-s=u],[data-s=t])::after { content: attr(data-g); font-size: .45em; opacity: .7; margin-inline-start: .2em; font-family: Inter, sans-serif; }
::-webkit-scrollbar { width: 8px; height: 8px; } ::-webkit-scrollbar-thumb { background: rgba(128,128,160,.35); border-radius: 4px; }
)")
        .arg(vertical ? QStringLiteral("vertical-rl") : QStringLiteral("horizontal-tb"), bg, fg,
             vertical ? QStringLiteral("6vh 4vw") : QStringLiteral("5vh max(6vw, calc(50vw - 24em))"),
             Settings::v("readerFont").toString(), QString::number(Settings::v("readerSize").toInt()));
    QStringList classes;
    if (status) classes << QStringLiteral("status");
    if (vertical) classes << QStringLiteral("vertical");
    classes << u"furi-" + furi << u"pitch-" + pitch << u"gloss-" + gloss;
    // Events go back to QML as console messages: "togu:{json}".
    const QString js = QStringLiteral(R"(
function send(o) { console.log('togu:' + JSON.stringify(o)); }
let last = null;
function rectOf(el) { const r = el.getBoundingClientRect(); return { x: r.left, y: r.top, w: r.width, h: r.height }; }
document.addEventListener('mouseover', e => {
  const w = e.target.closest('.w'); if (!w || w === last) return;
  last = w; const [p, t] = w.dataset.i.split(',').map(Number);
  send(Object.assign({ type: 'hover', p, t }, rectOf(w)));
});
document.addEventListener('click', e => {
  const w = e.target.closest('.w');
  if (!w) { send({ type: 'blank' }); return; }
  const [p, t] = w.dataset.i.split(',').map(Number);
  send(Object.assign({ type: 'click', p, t }, rectOf(w)));
});
document.addEventListener('mouseleave', () => { last = null; send({ type: 'leave' }); });
let posTimer = 0;
function fraction() {
  const el = document.scrollingElement;
  const vertical = document.documentElement.style.writingMode === 'vertical-rl' || getComputedStyle(document.documentElement).writingMode === 'vertical-rl';
  if (vertical) { const max = el.scrollWidth - el.clientWidth; return max > 0 ? Math.abs(el.scrollLeft) / max : 0; }
  const max = el.scrollHeight - el.clientHeight; return max > 0 ? el.scrollTop / max : 0;
}
window.addEventListener('scroll', () => { clearTimeout(posTimer); posTimer = setTimeout(() => send({ type: 'pos', f: fraction() }), 400); send({ type: 'scroll' }); });
window.togu = {
  setBody(html, f) {
    document.body.innerHTML = html;
    requestAnimationFrame(() => togu.goTo(f || 0));
  },
  goTo(f) {
    const el = document.scrollingElement;
    if (getComputedStyle(document.documentElement).writingMode === 'vertical-rl') el.scrollLeft = -(el.scrollWidth - el.clientWidth) * f;
    else el.scrollTop = (el.scrollHeight - el.clientHeight) * f;
  },
  page(dir) {
    const el = document.scrollingElement;
    const vertical = getComputedStyle(document.documentElement).writingMode === 'vertical-rl';
    const before = vertical ? el.scrollLeft : el.scrollTop;
    if (vertical) el.scrollBy({ left: -dir * el.clientWidth * 0.92, behavior: 'instant' });
    else el.scrollBy({ top: dir * el.clientHeight * 0.92, behavior: 'instant' });
    const after = vertical ? el.scrollLeft : el.scrollTop;
    if (Math.abs(after - before) < 2) send({ type: 'edge', dir });
  },
  status(map) {
    for (const el of document.querySelectorAll('.w')) { const s = map[el.dataset.k]; if (s) el.dataset.s = s; }
  },
  active(p, t) {
    for (const el of document.querySelectorAll('.w.active')) el.classList.remove('active');
    if (p >= 0) { const el = document.querySelector('.w[data-i="' + p + ',' + t + '"]'); if (el) el.classList.add('active'); }
  }
};
document.addEventListener('keydown', e => {
  if (e.ctrlKey || e.metaKey) return;
  send({ type: 'key', key: e.key, code: e.code, shift: e.shiftKey, alt: e.altKey });
  if ([' ', 'PageDown', 'PageUp', 'ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)) e.preventDefault();
});
)");
    return QStringLiteral("<!DOCTYPE html><html><head><meta charset=\"utf-8\"><style>%1</style></head><body class=\"%2\"></body><script>%3</script></html>")
        .arg(css, classes.join(u' '), js);
}
