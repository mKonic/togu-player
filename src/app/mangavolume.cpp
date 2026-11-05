#include "mangavolume.h"

#include "core/dict/dict.h"
#include "core/jp/tokenizer.h"
#include "core/media/cut.h"
#include "core/util/db.h"
#include "core/words/stats.h"
#include "core/words/words.h"
#include "settings.h"

#include <QDateTime>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

QString MangaVolume::title() const
{
    if (m_vol.title.isEmpty()) return m_vol.volume;
    return m_vol.volume.isEmpty() || m_vol.volume == m_vol.title ? m_vol.title : m_vol.title + QStringLiteral(" · ") + m_vol.volume;
}

void MangaVolume::open(const QString &pathOrUrl)
{
    const QUrl u(pathOrUrl);
    m_vol = mokuro::load(u.isLocalFile() ? u.toLocalFile() : pathOrUrl);
    if (!m_vol.error.isEmpty()) {
        emit failed(m_vol.error);
        return;
    }
    m_pages.clear();
    for (const mokuro::Page &p : m_vol.pages) {
        QVariantList boxes;
        for (const mokuro::Box &b : p.boxes)
            boxes << QVariantMap{{"x", b.rect.x()}, {"y", b.rect.y()}, {"w", b.rect.width()}, {"h", b.rect.height()},
                                 {"vertical", b.vertical}, {"text", b.text()}};
        m_pages << QVariantMap{{"image", QUrl::fromLocalFile(p.image).toString()}, {"width", p.width}, {"height", p.height}, {"boxes", boxes}};
    }
    const QString pos = db::scalar(db::app(), QStringLiteral("SELECT pos FROM positions WHERE path=?"), {m_vol.path}).toString();
    m_start = std::clamp(QJsonDocument::fromJson(pos.toUtf8()).object().value("page").toInt(), 0, std::max(0, int(m_pages.size()) - 1));
    emit opened();
}

void MangaVolume::savePage(int page)
{
    if (m_vol.path.isEmpty()) return;
    const QString pos = QString::fromUtf8(QJsonDocument(QJsonObject{{"page", page}, {"pages", int(m_pages.size())}}).toJson(QJsonDocument::Compact));
    db::exec(db::app(), QStringLiteral("INSERT INTO positions (path, pos, time) VALUES (?,?,?) ON CONFLICT(path) DO UPDATE SET pos=excluded.pos, time=excluded.time"),
             {m_vol.path, pos, QDateTime::currentSecsSinceEpoch()});
}

QString MangaVolume::crop(int page, double x, double y, double w, double h) const
{
    if (page < 0 || page >= m_vol.pages.size()) return {};
    QImage img(m_vol.pages[page].image);
    if (img.isNull()) return {};
    // Room around the bubble so the panel reads; at least a third of the page.
    const double cx = x + w / 2, cy = y + h / 2;
    const double cw = std::max(w * 3, img.width() / 2.0), ch = std::max(h * 1.8, img.height() / 3.0);
    QRect r = QRectF(cx - cw / 2, cy - ch / 2, cw, ch).toAlignedRect().intersected(img.rect());
    QImage out = img.copy(r);
    const int maxH = Settings::v("imageHeight").toInt();
    if (out.height() > maxH) out = out.scaledToHeight(maxH, Qt::SmoothTransformation);
    const QString path = media::mediaCacheDir() + QStringLiteral("/manga-%1.webp").arg(QDateTime::currentMSecsSinceEpoch());
    return out.save(path, "WEBP", 82) ? path : QString();
}

QVariantMap MangaVolume::stats() const
{
    QList<QList<ScoredToken>> groups;
    const Words &words = Words::instance();
    QList<QList<jp::Token>> toks;
    QStringList keys;
    for (const mokuro::Page &p : m_vol.pages)
        for (const mokuro::Box &b : p.boxes) {
            toks << jp::tokenize(b.text());
            for (const jp::Token &t : toks.last())
                if (t.word) keys << t.key;
        }
    const auto info = Dict::instance().info(keys);
    for (const auto &line : toks) {
        QList<ScoredToken> g;
        for (const jp::Token &t : line) g.append({t.surface, t.key, t.word, t.word ? words.status(t.key) : Status::Unknown, info.value(t.key).rank, t.word && words.hasCard(t.key)});
        groups << g;
    }
    const PageStats s = analyze(groups, words.knownCount()).stats;
    return {{"comprehension", s.comprehension}, {"words", s.words}};
}
