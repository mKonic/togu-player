#include "session.h"

#include "backend.h"
#include "core/library/names.h"
#include "core/util/arc.h"
#include "core/util/db.h"
#include "core/words/stats.h"
#include "core/words/words.h"
#include "display.h"
#include "mpvitem.h"
#include "queue.h"
#include "settings.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtConcurrent>

// ---------------------------------------------------------------- LineModel

QHash<int, QByteArray> LineModel::roleNames() const
{
    return {{StartRole, "start"}, {EndRole, "end"}, {TextRole, "text"}, {Text2Role, "text2"},
            {TokensRole, "tokens"}, {BookmarkRole, "bookmarked"}};
}

QVariant LineModel::data(const QModelIndex &i, int role) const
{
    if (!i.isValid() || i.row() >= m_lines.size()) return {};
    const SubLine &l = m_lines[i.row()];
    switch (role) {
    case StartRole: return l.start + m_offset;
    case EndRole: return l.end + m_offset;
    case TextRole: return l.text;
    case Text2Role: return m_text2.value(i.row());
    case TokensRole: return m_tokens.value(i.row());
    case BookmarkRole: return m_bookmarks.contains(i.row());
    }
    return {};
}

void LineModel::reset(const QList<SubLine> &lines, const QList<QVariantList> &tokens, const QStringList &text2, double offset)
{
    beginResetModel();
    m_lines = lines;
    m_tokens = tokens;
    m_text2 = text2;
    m_offset = offset;
    m_bookmarks.clear();
    endResetModel();
}

void LineModel::setTokens(const QList<QVariantList> &tokens)
{
    m_tokens = tokens;
    if (!m_lines.isEmpty()) emit dataChanged(index(0), index(int(m_lines.size()) - 1), {TokensRole});
}

void LineModel::setText2(const QStringList &text2)
{
    m_text2 = text2;
    if (!m_lines.isEmpty()) emit dataChanged(index(0), index(int(m_lines.size()) - 1), {Text2Role});
}

void LineModel::setOffset(double offset)
{
    m_offset = offset;
    if (!m_lines.isEmpty()) emit dataChanged(index(0), index(int(m_lines.size()) - 1), {StartRole, EndRole});
}

void LineModel::setBookmarks(const QSet<int> &rows)
{
    m_bookmarks = rows;
    if (!m_lines.isEmpty()) emit dataChanged(index(0), index(int(m_lines.size()) - 1), {BookmarkRole});
}

void LineModel::refreshStatuses()
{
    for (QVariantList &t : m_tokens) display::refresh(t);
    if (!m_lines.isEmpty()) emit dataChanged(index(0), index(int(m_lines.size()) - 1), {TokensRole});
}

// ---------------------------------------------------------------- PlaybackSession

PlaybackSession::PlaybackSession(QObject *parent) : QObject(parent)
{
    m_saveTimer.setInterval(5000);
    connect(&m_saveTimer, &QTimer::timeout, this, [this] {
        savePosition();
        trackImmersion();
    });
    connect(Togu::instance(), &Togu::statusesChanged, this, [this] {
        for (QVariantList &t : m_display1) display::refresh(t);
        m_model.refreshStatuses();
        if (m_current >= 0 && m_current < m_display1.size()) {
            m_currentTokens = m_display1[m_current];
            emit currentTokensChanged();
        }
        updateStats();
    });
}

PlaybackSession::~PlaybackSession()
{
    savePosition();
    trackImmersion();
}

void PlaybackSession::setPlayer(MpvItem *p)
{
    if (m_player == p) return;
    if (m_player) disconnect(m_player, nullptr, this, nullptr);
    m_player = p;
    if (p) {
        connect(p, &MpvItem::positionChanged, this, &PlaybackSession::onPosition);
        connect(p, &MpvItem::fileLoaded, this, [this] {
            if (m_restorePosition > 1) m_player->seek(m_restorePosition, false);
            m_restorePosition = -1;
            m_player->set(QStringLiteral("volume"), Settings::v("volume").toDouble());
            applySignsToMpv();
        });
        connect(p, &MpvItem::pausedChanged, this, [this] {
            trackImmersion();
            if (!m_player->paused()) m_playClock.start();
            else m_playClock.invalidate();
        });
        connect(p, &MpvItem::endOfFile, this, [this](const QString &reason) {
            if (reason == u"eof") {
                if (m_itemId) db::exec(db::app(), QStringLiteral("UPDATE items SET watched=1, position=0 WHERE id=?"), {m_itemId});
                emit finished();
            }
        });
        connect(p, &MpvItem::volumeChanged, this, [this] { Settings::instance()->set(QStringLiteral("volume"), m_player->volume()); });
    }
    emit playerChanged();
}

