#pragma once
// AnkiConnect: talking to Anki, building notes from card drafts (ported from togu).

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace anki {

struct Conn {
    QString url = QStringLiteral("http://127.0.0.1:8765");
    QString key;
};

struct Result {
    QJsonValue value;
    QString error; // empty on success
    bool ok() const { return error.isEmpty(); }
};

// Blocking; call from a worker thread.
Result invoke(const Conn &c, const QString &action, const QJsonObject &params = {});

// Everything a card can hold. Media are local file paths.
struct Draft {
    QString expression, reading, surface;
    QString glossaryHtml, glossaryFirstHtml, glossaryText;
    QString sentence, sentenceTranslation, sentenceFurigana;
    QString source, notes, examples;
    QStringList images;
    QString sentenceAudio, wordAudio;
    QString frequency;
    int freqSort = 0;
    QString pitchPositions, pitchCategories;
};

// Field template markers, {marker}, with what they put in.
QVariantList markers();
// A template for a field from its name (Lapis, Kaishi, Migaku-style names...).
QString guessField(const QString &fieldName);
// Field templates for the Lapis note type.
QVariantMap lapisPreset();

struct Note {
    QJsonObject json; // AnkiConnect note object
    QStringList mediaFiles;
};
// fields: field name -> template
Note buildNote(const Draft &d, const QString &deck, const QString &model, const QVariantMap &fields,
               const QStringList &tags, bool allowDuplicates, qint64 stamp);

// Field text without HTML, furigana brackets resolved to the base text.
QString plainField(const QString &field);

} // namespace anki
