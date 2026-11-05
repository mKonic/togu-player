#pragma once
// One playback: the file in mpv, its subtitle tracks, timing, the current line and
// the line list. Video and audio files both play through this.

#include "core/jp/tokenizer.h"
#include "core/media/probe.h"
#include "core/subs/subs.h"
#include "core/subs/sync.h"

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class MpvItem;

class LineModel : public QAbstractListModel {
    Q_OBJECT
    QML_ANONYMOUS
public:
    enum Roles { StartRole = Qt::UserRole + 1, EndRole, TextRole, Text2Role, TokensRole, BookmarkRole };
    using QAbstractListModel::QAbstractListModel;
    int rowCount(const QModelIndex &p = {}) const override { return p.isValid() ? 0 : int(m_lines.size()); }
    QVariant data(const QModelIndex &i, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void reset(const QList<SubLine> &lines, const QList<QVariantList> &tokens, const QStringList &text2, double offset);
    void setTokens(const QList<QVariantList> &tokens);
    void setText2(const QStringList &text2);
    void setOffset(double offset);
    void setBookmarks(const QSet<int> &rows);
    void refreshStatuses();

private:
    QList<SubLine> m_lines;
    QList<QVariantList> m_tokens;
    QStringList m_text2;
    QSet<int> m_bookmarks;
    double m_offset = 0;
};

class PlaybackSession : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(MpvItem *player READ player WRITE setPlayer NOTIFY playerChanged)
    Q_PROPERTY(QString path READ path NOTIFY opened)
    Q_PROPERTY(int itemId READ itemId NOTIFY opened)
    Q_PROPERTY(QString title READ title NOTIFY opened)
    Q_PROPERTY(QString kind READ kind NOTIFY opened)
    Q_PROPERTY(QString coverUrl READ coverUrl NOTIFY opened)
    Q_PROPERTY(QVariantList subtitleSources READ subtitleSources NOTIFY sourcesChanged)
    Q_PROPERTY(QString primaryId READ primaryId NOTIFY primaryChanged)
    Q_PROPERTY(QString secondaryId READ secondaryId NOTIFY secondaryChanged)
    Q_PROPERTY(bool loadingSubs READ loadingSubs NOTIFY loadingChanged)
    Q_PROPERTY(int lineCount READ lineCount NOTIFY primaryChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(int lastIndex READ lastIndex NOTIFY currentChanged)
    Q_PROPERTY(QVariantList currentTokens READ currentTokens NOTIFY currentTokensChanged)
    Q_PROPERTY(QString currentText READ currentText NOTIFY currentChanged)
    Q_PROPERTY(QString currentText2 READ currentText2 NOTIFY current2Changed)
    Q_PROPERTY(double offset READ offset NOTIFY offsetChanged)
    Q_PROPERTY(double offset2 READ offset2 NOTIFY offsetChanged)
    Q_PROPERTY(bool pinned READ pinned NOTIFY offsetChanged)
    Q_PROPERTY(QString syncState READ syncState NOTIFY syncChanged)
    Q_PROPERTY(QString syncMessage READ syncMessage NOTIFY syncChanged)
    Q_PROPERTY(LineModel *lines READ lines CONSTANT)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)
    Q_PROPERTY(bool hasSigns READ hasSigns NOTIFY primaryChanged)

public:
    explicit PlaybackSession(QObject *parent = nullptr);
    ~PlaybackSession() override;

    MpvItem *player() const { return m_player; }
    void setPlayer(MpvItem *p);
    QString path() const { return m_path; }
    int itemId() const { return m_itemId; }
    QString title() const { return m_title; }
    QString kind() const { return m_kind; }
    QString coverUrl() const { return m_cover; }
    QVariantList subtitleSources() const { return m_sources; }
    QString primaryId() const { return m_primaryId; }
    QString secondaryId() const { return m_secondaryId; }
    bool loadingSubs() const { return m_loading > 0; }
    int lineCount() const { return int(m_lines1.size()); }
    int currentIndex() const { return m_current; }
    int lastIndex() const { return m_last; }
    QVariantList currentTokens() const { return m_currentTokens; }
    QString currentText() const;
    QString currentText2() const { return m_currentText2; }
    double offset() const { return m_offset; }
    double offset2() const { return m_offset2; }
    bool pinned() const { return m_pinned; }
    QString syncState() const { return m_syncState; }
    QString syncMessage() const { return m_syncMessage; }
    LineModel *lines() { return &m_model; }
    QVariantMap stats() const { return m_stats; }
    bool hasSigns() const { return !m_signsPath.isEmpty(); }