void PlaybackSession::open(const QString &pathOrUrl, int itemId)
{
    close();
    const QUrl u(pathOrUrl);
    m_path = u.isLocalFile() ? u.toLocalFile() : pathOrUrl;
    m_itemId = itemId;
    m_kind = names::isAudio(m_path) ? QStringLiteral("audio") : QStringLiteral("video");
    m_title = names::parse(m_path).title;
    if (m_title.isEmpty()) m_title = QFileInfo(m_path).completeBaseName();

    auto d = db::app();
    if (!m_itemId) {
        m_itemId = db::scalar(d, QStringLiteral("SELECT id FROM items WHERE path=?"), {m_path}).toInt();
        if (!m_itemId) {
            db::exec(d, QStringLiteral("INSERT INTO items (path, kind, title, size, mtime) VALUES (?,?,?,?,?)"),
                     {m_path, m_kind, QFileInfo(m_path).completeBaseName(), QFileInfo(m_path).size(),
                      QFileInfo(m_path).lastModified().toSecsSinceEpoch()});
            m_itemId = db::scalar(d, QStringLiteral("SELECT id FROM items WHERE path=?"), {m_path}).toInt();
        }
    }
    QSqlQuery q(d);
    q.prepare(QStringLiteral("SELECT i.position, i.sub_offset, i.sub_pinned, i.sub_synced, i.sub2_offset, i.title, s.title,"
                             " COALESCE(s.meta_poster, s.poster), i.episode, i.season, s.meta_title FROM items i"
                             " LEFT JOIN series s ON s.id=i.series WHERE i.id=?"));
    q.addBindValue(m_itemId);
    if (db::exec(q) && q.next()) {
        if (Settings::v("resumePlayback").toBool()) m_restorePosition = q.value(0).toDouble();
        m_offset = q.value(1).toDouble();
        m_pinned = q.value(2).toBool();
        m_synced = q.value(3).toBool();
        m_offset2 = q.value(4).toDouble();
        const QString series = q.value(10).toString().isEmpty() ? q.value(6).toString() : q.value(10).toString();
        if (!series.isEmpty()) {
            m_title = series;
            if (!q.value(8).isNull() && q.value(8).toDouble() >= 0)
                m_title += QStringLiteral(" · %1").arg(q.value(8).toDouble());
        }
        const QString poster = q.value(7).toString();
        m_cover = poster.isEmpty() ? QString() : (poster.contains(u"://") ? poster : QUrl::fromLocalFile(poster).toString());
    }
    db::exec(d, QStringLiteral("UPDATE items SET last_played=? WHERE id=?"), {QDateTime::currentSecsSinceEpoch(), m_itemId});
    emit opened();
    emit offsetChanged();
    if (m_player) m_player->setSource(m_path);
    m_saveTimer.start();
    probeAndChooseSubs();
}

void PlaybackSession::openAt(const QString &path, int itemId, double seconds)
{
    if (path == m_path && m_player) {
        m_player->seek(seconds);
        return;
    }
    open(path, itemId);
    m_restorePosition = seconds;
}

void PlaybackSession::openItem(int itemId)
{
    const QString path = db::scalar(db::app(), QStringLiteral("SELECT path FROM items WHERE id=?"), {itemId}).toString();
    if (!path.isEmpty()) open(path, itemId);
}

void PlaybackSession::close()
{
    if (!m_path.isEmpty()) {
        savePosition();
        trackImmersion();
    }
    ++m_generation;
    ++m_syncGeneration;
    m_saveTimer.stop();
    m_path.clear();
    m_itemId = 0;
    m_srcs.clear();
    m_sources.clear();
    m_primaryId.clear();
    m_secondaryId.clear();
    m_lines1.clear();
    m_lines2.clear();
    m_tokens1.clear();
    m_display1.clear();
    m_signsPath.clear();
    m_current = m_last = -1;
    m_currentTokens.clear();
    m_currentText2.clear();
    m_offset = m_offset2 = 0;
    m_pinned = m_synced = false;
    m_syncState.clear();
    m_syncMessage.clear();
    m_stats.clear();
    m_cover.clear();
    m_model.reset({}, {}, {}, 0);
    emit sourcesChanged();
    emit primaryChanged();
    emit secondaryChanged();
    emit currentChanged();
    emit currentTokensChanged();
    emit current2Changed();
    emit syncChanged();
    emit statsChanged();
}

const PlaybackSession::Source *PlaybackSession::source(const QString &id) const
{
    for (const Source &s : m_srcs)
        if (s.id == id) return &s;
    return nullptr;
}

