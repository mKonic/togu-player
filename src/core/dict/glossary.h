#pragma once
// Yomitan definitions (plain strings or structured content) to HTML.

#include <QJsonArray>
#include <QString>

namespace glossary {

// Sanitized HTML as togu builds it, for Anki cards. Images left out when `media` is off.
QString cardHtml(const QJsonArray &defs, const QString &dict, bool media = false);
// The subset Qt's rich text understands (no CSS classes, ruby or details), for the popup.
// Images point at image://dict/<dictId>/<path>.
QString displayHtml(const QJsonArray &defs, int dictId);
// The first meaning, a few words long, for word translations under subtitles.
QString shortGloss(const QJsonArray &defs);
QString htmlToText(const QString &html);

} // namespace glossary