    Q_INVOKABLE void open(const QString &pathOrUrl, int itemId = 0);
    Q_INVOKABLE void openItem(int itemId);
    Q_INVOKABLE void close();
    Q_INVOKABLE void selectPrimary(const QString &id);
    Q_INVOKABLE void selectSecondary(const QString &id);
    Q_INVOKABLE void importSubtitle(const QUrl &file, bool secondary = false);
    Q_INVOKABLE void adjustOffset(double delta, bool secondary = false);
    Q_INVOKABLE void setOffset(double value, bool secondary = false);
    Q_INVOKABLE void lineStartsNow();
    Q_INVOKABLE void runSync();
    Q_INVOKABLE void acceptSync();
    Q_INVOKABLE void dismissSync();
    Q_INVOKABLE void replayLine();
    Q_INVOKABLE void prevLine();
    Q_INVOKABLE void nextLine();
    Q_INVOKABLE void seekToLine(int index);
    Q_INVOKABLE QVariantMap line(int index) const;
    Q_INVOKABLE QVariantList tokensForLine(int index) const;
    // Text from token `tokenIndex` to the end of the line, for lookups.
    Q_INVOKABLE QString textFrom(int lineIndex, int tokenIndex) const;
    Q_INVOKABLE void toggleBookmark(int index);
    Q_INVOKABLE void queueLine(int index, const QString &word = {});
    Q_INVOKABLE void savePosition();

signals:
    void playerChanged();
    void opened();
    void sourcesChanged();
    void primaryChanged();
    void secondaryChanged();
    void loadingChanged();
    void currentChanged();
    void currentTokensChanged();
    void current2Changed();
    void offsetChanged();
    void syncChanged();
    void statsChanged();
    void toast(const QString &text, const QString &tone);
    void lineEnded(int index); // auto-pause hook
    void finished();

private:
    struct Source {
        QString id, label, lang, kind, path; // kind: file, track, imported
        media::Stream stream;
        bool japanese = false;
    };
    void onPosition();
    void probeAndChooseSubs();
    void loadPrimary(const Source &src, bool restoring);
    void loadSecondary(const Source &src);
    void setLines1(const QList<SubLine> &lines, const QString &signsAss);
    void rebuildCurrent();
    void rebuildText2Column();
    void updateStats();
    void applySignsToMpv();
    void saveItem();
    void startAutoSync(bool manual);
    const Source *source(const QString &id) const;
    void trackImmersion();

    QPointer<MpvItem> m_player;
    QString m_path, m_title, m_kind = QStringLiteral("video"), m_cover;
    int m_itemId = 0;
    media::Probe m_probe;
    QList<Source> m_srcs;
    QVariantList m_sources;
    QString m_primaryId, m_secondaryId;
    int m_loading = 0;

    QList<SubLine> m_lines1, m_lines2;
    QList<QList<jp::Token>> m_tokens1;
    QList<QVariantList> m_display1;
    QString m_signsPath;
    int m_current = -1, m_last = -1;
    QVariantList m_currentTokens;
    QString m_currentText2;
    double m_offset = 0, m_offset2 = 0;
    bool m_pinned = false, m_synced = false;
    sync::Result m_pendingSync;
    QString m_syncState, m_syncMessage;
    int m_syncGeneration = 0;
    LineModel m_model;
    QVariantMap m_stats;
    int m_pauseArmed = -1;
    double m_restorePosition = -1;
    QTimer m_saveTimer;
    QElapsedTimer m_playClock;
    double m_immersionPending = 0;
    QSet<QString> m_recommended;
    int m_generation = 0;
};