void PlaybackSession::probeAndChooseSubs()
{
    const int gen = m_generation;
    const QString path = m_path;
    ++m_loading;
    emit loadingChanged();
    auto *w = new QFutureWatcher<std::pair<media::Probe, QList<names::SubMatch>>>(this);
    connect(w, &QFutureWatcherBase::finished, this, [this, w, gen] {
        w->deleteLater();
        --m_loading;
        emit loadingChanged();
        if (gen != m_generation) return;
        auto [probe, matches] = w->result();
        m_probe = probe;
        m_srcs.clear();
        for (const names::SubMatch &m : matches) {
            Source s;
            s.id = QStringLiteral("file:") + m.path;
            s.kind = QStringLiteral("file");
            s.path = m.path;
            s.label = QFileInfo(m.path).fileName();
            s.japanese = m.lang == names::Lang::Japanese;
            s.lang = m.lang == names::Lang::Japanese ? QStringLiteral("ja") : m.lang == names::Lang::English ? QStringLiteral("en") : QString();
            m_srcs.append(s);
        }
        for (const media::Stream &st : probe.streams) {
            if (st.type != u"subtitle") continue;
            Source s;
            s.id = QStringLiteral("track:%1").arg(st.index);
            s.kind = QStringLiteral("track");
            s.stream = st;
            s.japanese = media::isJapaneseLang(st.lang, st.title);
            s.lang = st.lang;
            QStringList bits;
            bits << (st.title.isEmpty() ? tr("Track %1").arg(st.typeIndex + 1) : st.title);
            if (!st.lang.isEmpty()) bits << st.lang;
            bits << st.codec.toUpper();
            if (!st.textSub()) bits << tr("image, can't be read");
            s.label = bits.join(QStringLiteral(" · "));
            m_srcs.append(s);
        }
        // Subtitles imported earlier for this item.
        QSqlQuery q(db::app());
        q.prepare(QStringLiteral("SELECT extra FROM items WHERE id=?"));
        q.addBindValue(m_itemId);
        if (db::exec(q) && q.next()) {
            const QJsonObject extra = QJsonDocument::fromJson(q.value(0).toByteArray()).object();
            for (const QJsonValue &v : extra.value("imported").toArray()) {
                const QString p = v.toString();
                if (!QFileInfo::exists(p) || source(QStringLiteral("file:") + p)) continue;
                Source s;
                s.id = QStringLiteral("file:") + p;
                s.kind = QStringLiteral("imported");
                s.path = p;
                s.label = QFileInfo(p).fileName();
                s.japanese = names::subtitleLang(p) != names::Lang::English;
                m_srcs.append(s);
            }
        }
        m_sources.clear();
        for (const Source &s : m_srcs)
            m_sources << QVariantMap{{"id", s.id}, {"label", s.label}, {"lang", s.lang}, {"kind", s.kind},
                                     {"japanese", s.japanese}, {"usable", s.kind != u"track" || s.stream.textSub()}};
        emit sourcesChanged();

        // Restore the last choice, else the most Japanese-looking one.
        QString primary, secondary;
        QSqlQuery q2(db::app());
        q2.prepare(QStringLiteral("SELECT sub_path, sub2_path FROM items WHERE id=?"));
        q2.addBindValue(m_itemId);
        bool restoring = false;
        if (db::exec(q2) && q2.next()) {
            primary = q2.value(0).toString();
            secondary = q2.value(1).toString();
            restoring = !primary.isEmpty() && source(primary);
        }
        if (!source(primary)) primary.clear();
        if (!source(secondary)) secondary.clear();
        if (primary.isEmpty()) {
            for (const Source &s : m_srcs)
                if (s.japanese && (s.kind != u"track" || s.stream.textSub())) {
                    primary = s.id;
                    break;
                }
        }
        if (primary.isEmpty()) {
            // Unlabelled files: take the best-matching one; its text decides below.
            for (const Source &s : m_srcs)
                if (s.lang.isEmpty() && (s.kind != u"track" || s.stream.textSub())) {
                    primary = s.id;
                    break;
                }
        }
        if (secondary.isEmpty()) {
            for (const Source &s : m_srcs)
                if (!s.japanese && s.id != primary && (s.lang == u"en" || s.lang == u"eng") && (s.kind != u"track" || s.stream.textSub())) {
                    secondary = s.id;
                    break;
                }
        }
        if (const Source *s = source(primary)) loadPrimary(*s, restoring);
        if (const Source *s = source(secondary)) loadSecondary(*s);
    });
    w->setFuture(QtConcurrent::run([path] { return std::make_pair(media::probe(path), names::subtitlesFor(path)); }));
}

void PlaybackSession::selectPrimary(const QString &id)
{
    if (id.isEmpty() || id == u"none") {
        m_primaryId.clear();
        setLines1({}, {});
        saveItem();
        return;
    }
    if (const Source *s = source(id)) {
        // A different subtitle file starts from a clean timing.
        if (id != m_primaryId) {
            m_offset = 0;
            m_pinned = m_synced = false;
            emit offsetChanged();
        }
        loadPrimary(*s, false);
    }
}

