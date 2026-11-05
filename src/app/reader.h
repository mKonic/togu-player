#pragma once
// A book open in the reader: chapters as HTML with word spans, lookups, positions.

#include "core/book/book.h"
#include "core/jp/tokenizer.h"

#include <QHash>
#include <QObject>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class Reader : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString path READ path NOTIFY opened)
    Q_PROPERTY(QString title READ title NOTIFY opened)
    Q_PROPERTY(QString author READ author NOTIFY opened)
    Q_PROPERTY(QStringList chapterTitles READ chapterTitles NOTIFY opened)
    Q_PROPERTY(int chapter READ chapter NOTIFY chapterChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QVariantMap stats READ stats NOTIFY statsChanged)
    Q_PROPERTY(double restoreFraction READ restoreFraction NOTIFY chapterChanged)

public:
    explicit Reader(QObject *parent = nullptr);
    QString path() const { return m_book.path; }
    QString title() const { return m_book.title; }
    QString author() const { return m_book.author; }
    QStringList chapterTitles() const;
    int chapter() const { return m_chapter; }
    bool loading() const { return m_loading; }
    QVariantMap stats() const { return m_stats; }
    double restoreFraction() const { return m_restore; }

    Q_INVOKABLE void open(const QString &path);
    Q_INVOKABLE void showChapter(int index, double fraction = 0);
    Q_INVOKABLE QVariantMap token(int para, int tok) const;
    Q_INVOKABLE QString textFrom(int para, int tok) const;
    // The sentence around a token, for cards.
    Q_INVOKABLE QString sentenceAt(int para, int tok) const;
    Q_INVOKABLE void savePosition(double fraction);
    // Builds the page shell (styles, scripts) for the current settings.
    Q_INVOKABLE QString pageShell() const;

signals:
    void opened();
    void chapterChanged();
    void loadingChanged();
    void statsChanged();
    void chapterReady(const QString &bodyHtml);
    // key -> status letter (u, t, l, k, i) for words whose status changed
    void statusUpdates(const QVariantMap &updates);
    void failed(const QString &message);

private:
    QString bodyFor(int index);
    void updateStats();

    book::Book m_book;
    int m_chapter = 0;
    double m_restore = 0;
    bool m_loading = false;
    QVariantMap m_stats;
    QHash<int, QList<QList<jp::Token>>> m_tokens; // chapter -> paragraph tokens
    int m_generation = 0;
};
