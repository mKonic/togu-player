#pragma once
// Card creation for QML: drafts from a word and a line, media cut from the file,
// sending to Anki, adding media to the last note, and the "has a card" marks.

#include <QObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class Cards : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool connected READ connected NOTIFY ankiChanged)
    Q_PROPERTY(QString ankiError READ ankiError NOTIFY ankiChanged)
    Q_PROPERTY(QStringList decks READ decks NOTIFY ankiChanged)
    Q_PROPERTY(QStringList models READ models NOTIFY ankiChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList markers READ markers CONSTANT)

public:
    static Cards *instance();
    static Cards *create(QQmlEngine *, QJSEngine *);

    bool connected() const { return m_connected; }
    QString ankiError() const { return m_error; }
    QStringList decks() const { return m_decks; }
    QStringList models() const { return m_models; }
    bool busy() const { return m_busy > 0; }
    QVariantList markers() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void modelFields(const QString &model);
    Q_INVOKABLE QVariantMap guessFields(const QStringList &fields, const QString &model) const;

    // A draft from a dictionary entry (may be empty for a sentence card) and a line from the session.
    Q_INVOKABLE QVariantMap draft(const QVariant &entry, const QVariantMap &line, const QString &surface, int itemId, double offset);
    // Cuts audio and picture for a line. Emits mediaReady(token, map).
    Q_INVOKABLE int cutMedia(const QString &path, double start, double end, int audioTrack, const QString &imageMode, double frameAt);
    Q_INVOKABLE int cutAudio(const QString &path, double start, double end, int audioTrack);
    Q_INVOKABLE int cutImage(const QString &path, double start, double end, const QString &imageMode, double frameAt);
    Q_INVOKABLE QVariantList wordAudio(const QString &expression, const QString &reading) const;
    Q_INVOKABLE QString saveGrab(const QVariant &image) const; // QML grabToImage result -> file
    Q_INVOKABLE void add(const QVariantMap &draft);
    Q_INVOKABLE void addMediaToLast(const QVariantMap &draft);
    Q_INVOKABLE void browse(qlonglong noteId);
    Q_INVOKABLE QVariantList history(int limit = 50) const;
    Q_INVOKABLE void syncCardMarks();
    // Word statuses from the chosen learning decks (statussync).
    Q_INVOKABLE void syncStatuses(bool quiet = true);
    Q_PROPERTY(QString statusSyncReport READ statusSyncReport NOTIFY statusSyncChanged)
    Q_PROPERTY(bool statusSyncing READ statusSyncing NOTIFY statusSyncChanged)
    QString statusSyncReport() const { return m_syncReport; }
    bool statusSyncing() const { return m_syncing; }
    // Field names of every note type, for the status settings.
    Q_INVOKABLE void allModelFields();

signals:
    void ankiChanged();
    void busyChanged();
    void fieldsReady(const QString &model, const QStringList &fields);
    void mediaReady(int token, const QVariantMap &media);
    void added(qlonglong noteId, const QString &expression);
    void failed(const QString &message);
    void toast(const QString &text, const QString &tone);
    void statusSyncChanged();
    void allFieldsReady(const QVariantMap &modelFields);

private:
    explicit Cards(QObject *parent = nullptr);
    void busyInc(int d);
    bool m_connected = false;
    QString m_error;
    QStringList m_decks, m_models;
    int m_busy = 0;
    int m_token = 0;
    QString m_syncReport;
    bool m_syncing = false;
    class QTimer *m_syncTimer = nullptr;
};