void PlaybackSession::selectSecondary(const QString &id)
{
    if (id.isEmpty() || id == u"none") {
        m_secondaryId.clear();
        m_lines2.clear();
        rebuildText2Column();
        rebuildCurrent();
        emit secondaryChanged();
        saveItem();
        return;
    }
    if (const Source *s = source(id)) loadSecondary(*s);
}

namespace {

struct Loaded {
    QList<SubLine> lines;
    QString signsAss;
    QList<QList<jp::Token>> tokens;
    QList<QVariantList> display;
    QString error;
};

Loaded loadSource(const QString &mediaPath, const QString &path, const media::Stream &stream, bool isTrack, bool tokenize)
{
    Loaded out;
    QString file = path;
    if (isTrack) {
        if (!stream.textSub()) {
            out.error = QObject::tr("This track is made of images; Togu can only read text subtitles.");
            return out;
        }
        file = media::extractSubtitle(mediaPath, stream);
        if (file.isEmpty()) {
            out.error = QObject::tr("Could not read the subtitle track.");
            return out;
        }
    }
    const QString text = subs::readText(file);
    const QString ext = QFileInfo(file).suffix().toLower();
    if (ext == u"ass" || ext == u"ssa") {
        auto split = subs::splitAss(text);
        out.lines = subs::normalize(split.dialogue);
        out.signsAss = split.signsAss;
    } else {
        out.lines = subs::normalize(subs::parse(file, text));
    }
    if (out.lines.isEmpty()) out.error = QObject::tr("No lines found in this subtitle.");
    if (tokenize) {
        const bool tidy = true;
        for (const SubLine &l : out.lines) out.tokens.append(jp::tokenize(tidy ? subs::tidyLine(l.text) : l.text));
        const QSet<QString> rec = display::recommendedKeys(out.tokens);
        for (const auto &t : out.tokens) out.display.append(display::tokens(t, rec));
    }
    return out;
}

} // namespace

void PlaybackSession::loadPrimary(const Source &src, bool restoring)
{
    Q_UNUSED(restoring)
    const int gen = m_generation;
    ++m_loading;
    emit loadingChanged();
    auto *w = new QFutureWatcher<Loaded>(this);
    const QString id = src.id;
    connect(w, &QFutureWatcherBase::finished, this, [this, w, gen, id] {
        w->deleteLater();
        --m_loading;
        emit loadingChanged();
        if (gen != m_generation) return;
        Loaded r = w->result();
        if (!r.error.isEmpty()) {
            emit toast(r.error, QStringLiteral("pink"));
            return;
        }
        m_primaryId = id;
        m_tokens1 = r.tokens;
        m_display1 = r.display;
        setLines1(r.lines, r.signsAss);
        saveItem();
        if (!subs::looksJapanese(r.lines))
            emit toast(tr("This subtitle doesn't look Japanese. Pick another one from the subtitle menu."), QStringLiteral("orange"));
        if (Settings::v("autoSync").toBool() && !m_pinned && !m_synced) startAutoSync(false);
    });
    w->setFuture(QtConcurrent::run([m = m_path, p = src.path, s = src.stream, t = src.kind == u"track"] { return loadSource(m, p, s, t, true); }));
}

void PlaybackSession::loadSecondary(const Source &src)
{
    const int gen = m_generation;
    ++m_loading;
    emit loadingChanged();
    auto *w = new QFutureWatcher<Loaded>(this);
    const QString id = src.id;
    connect(w, &QFutureWatcherBase::finished, this, [this, w, gen, id] {
        w->deleteLater();
        --m_loading;
        emit loadingChanged();
        if (gen != m_generation) return;
        Loaded r = w->result();
        if (!r.error.isEmpty()) {
            emit toast(r.error, QStringLiteral("pink"));
            return;
        }
        m_secondaryId = id;
        m_lines2 = r.lines;
        rebuildText2Column();
        rebuildCurrent();
        emit secondaryChanged();
        saveItem();
    });
    w->setFuture(QtConcurrent::run([m = m_path, p = src.path, s = src.stream, t = src.kind == u"track"] { return loadSource(m, p, s, t, false); }));
}

