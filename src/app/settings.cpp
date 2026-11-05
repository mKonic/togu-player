#include "settings.h"

#include "core/util/db.h"

#include <QJSValue>
#include <QJsonDocument>
#include <QQmlEngine>

namespace {

const QVariantMap &defaults()
{
    static const QVariantMap d = {
        // Look
        {"theme", "dark"},
        {"uiScale", 1.0},
        // Subtitles
        {"subFont", "Noto Sans CJK JP"},
        {"subSize", 44},
        {"subWeight", 500},
        {"subBottom", 0.07},         // share of the video height
        {"subBackground", 0.0},      // opacity of the box behind lines
        {"subOutline", true},
        {"sub2Size", 26},
        {"sub2Mode", "hover"},       // off, always, hover, blur
        {"sub2Position", "top"},     // top, below
        {"subBlur", false},          // primary blurred until hovered
        {"furigana", "unknown"},     // off, unknown, all, hover
        {"statusColors", true},
        {"pitchColors", "off"},      // off, hover, all
        {"wordGloss", "off"},        // off, unknown, hover
        {"stars", "hover"},          // frequency stars under recommended words: off, hover, all
        {"tidyLines", true},
        // Playback
        {"autoPause", "off"},        // off, end
        {"pauseOnHover", true},
        {"resumePlayback", true},
        {"seekStep", 5.0},
        {"offsetStep", 0.05},
        {"offsetStepBig", 0.5},
        {"autoSync", true},
        {"volume", 100.0},
        {"speedStep", 0.1},
        {"lookupMode", "hover"},     // hover, shift, click
        // Cards
        {"ankiUrl", "http://127.0.0.1:8765"},
        {"ankiKey", ""},
        {"ankiDeck", ""},
        {"ankiModel", ""},
        {"ankiFields", QVariantMap()},
        {"ankiTags", "togu"},
        {"ankiDuplicates", false},
        {"ankiWordField", ""},
        {"audioFormat", "mp3"},      // mp3, opus
        {"audioPadBefore", 0.15},
        {"audioPadAfter", 0.35},
        {"imageMode", "still"},      // still, animated, clip
        {"imageWithSubs", false},
        {"imageHeight", 480},
        {"clipMaxSeconds", 8.0},
        {"imageFormat", "webp"},     // webp, jpg, avif
        {"wordAudioDir", ""},        // local-audio collection (user_files of the Anki add-on)
        // Anki-driven statuses
        {"ankiStatusDecks", QVariantList()},
        {"ankiStatusModels", QVariantMap()}, // model -> {word, reading, templates[]}
        {"ankiStatusSync", false},
        {"ankiKnownDecks", QVariantList()},
        {"ankiMatureDays", 21},
        {"ankiLeechLapses", 8},
        // Online
        {"metadata", true},
        {"tmdbKey", ""},
        {"jimakuKey", ""},
        // Reader
        {"readerFont", "Noto Serif CJK JP"},
        {"readerSize", 22},
        {"readerVertical", false},
        {"readerTheme", "dark"},
        // Stats
        {"dailyGoalMinutes", 60},
    };
    return d;
}

} // namespace

Settings *Settings::instance()
{
    static Settings *s = new Settings;
    return s;
}

Settings *Settings::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

Settings::Settings(QObject *parent) : QQmlPropertyMap(this, parent)
{
    for (auto it = defaults().constBegin(); it != defaults().constEnd(); ++it) insert(it.key(), it.value());
    QSqlQuery q(db::app());
    q.exec(QStringLiteral("SELECT k, v FROM settings"));
    while (q.next()) {
        const QString k = q.value(0).toString();
        const QVariant v = QJsonDocument::fromJson(q.value(1).toByteArray()).toVariant().toMap().value("v");
        insert(k, v);
    }
}

QVariant Settings::updateValue(const QString &key, const QVariant &input)
{
    store(key, input);
    return input;
}

void Settings::set(const QString &key, const QVariant &v)
{
    QVariant val = v;
    if (val.canConvert<QJSValue>()) val = val.value<QJSValue>().toVariant();
    insert(key, val);
    store(key, val);
    emit valueChanged(key, val);
}

void Settings::reset(const QString &key) { set(key, defaults().value(key)); }

QVariant Settings::defaultValue(const QString &key) const { return defaults().value(key); }

void Settings::store(const QString &key, const QVariant &v)
{
    const QByteArray json = QJsonDocument::fromVariant(QVariantMap{{"v", v}}).toJson(QJsonDocument::Compact);
    db::exec(db::app(), QStringLiteral("INSERT INTO settings (k, v) VALUES (?, ?) ON CONFLICT(k) DO UPDATE SET v=excluded.v"),
             {key, json});
}
