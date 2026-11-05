#pragma once
// The library as QML sees it: folders, series grids, episodes, continue watching,
// metadata and the background scan → match → index → score pipeline.

#include <QFileSystemWatcher>
#include <QObject>
#include <QQuickImageProvider>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

#include <atomic>

class QQmlEngine;
class QJSEngine;

class Library : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY busyChanged)
    Q_PROPERTY(int revision READ revision NOTIFY changed)

public:
    static Library *instance();
    static Library *create(QQmlEngine *, QJSEngine *);

    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    int revision() const { return m_revision; }

    Q_INVOKABLE QVariantList roots(const QString &kind) const;
    Q_INVOKABLE void addRoot(const QUrl &folder, const QString &kind);
    Q_INVOKABLE void removeRoot(int id);
    Q_INVOKABLE void rescan();

    // sort: recent, title, comprehension, added
    Q_INVOKABLE QVariantList series(const QString &kind, const QString &sort = QStringLiteral("recent"), const QString &filter = {}) const;
    Q_INVOKABLE QVariantMap seriesInfo(int id) const;
    Q_INVOKABLE QVariantList items(int seriesId) const;
    Q_INVOKABLE QVariantList continueItems(const QString &kind, int limit = 12) const;
    Q_INVOKABLE QVariantMap item(int id) const;
    Q_INVOKABLE int nextItem(int itemId) const;
    Q_INVOKABLE void setWatched(int itemId, bool watched);
    Q_INVOKABLE void setSeriesWatched(int seriesId, bool watched);
    Q_INVOKABLE QVariantList linesWithWord(const QString &key, int limit = 30) const;
    // Books or manga volumes with cover and reading progress, grouped by series.
    Q_INVOKABLE QVariantList shelf(const QString &kind, const QString &filter = {}) const;

    Q_INVOKABLE void searchMeta(int seriesId, const QString &query);
    Q_INVOKABLE void applyMeta(int seriesId, const QVariantMap &candidate);
    Q_INVOKABLE void clearMeta(int seriesId);
    Q_INVOKABLE void refreshMeta(int seriesId);
    // Condensed audio: each episode's dialogue (from its indexed subtitle lines) joined
    // into one file per episode, written to `folder`.
    Q_INVOKABLE void exportCondensed(int seriesId, const QUrl &folder);

signals:
    void busyChanged();
    void changed();
    void metaResults(int seriesId, const QVariantList &results);
    void condensedProgress(int done, int total);
    void condensedDone(const QString &folder, int written, int skipped);

private:
    explicit Library(QObject *parent = nullptr);
    void runPipeline(int rootId = 0);
    void setBusy(bool b, const QString &status = {});
    void updateWatches();
    void bump();

    QThreadPool m_pool;
    QFileSystemWatcher m_watch;
    QTimer m_watchDebounce, m_rescoreDebounce;
    bool m_busy = false;
    QString m_status;
    int m_revision = 0;
    std::atomic_bool m_cancel{false};
    bool m_pending = false;
};

// image://thumb/<itemId> — a frame from a quarter of the way into the video.
// Images using it set `asynchronous: true`, so this runs off the GUI thread.
class ThumbProvider : public QQuickImageProvider {
public:
    ThumbProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