void PlaybackSession::setLines1(const QList<SubLine> &lines, const QString &signsAss)
{
    m_lines1 = lines;
    if (lines.isEmpty()) {
        m_tokens1.clear();
        m_display1.clear();
    }
    m_signsPath.clear();
    if (!signsAss.isEmpty()) {
        const QString dir = db::cacheDir() + QStringLiteral("/signs");
        QDir().mkpath(dir);
        m_signsPath = dir + u'/' + QString::fromLatin1(QCryptographicHash::hash(m_primaryId.toUtf8(), QCryptographicHash::Sha1).toHex().left(16)) + QStringLiteral(".ass");
        QFile f(m_signsPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(signsAss.toUtf8());
    }
    applySignsToMpv();
    m_current = m_last = -1;
    QStringList text2;
    m_model.reset(m_lines1, m_display1, text2, m_offset);
    rebuildText2Column();
    emit primaryChanged();
    onPosition();
    rebuildCurrent();
    updateStats();
}

void PlaybackSession::applySignsToMpv()
{
    if (!m_player || !m_player->loaded()) return;
    // Drop tracks we added before, then add the signs file (if any) as the visible mpv track.
    for (const QVariant &t : m_player->tracks()) {
        const QVariantMap m = t.toMap();
        if (m.value("type").toString() == u"sub" && m.value("external").toBool())
            m_player->command({QStringLiteral("sub-remove"), m.value("id").toString()});
    }
    if (m_signsPath.isEmpty()) {
        m_player->set(QStringLiteral("sid"), QStringLiteral("no"));
        return;
    }
    m_player->command({QStringLiteral("sub-add"), m_signsPath, QStringLiteral("select"), QStringLiteral("togu-signs")});
    m_player->set(QStringLiteral("sub-delay"), m_offset);
    m_player->set(QStringLiteral("sub-visibility"), true);
}

void PlaybackSession::onPosition()
{
    if (!m_player) return;
    const double pos = m_player->position();
    const double t = pos - m_offset;
    const int idx = subs::lineAt(m_lines1, t);
    const int last = subs::lineBefore(m_lines1, t);
    // Auto-pause at the end of a line, once per line.
    if (Settings::v("autoPause").toString() == u"end" && m_pauseArmed >= 0 && m_pauseArmed < m_lines1.size() &&
        t >= m_lines1[m_pauseArmed].end - 0.03 && !m_player->paused()) {
        const int ended = m_pauseArmed;
        m_pauseArmed = -1;
        m_player->setPaused(true);
        emit lineEnded(ended);
    }
    if (idx >= 0 && idx != m_current) m_pauseArmed = idx;
    if (idx != m_current || last != m_last) {
        m_current = idx;
        m_last = last;
        rebuildCurrent();
        emit currentChanged();
    }
    // The secondary line follows its own timing.
    if (!m_lines2.isEmpty()) {
        const int i2 = subs::lineAt(m_lines2, pos - m_offset2);
        const QString t2 = i2 >= 0 ? m_lines2[i2].text : QString();
        if (t2 != m_currentText2) {
            m_currentText2 = t2;
            emit current2Changed();
        }
    }
}

void PlaybackSession::rebuildCurrent()
{
    m_currentTokens = m_current >= 0 && m_current < m_display1.size() ? m_display1[m_current] : QVariantList();
    emit currentTokensChanged();
    if (m_lines2.isEmpty() && !m_currentText2.isEmpty()) {
        m_currentText2.clear();
        emit current2Changed();
    }
}

void PlaybackSession::rebuildText2Column()
{
    QStringList t2;
    if (!m_lines2.isEmpty())
        for (const SubLine &l : m_lines1) {
            SubLine shifted{l.start + m_offset, l.end + m_offset, l.text};
            t2 << subs::overlapText(shifted, m_lines2, m_offset2);
        }
    m_model.setText2(t2);
}

QString PlaybackSession::currentText() const
{
    return m_current >= 0 && m_current < m_lines1.size() ? m_lines1[m_current].text : QString();
}

void PlaybackSession::updateStats()
{
    if (m_tokens1.isEmpty()) {
        m_stats.clear();
        emit statsChanged();
        return;
    }
    QList<QList<ScoredToken>> groups;
    for (const QVariantList &line : m_display1) {
        QList<ScoredToken> g;
        for (const QVariant &v : line) {
            const QVariantMap m = v.toMap();
            g.append({m.value("s").toString(), m.value("k").toString(), m.value("w").toBool(),
                      statusFromName(m.value("st").toString()), m.value("rank").toInt(), m.value("c").toBool()});
        }
        groups << g;
    }
    const Analysis a = analyze(groups, Words::instance().knownCount());
    const PageStats &s = a.stats;
    const Difficulty d = difficulty(s.comprehension);
    m_stats = {{"comprehension", s.comprehension}, {"words", s.words}, {"known", s.uniqueKnown}, {"learning", s.uniqueLearning},
               {"unknown", s.uniqueUnknown}, {"ignored", s.uniqueIgnored}, {"sentences", s.sentences},
               {"recommended", s.recommended}, {"label", d.label}, {"tone", d.tone}};
    emit statsChanged();
    if (m_itemId)
        db::exec(db::app(), QStringLiteral("UPDATE items SET comprehension=?, words=? WHERE id=?"), {s.comprehension, s.words, m_itemId});
}

void PlaybackSession::importSubtitle(const QUrl &file, bool secondary)
{
    QString p = file.isLocalFile() ? file.toLocalFile() : file.toString();
    if (!QFileInfo::exists(p)) return;
    if (!names::isSubtitle(p)) {
        // An archive of subtitles: keep them all, take the one for this episode.
        const double ep = names::parse(m_path).episode;
        const QString dir = db::dataDir() + QStringLiteral("/subtitles/") + QFileInfo(p).completeBaseName();
        QDir().mkpath(dir);
        QString best;
        int bestScore = -1;
        arc::each(p, [](const QString &n) { return names::isSubtitle(n); }, [&](const QString &n, const QByteArray &bytes) {
            const QString out = dir + u'/' + QFileInfo(n).fileName();
            QFile f(out);
            if (f.open(QIODevice::WriteOnly)) f.write(bytes);
            int score = 0;
            if (ep >= 0 && names::parse(QFileInfo(n).fileName()).episode == ep) score += 10;
            const names::Lang lang = names::subtitleLang(n);
            if ((lang == names::Lang::Japanese) != secondary) score += 3;
            if (score > bestScore) {
                bestScore = score;
                best = out;
            }
            return true;
        });
        if (best.isEmpty()) {
            emit toast(tr("No subtitles in that file."), QStringLiteral("pink"));
            return;
        }
        p = best;
    }
    if (!source(QStringLiteral("file:") + p)) {
        Source s;
        s.id = QStringLiteral("file:") + p;
        s.kind = QStringLiteral("imported");
        s.path = p;
        s.label = QFileInfo(p).fileName();
        s.japanese = !secondary;
        m_srcs.append(s);
        m_sources << QVariantMap{{"id", s.id}, {"label", s.label}, {"lang", QString()}, {"kind", s.kind},
                                 {"japanese", s.japanese}, {"usable", true}};
        emit sourcesChanged();
        // Remember it for this item.
        QSqlQuery q(db::app());
        q.prepare(QStringLiteral("SELECT extra FROM items WHERE id=?"));
        q.addBindValue(m_itemId);
        QJsonObject extra;
        if (db::exec(q) && q.next()) extra = QJsonDocument::fromJson(q.value(0).toByteArray()).object();
        QJsonArray imported = extra.value("imported").toArray();
        if (!imported.contains(p)) imported.append(p);
        extra.insert("imported", imported);
        db::exec(db::app(), QStringLiteral("UPDATE items SET extra=? WHERE id=?"),
                 {QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact)), m_itemId});
    }
    if (secondary) selectSecondary(QStringLiteral("file:") + p);
    else selectPrimary(QStringLiteral("file:") + p);
}

