#pragma once
// The learner's word list: one status per word key (dictionary form + reading).

#include <QHash>
#include <QObject>
#include <QReadWriteLock>
#include <QStringList>

enum class Status : int { Unknown = 0, Tracked = 1, Learning = 2, Known = 3, Ignored = 4 };

inline size_t qHash(Status s, size_t seed = 0) noexcept { return ::qHash(int(s), seed); }
QString statusName(Status s);
Status statusFromName(const QString &s);

class Words : public QObject {
    Q_OBJECT
public:
    static Words &instance();

    void load();
    // A word saved without a reading (a pasted list) stands for every reading of that spelling.
    Status status(const QString &key) const;
    bool manual(const QString &key) const;

    // Set by hand in the app. Anki-managed words remember the card's modification time
    // so the hand setting holds until the card is reviewed again.
    void setManual(const QList<std::pair<QString, Status>> &entries);
    // Set by Anki sync: does not override a hand setting whose anki_mod matches `mod`.
    struct AnkiState {
        QString key;
        Status status;
        qint64 mod;
    };
    void applyAnki(const QList<AnkiState> &states);
    void importList(const QStringList &keys, Status s);
    void clear();

    QHash<Status, int> counts() const;
    int knownCount() const;
    QList<std::pair<QString, Status>> all() const;

signals:
    void changed(const QStringList &keys); // empty: everything may have changed

private:
    struct Row {
        Status s;
        bool manual;
        qint64 ankiMod;
    };
    mutable QReadWriteLock m_lock;
    QHash<QString, Row> m_rows;
    QHash<QString, qint64> m_cardMod; // latest Anki card mod time per word, for manual overrides
    bool m_loaded = false;
};
