#include "anki.h"

#include "core/jp/jp.h"

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimer>

namespace anki {

Result invoke(const Conn &c, const QString &action, const QJsonObject &params)
{
    QNetworkAccessManager net;
    QNetworkRequest req{QUrl(c.url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    QJsonObject body{{"action", action}, {"version", 6}, {"params", params}};
    if (!c.key.isEmpty()) body.insert("key", c.key);
    QNetworkReply *r = net.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, r, &QNetworkReply::abort);
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(action == u"addNote" || action == u"storeMediaFile" ? 60000 : 15000);
    loop.exec();
    Result out;
    if (r->error() != QNetworkReply::NoError) {
        out.error = QObject::tr("Could not reach Anki. Is it running with AnkiConnect installed?");
    } else {
        const QJsonObject res = QJsonDocument::fromJson(r->readAll()).object();
        if (!res.value("error").isNull() && res.contains("error")) out.error = res.value("error").toString();
        out.value = res.value("result");
    }
    r->deleteLater();
    return out;
}

QVariantList markers()
{
    static const QVariantList m = {
        QVariantMap{{"key", "expression"}, {"about", "The word in dictionary form"}},
        QVariantMap{{"key", "reading"}, {"about", "Reading in kana"}},
        QVariantMap{{"key", "furigana"}, {"about", "Word with furigana, 言葉[ことば]"}},
        QVariantMap{{"key", "furigana-html"}, {"about", "Word with furigana as ruby text"}},
        QVariantMap{{"key", "glossary"}, {"about", "Definitions from every dictionary"}},
        QVariantMap{{"key", "glossary-first"}, {"about", "Definitions from the first dictionary"}},
        QVariantMap{{"key", "glossary-plain"}, {"about", "Definitions as plain text"}},
        QVariantMap{{"key", "sentence"}, {"about", "The subtitle line"}},
        QVariantMap{{"key", "sentence-bold"}, {"about", "The line with the word in bold"}},
        QVariantMap{{"key", "sentence-furigana"}, {"about", "The line with furigana"}},
        QVariantMap{{"key", "sentence-translation"}, {"about", "The second subtitles' line"}},
        QVariantMap{{"key", "audio"}, {"about", "Audio of the word"}},
        QVariantMap{{"key", "sentence-audio"}, {"about", "Audio of the line, cut from the file"}},
        QVariantMap{{"key", "image"}, {"about", "Screenshot, animated image or clip"}},
        QVariantMap{{"key", "frequency"}, {"about", "Frequency rankings"}},
        QVariantMap{{"key", "freq-sort"}, {"about", "Lowest frequency rank as a number"}},
        QVariantMap{{"key", "pitch"}, {"about", "Pitch accent positions"}},
        QVariantMap{{"key", "pitch-categories"}, {"about", "heiban, atamadaka, nakadaka, odaka, kifuku"}},
        QVariantMap{{"key", "source"}, {"about", "Show, episode and time"}},
        QVariantMap{{"key", "example-sentences"}, {"about", "Extra example sentences"}},
        QVariantMap{{"key", "notes"}, {"about", "Your notes"}},
    };
    return m;
}

QString guessField(const QString &name)
{
    QString n = name.toLower();
    n.remove(QRegularExpression(QStringLiteral("[\\s_-]+")));
    auto has = [&](std::initializer_list<const char *> keys) {
        for (const char *k : keys)
            if (n.contains(QLatin1String(k))) return true;
        return false;
    };
    if (QRegularExpression(QStringLiteral("^is[a-z]+card$")).match(n).hasMatch() || has({"categories", "hint", "selection"}))
        return n == u"pitchcategories" ? QStringLiteral("{pitch-categories}") : QString();
    if (has({"sentenceaudio"})) return QStringLiteral("{sentence-audio}");
    if (has({"sentencefurigana", "sentencereading"})) return QStringLiteral("{sentence-furigana}");
    if (has({"sentencetranslation", "sentenceenglish", "sentencemeaning", "translation"})) return QStringLiteral("{sentence-translation}");
    if (has({"examplesentence", "examples"})) return QStringLiteral("{example-sentences}");
    if (has({"sentence", "example", "context"})) return QStringLiteral("{sentence-bold}");
    if (has({"expressionfurigana", "wordfurigana", "furigana"})) return QStringLiteral("{furigana}");
    if (has({"expressionreading", "wordreading", "reading", "kana"})) return QStringLiteral("{reading}");
    if (has({"expressionaudio", "wordaudio", "audio", "sound"})) return QStringLiteral("{audio}");
    if (has({"freqsort", "frequencysort"})) return QStringLiteral("{freq-sort}");
    if (has({"frequency", "freq"})) return QStringLiteral("{frequency}");
    if (has({"pitchposition", "pitch", "accent"})) return n.contains(u"note") ? QString() : QStringLiteral("{pitch}");
    if (has({"definitionpicture"})) return {};
    if (has({"picture", "image", "screenshot", "photo"})) return QStringLiteral("{image}");
    if (has({"maindefinition"})) return QStringLiteral("{glossary-first}");
    if (has({"definition", "meaning", "glossary", "english"})) return QStringLiteral("{glossary}");
    if (has({"expression", "word", "vocab", "term", "front", "target"})) return QStringLiteral("{expression}");
    if (has({"url", "source", "link", "miscinfo"})) return QStringLiteral("{source}");
    if (has({"note"})) return QStringLiteral("{notes}");
    return {};
}

QVariantMap lapisPreset()
{
    return {{"Expression", "{expression}"}, {"ExpressionFurigana", "{furigana}"}, {"ExpressionReading", "{reading}"},
            {"ExpressionAudio", "{audio}"}, {"SelectionText", ""}, {"MainDefinition", "{glossary-first}"},
            {"DefinitionPicture", ""}, {"Sentence", "{sentence-bold}"}, {"SentenceFurigana", "{sentence-furigana}"},
            {"SentenceAudio", "{sentence-audio}"}, {"Picture", "{image}"}, {"Glossary", "{glossary}"}, {"Hint", ""},
            {"IsWordAndSentenceCard", ""}, {"IsClickCard", ""}, {"IsSentenceCard", ""}, {"IsAudioCard", ""},
            {"PitchPosition", "{pitch}"}, {"PitchCategories", "{pitch-categories}"}, {"Frequency", "{frequency}"},
            {"FreqSort", "{freq-sort}"}, {"MiscInfo", "{source}"}};
}

namespace {

QString esc(const QString &s) { return s.toHtmlEscaped(); }
QString br(const QString &s) { return esc(s).replace(u'\n', QStringLiteral("<br>")); }

QString boldWord(const QString &sentence, const QString &surface)
{
    const qsizetype i = surface.isEmpty() ? -1 : sentence.indexOf(surface);
    if (i < 0) return br(sentence);
    return (esc(sentence.left(i)) + u"<b>" + esc(surface) + u"</b>" + esc(sentence.mid(i + surface.size()))).replace(u'\n', QStringLiteral("<br>"));
}

QString mediaName(const QString &path, qint64 stamp, const QString &kind)
{
    return QStringLiteral("togu_%1_%2.%3").arg(stamp).arg(kind, QFileInfo(path).suffix().toLower());
}

} // namespace

Note buildNote(const Draft &d, const QString &deck, const QString &model, const QVariantMap &fields,
               const QStringList &tags, bool allowDuplicates, qint64 stamp)
{
    const auto segs = jp::furiganaSegments(d.expression, d.reading);
    QHash<QString, QString> values = {
        {"expression", esc(d.expression)},
        {"reading", esc(d.reading)},
        {"furigana", esc(jp::furiganaPlain(segs))},
        {"furigana-html", jp::furiganaHtml(segs)},
        {"glossary", d.glossaryHtml},
        {"glossary-first", d.glossaryFirstHtml.isEmpty() ? d.glossaryHtml : d.glossaryFirstHtml},
        {"glossary-plain", br(d.glossaryText)},
        {"sentence", br(d.sentence)},
        {"sentence-bold", boldWord(d.sentence, d.surface.isEmpty() ? d.expression : d.surface)},
        {"sentence-furigana", esc(d.sentenceFurigana)},
        {"sentence-translation", br(d.sentenceTranslation)},
        {"frequency", d.frequency},
        {"freq-sort", d.freqSort ? QString::number(d.freqSort) : QString()},
        {"pitch", esc(d.pitchPositions)},
        {"pitch-categories", esc(d.pitchCategories)},
        {"source", esc(d.source)},
        {"notes", br(d.notes)},
        {"example-sentences", br(d.examples)},
    };
    QHash<QString, QStringList> mediaFields; // marker -> fields that want it
    QJsonObject outFields;
    static const QRegularExpression marker(QStringLiteral("\\{([a-z-]+)\\}"));
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        QString tpl = it.value().toString();
        QString out;
        qsizetype last = 0;
        auto m = marker.globalMatch(tpl);
        while (m.hasNext()) {
            auto mm = m.next();
            out += tpl.mid(last, mm.capturedStart() - last);
            const QString key = mm.captured(1);
            if (key == u"audio" || key == u"sentence-audio" || key == u"image") mediaFields[key] << it.key();
            else out += values.contains(key) ? values.value(key) : mm.captured();
            last = mm.capturedEnd();
        }
        out += tpl.mid(last);
        outFields.insert(it.key(), out);
    }
    Note n;
    QJsonObject note{{"deckName", deck}, {"modelName", model}, {"fields", outFields},
                     {"tags", QJsonArray::fromStringList(tags)},
                     {"options", QJsonObject{{"allowDuplicate", allowDuplicates}, {"duplicateScope", "deck"}}}};
    QJsonArray audio, picture, video;
    auto add = [&](QJsonArray &arr, const QString &path, const QString &kind, const QStringList &flds) {
        if (path.isEmpty() || flds.isEmpty()) return;
        arr.append(QJsonObject{{"path", path}, {"filename", mediaName(path, stamp, kind)}, {"fields", QJsonArray::fromStringList(flds)}});
        n.mediaFiles << path;
    };
    add(audio, d.wordAudio, QStringLiteral("word"), mediaFields.value("audio"));
    add(audio, d.sentenceAudio, QStringLiteral("sentence"), mediaFields.value("sentence-audio"));
    for (qsizetype i = 0; i < d.images.size(); ++i) {
        const QString suffix = QFileInfo(d.images[i]).suffix().toLower();
        // A video clip goes in as a [sound:] tag, which Anki plays with its video player.
        add(suffix == u"webm" || suffix == u"mp4" ? video : picture, d.images[i], i ? QStringLiteral("image%1").arg(i + 1) : QStringLiteral("image"),
            mediaFields.value("image"));
    }
    if (!audio.isEmpty()) note.insert("audio", audio);
    if (!picture.isEmpty()) note.insert("picture", picture);
    if (!video.isEmpty()) note.insert("video", video);
    n.json = note;
    return n;
}

QString plainField(const QString &field)
{
    QString s = field;
    s.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
    s.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    s.replace(QStringLiteral("&amp;"), QStringLiteral("&")).replace(QStringLiteral("&lt;"), QStringLiteral("<")).replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    // 漢字[かんじ] -> 漢字
    s.remove(QRegularExpression(QStringLiteral("\\[[^\\]]*\\]")));
    s.remove(u' ');
    return s.trimmed();
}

} // namespace anki