void PlaybackSession::adjustOffset(double delta, bool secondary) { setOffset((secondary ? m_offset2 : m_offset) + delta, secondary); }

void PlaybackSession::setOffset(double value, bool secondary)
{
    value = std::round(value * 1000) / 1000;
    if (secondary) {
        m_offset2 = value;
    } else {
        m_offset = value;
        // A hand adjustment pins this video: auto-sync leaves it alone until asked again.
        m_pinned = true;
        if (m_syncState == u"running" || m_syncState == u"offered") {
            ++m_syncGeneration;
            m_syncState.clear();
            m_syncMessage.clear();
            emit syncChanged();
        }
        m_model.setOffset(m_offset);
        if (m_player) m_player->set(QStringLiteral("sub-delay"), m_offset);
    }
    rebuildText2Column();
    emit offsetChanged();
    onPosition();
    saveItem();
}

void PlaybackSession::lineStartsNow()
{
    if (!m_player || m_lines1.isEmpty()) return;
    const double pos = m_player->position();
    // The first line that hasn't started yet (or just started) is the one being heard.
    int i = subs::lineBefore(m_lines1, pos - m_offset - 1.0) + 1;
    i = std::clamp(i, 0, int(m_lines1.size()) - 1);
    setOffset(pos - m_lines1[i].start);
    emit toast(tr("Subtitles shifted to %1%2 s").arg(m_offset >= 0 ? "+" : "").arg(m_offset, 0, 'f', 2), QStringLiteral("blue"));
}

void PlaybackSession::runSync()
{
    m_pinned = false;
    emit offsetChanged();
    startAutoSync(true);
}

