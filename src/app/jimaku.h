#pragma once
// jimaku.cc: Japanese subtitles for anime, with the user's API key.

#include <QNetworkAccessManager>
#include <QObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class Jimaku : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    static Jimaku *create(QQmlEngine *, QJSEngine *);
    bool busy() const { return m_busy > 0; }

    // What to search for when the dialog opens for this item: title, AniList id, episode.
    Q_INVOKABLE QVariantMap guess(int itemId) const;
    Q_INVOKABLE void search(const QString &query, int anilistId);
    Q_INVOKABLE void files(int entryId, double episode);
    // Downloads a file; archives are opened and the subtitle for `episode` is picked.
    // Emits downloaded(path) with a ready subtitle file.
    Q_INVOKABLE void download(const QString &url, const QString &name, int entryId, double episode);

signals:
    void busyChanged();
    void searchResults(const QVariantList &entries);
    void fileResults(const QVariantList &files);
    void downloaded(const QString &path);
    void failed(const QString &message);

private:
    explicit Jimaku(QObject *parent = nullptr);
    class QNetworkReply *get(const QUrl &url);
    QNetworkAccessManager m_net;
    int m_busy = 0;
};
