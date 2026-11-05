#pragma once
// What QML calls for words, lookups and dictionaries.

#include <QObject>
#include <QUrl>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

class Togu : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantMap counts READ counts NOTIFY countsChanged)
    Q_PROPERTY(QVariantList dicts READ dicts NOTIFY dictsChanged)
    Q_PROPERTY(bool dictBusy READ dictBusy NOTIFY dictProgressChanged)
    Q_PROPERTY(QString dictProgress READ dictProgress NOTIFY dictProgressChanged)
    Q_PROPERTY(double dictFraction READ dictFraction NOTIFY dictProgressChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QVariantList starterDicts READ starterDicts NOTIFY dictsChanged)

public:
    static Togu *instance();
    static Togu *create(QQmlEngine *, QJSEngine *);

    QVariantMap counts() const;
    QVariantList dicts() const;
    bool dictBusy() const { return m_dictBusy; }
    QString dictProgress() const { return m_dictProgress; }
    double dictFraction() const { return m_dictFraction; }
    QString version() const;

    Q_INVOKABLE QVariantList lookup(const QString &textFromWord);
    Q_INVOKABLE QVariantList kanji(const QString &text);
    Q_INVOKABLE QString status(const QString &key) const;
    Q_INVOKABLE void setStatus(const QVariant &keys, const QString &status);
    Q_INVOKABLE QVariantList tokenize(const QString &text);
    Q_INVOKABLE QString furiganaHtml(const QString &surface, const QString &reading) const;
    Q_INVOKABLE QVariantList pitchMorae(const QString &reading, int position) const;

    Q_INVOKABLE void importDicts(const QList<QUrl> &files);
    QVariantList starterDicts() const;
    // Downloads the chosen starter dictionaries from their usual sources, then imports them.
    Q_INVOKABLE void downloadDicts(const QStringList &ids);
    // First run: dictionaries a package placed in /usr/share/togu-player/dictionaries.
    void importBundled();
    Q_INVOKABLE void removeDict(int id);
    Q_INVOKABLE void setDictEnabled(int id, bool on);
    Q_INVOKABLE void moveDict(int id, int delta);

    Q_INVOKABLE void importWordList(const QString &text, const QString &status);
    Q_INVOKABLE QString exportWordList(const QString &status) const;
    // The Words page: saved words with a status, newest first or by frequency.
    Q_INVOKABLE QVariantMap wordList(const QString &status, const QString &query, const QString &sort, int limit) const;
    Q_INVOKABLE void clearWords();

    Q_INVOKABLE QString readTextFile(const QUrl &file) const;
    Q_INVOKABLE bool writeTextFile(const QUrl &file, const QString &text) const;
    Q_INVOKABLE QString localPath(const QUrl &url) const { return url.toLocalFile(); }
    Q_INVOKABLE QUrl fileUrl(const QString &path) const { return QUrl::fromLocalFile(path); }
    Q_INVOKABLE bool exists(const QString &path) const;
    Q_INVOKABLE void openExternally(const QString &urlOrPath) const;
    Q_INVOKABLE void copyText(const QString &text) const;
    Q_INVOKABLE QString formatTime(double seconds) const;

signals:
    void statusesChanged(const QStringList &keys);
    void countsChanged();
    void dictsChanged();
    void dictProgressChanged();
    void dictImported(const QString &title, const QString &error);
    void toast(const QString &text, const QString &tone);

private:
    void runImport(const QStringList &paths, double base, double span, bool removeAfter);
    explicit Togu(QObject *parent = nullptr);
    class QNetworkAccessManager *m_net = nullptr;
    bool m_dictBusy = false;
    QString m_dictProgress;
    double m_dictFraction = 0;
};