void PlaybackSession::startAutoSync(bool manual)
{
    if (m_lines1.isEmpty() || m_path.isEmpty()) return;
    const int gen = ++m_syncGeneration;
    m_syncState = QStringLiteral("running");
    m_syncMessage = tr("Checking subtitle timing…");
    emit syncChanged();
    int audioIndex = -1;
    if (m_player && m_player->audioTrack() > 0) {
        // mpv's audio ids count external tracks too; internal ones map onto ffmpeg's 0:a:N.
        for (const QVariant &t : m_player->tracks()) {
            const QVariantMap m = t.toMap();
            if (m.value("type").toString() == u"audio" && m.value("id").toInt() == m_player->audioTrack() && !m.value("external").toBool())
                audioIndex = m_player->audioTrack() - 1;
        }
    }
    auto *w = new QFutureWatcher<sync::Result>(this);
    connect(w, &QFutureWatcherBase::finished, this, [this, w, gen, manual] {
        w->deleteLater();
        if (gen != m_syncGeneration) return;
        const sync::Result r = w->result();
        m_pendingSync = r;
        const double change = r.offset - m_offset;
        auto fmt = [](double v) { return QStringLiteral("%1%2 s").arg(v >= 0 ? "+" : "").arg(v, 0, 'f', 2); };
        if (r.confidence <= 0.05) {
            m_syncState = QStringLiteral("failed");
            m_syncMessage = tr("Couldn't find the speech to line the subtitles up with.");
        } else if (r.split < 0 && std::abs(change) < 0.1 && r.ok()) {
            m_syncState = QStringLiteral("inSync");
            m_syncMessage = tr("Subtitles are in sync");
            m_synced = true;
        } else if (r.ok() && r.split < 0) {
            m_offset = r.offset;
            m_synced = true;
            m_model.setOffset(m_offset);
            if (m_player) m_player->set(QStringLiteral("sub-delay"), m_offset);
            rebuildText2Column();
            emit offsetChanged();
            onPosition();
            m_syncState = QStringLiteral("applied");
            m_syncMessage = tr("Subtitles shifted %1").arg(fmt(change));
        } else {
            m_syncState = QStringLiteral("offered");
            m_syncMessage = r.split >= 0
                ? tr("Timing changes at line %1: %2, then %3. Apply?").arg(r.split + 1).arg(fmt(r.offset), fmt(r.offset2))
                : tr("Subtitles may be %1 off. Apply?").arg(fmt(change));
        }
        Q_UNUSED(manual)
        saveItem();
        emit syncChanged();
    });
    w->setFuture(QtConcurrent::run([path = m_path, lines = m_lines1, audioIndex] {
        const QByteArray pcm = media::decodePcm16k(path, audioIndex);
        return sync::align(lines, sync::voiceActivity(pcm));
    }));
}

void PlaybackSession::acceptSync()
{
    if (m_syncState != u"offered") return;
    const sync::Result r = m_pendingSync;
    if (r.split >= 0) {
        // Bake the split into the lines; the stored offset then stays 0 for the shifted copy.
        QList<SubLine> shifted = sync::apply(m_lines1, r.offset, r.split, r.offset2);
        m_offset = 0;
        setLines1(shifted, {});
    } else {
        m_offset = r.offset;
        m_model.setOffset(m_offset);
        if (m_player) m_player->set(QStringLiteral("sub-delay"), m_offset);
    }
    m_synced = true;
    m_syncState = QStringLiteral("applied");
    m_syncMessage = tr("Timing applied");
    rebuildText2Column();
    emit offsetChanged();
    emit syncChanged();
    onPosition();
    saveItem();
}

void PlaybackSession::dismissSync()
{
    if (m_syncState == u"offered") m_synced = true; // asked once; don't ask again on reopen
    m_syncState.clear();
    m_syncMessage.clear();
    emit syncChanged();
    saveItem();
}

void PlaybackSession::replayLine()
{
    if (!m_player) return;
    const int i = m_current >= 0 ? m_current : m_last;
    if (i >= 0) seekToLine(i);
}

void PlaybackSession::prevLine()
{
    if (!m_player || m_lines1.isEmpty()) return;
    const double t = m_player->position() - m_offset;
    int i = subs::lineBefore(m_lines1, t);
    // Within the first second of a line, go to the one before it.
    if (i >= 0 && t - m_lines1[i].start < 1.0) --i;
    seekToLine(std::max(0, i));
}

void PlaybackSession::nextLine()
{
    if (!m_player || m_lines1.isEmpty()) return;
    const int i = subs::lineBefore(m_lines1, m_player->position() - m_offset) + 1;
    if (i < m_lines1.size()) seekToLine(i);
}

void PlaybackSession::seekToLine(int index)
{
    if (!m_player || index < 0 || index >= m_lines1.size()) return;
    m_pauseArmed = index;
    m_player->seek(m_lines1[index].start + m_offset);
    if (m_player->paused() && Settings::v("autoPause").toString() == u"end") m_player->setPaused(false);
}

