#pragma once

#include <QDBusContext>
#include <QHash>
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

// The desktop's own file chooser, through the xdg-desktop-portal FileChooser
// interface. Qt only uses it when the platform theme offers it, and many
// themes don't, which leaves Qt's bare built-in picker.
class Portal : public QObject, protected QDBusContext {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Portal(QObject *parent = nullptr);

    // opts: title, folder, multiple, save, currentFolder (url), currentName,
    // filters (["Video (*.mkv *.mp4)", ...]). Returns a request id; the answer
    // comes as finished() (empty urls on cancel) or failed() (no portal).
    Q_INVOKABLE int open(const QVariantMap &opts);

signals:
    void finished(int id, const QStringList &urls);
    void failed(int id);

private slots:
    void onResponse(uint response, const QVariantMap &results);

private:
    void listen(const QString &path, int id);
    void forget(const QString &path);

    int m_next = 1;
    QHash<QString, int> m_pending;  // request object path → id
};