QVariantMap PlaybackSession::line(int index) const
{
    if (index < 0 || index >= m_lines1.size()) return {};
    const SubLine &l = m_lines1[index];
    SubLine shifted{l.start + m_offset, l.end + m_offset, l.text};
    return {{"index", index}, {"start", shifted.start}, {"end", shifted.end}, {"text", l.text},
            {"tidy", subs::tidyLine(l.text)}, {"text2", subs::overlapText(shifted, m_lines2, m_offset2)}};
}

QVariantList PlaybackSession::tokensForLine(int index) const { return m_display1.value(index); }

QString PlaybackSession::textFrom(int lineIndex, int tokenIndex) const
{
    const QVariantList toks = m_display1.value(lineIndex);
    QString out;
    for (qsizetype i = tokenIndex; i < toks.size(); ++i) out += toks[i].toMap().value("s").toString();
    return out;
}

void PlaybackSession::toggleBookmark(int index)
{
    if (index < 0 || index >= m_lines1.size()) return;
    const SubLine &l = m_lines1[index];
    auto d = db::app();
    const int existing = db::scalar(d, QStringLiteral("SELECT id FROM bookmarks WHERE item=? AND abs(start-?)<0.01"), {m_itemId, l.start}).toInt();
    if (existing) db::exec(d, QStringLiteral("DELETE FROM bookmarks WHERE id=?"), {existing});
    else db::exec(d, QStringLiteral("INSERT INTO bookmarks (item, start, end, text, time) VALUES (?,?,?,?,?)"),
                  {m_itemId, l.start, l.end, l.text, QDateTime::currentSecsSinceEpoch()});
    QSet<int> rows;
    QSqlQuery q(d);
    q.prepare(QStringLiteral("SELECT start FROM bookmarks WHERE item=?"));
    q.addBindValue(m_itemId);
    db::exec(q);
    while (q.next()) {
        const double s = q.value(0).toDouble();
        for (int i = 0; i < m_lines1.size(); ++i)
            if (std::abs(m_lines1[i].start - s) < 0.01) rows.insert(i);
    }
    m_model.setBookmarks(rows);
    emit toast(existing ? tr("Bookmark removed") : tr("Line bookmarked"), QStringLiteral("blue"));
}

void PlaybackSession::queueLine(int index, const QString &word)
{
    if (index < 0 || index >= m_lines1.size()) return;
    const QVariantMap l = line(index);
    db::exec(db::app(), QStringLiteral("INSERT INTO queue (item, start, end, text, text2, word, time) VALUES (?,?,?,?,?,?,?)"),
             {m_itemId, l.value("start"), l.value("end"), l.value("text"), l.value("text2"), word, QDateTime::currentSecsSinceEpoch()});
    Queue::instance()->notifyChanged();
    emit toast(tr("Line added to the card queue"), QStringLiteral("purple"));
}

void PlaybackSession::savePosition()
{
    if (!m_player || !m_itemId || m_path.isEmpty()) return;
    const double pos = m_player->position(), dur = m_player->duration();
    if (dur <= 0) return;
    const bool watched = pos / dur > 0.9;
    db::exec(db::app(), QStringLiteral("UPDATE items SET position=?, duration=?, watched=MAX(watched, ?) WHERE id=?"),
             {watched ? 0.0 : pos, dur, watched ? 1 : 0, m_itemId});
}

void PlaybackSession::saveItem()
{
    if (!m_itemId) return;
    db::exec(db::app(), QStringLiteral("UPDATE items SET sub_path=?, sub2_path=?, sub_offset=?, sub_pinned=?, sub_synced=?, sub2_offset=? WHERE id=?"),
             {m_primaryId, m_secondaryId, m_offset, m_pinned ? 1 : 0, m_synced ? 1 : 0, m_offset2, m_itemId});
}

// Immersion time: seconds actually played, credited to the day and item.
void PlaybackSession::trackImmersion()
{
    if (!m_playClock.isValid() || !m_itemId) return;
    const double secs = m_playClock.restart() / 1000.0 * (m_player ? m_player->speed() : 1.0);
    if (secs <= 0 || secs > 600) return;
    const QString day = QDate::currentDate().toString(Qt::ISODate);
    db::exec(db::app(), QStringLiteral("INSERT INTO immersion (day, item, series, kind, seconds) VALUES (?, ?, (SELECT series FROM items WHERE id=?), ?, ?)"
                                       " ON CONFLICT(day, item, kind) DO UPDATE SET seconds=seconds+excluded.seconds"),
             {day, m_itemId, m_itemId, m_kind, secs});
}
