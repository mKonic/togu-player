#include "glossary.h"

#include <QHash>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace glossary {

namespace {

QString esc(const QString &s) { return s.toHtmlEscaped(); }
QString escBr(const QString &s) { return esc(s).replace(u'\n', QStringLiteral("<br>")); }

const QSet<QString> &cardTags()
{
    static const QSet<QString> t = {"br", "ruby", "rt", "rp", "table", "thead", "tbody", "tfoot", "tr", "td", "th",
                                    "span", "div", "ol", "ul", "li", "details", "summary", "a", "img"};
    return t;
}

const QHash<QString, QString> &styleProps()
{
    static const QHash<QString, QString> p = {
        {"fontStyle", "font-style"}, {"fontWeight", "font-weight"}, {"fontSize", "font-size"}, {"color", "color"},
        {"background", "background"}, {"backgroundColor", "background-color"},
        {"textDecorationLine", "text-decoration-line"}, {"textDecorationStyle", "text-decoration-style"},
        {"textDecorationColor", "text-decoration-color"}, {"borderColor", "border-color"}, {"borderStyle", "border-style"},
        {"borderRadius", "border-radius"}, {"borderWidth", "border-width"}, {"clipPath", "clip-path"},
        {"verticalAlign", "vertical-align"}, {"textAlign", "text-align"}, {"textEmphasis", "text-emphasis"},
        {"textShadow", "text-shadow"}, {"margin", "margin"}, {"marginTop", "margin-top"}, {"marginLeft", "margin-left"},
        {"marginRight", "margin-right"}, {"marginBottom", "margin-bottom"}, {"padding", "padding"},
        {"paddingTop", "padding-top"}, {"paddingLeft", "padding-left"}, {"paddingRight", "padding-right"},
        {"paddingBottom", "padding-bottom"}, {"wordBreak", "word-break"}, {"whiteSpace", "white-space"},
        {"cursor", "cursor"}, {"listStyleType", "list-style-type"}};
    return p;
}

QString kebab(const QString &s)
{
    QString out;
    for (QChar c : s) {
        if (c.isUpper()) out += u'-' + c.toLower();
        else out += c;
    }
    return out;
}

QString styleAttr(const QJsonObject &style)
{
    static const QRegularExpression bad(QStringLiteral("[;{}<>]|url\\(|expression\\("), QRegularExpression::CaseInsensitiveOption);
    QStringList parts;
    for (auto it = style.begin(); it != style.end(); ++it) {
        auto prop = styleProps().value(it.key());
        if (prop.isEmpty() || !(it->isString() || it->isDouble())) continue;
        QString value = it->isDouble() ? QString::number(it->toDouble()) : it->toString();
        if (bad.match(value).hasMatch()) continue;
        if (it->isDouble() && (prop.contains(u"margin") || prop.contains(u"padding"))) value += QStringLiteral("em");
        parts << prop + u':' + value;
    }
    return parts.isEmpty() ? QString() : QStringLiteral(" style=\"%1\"").arg(esc(parts.join(u';')));
}

QString cardNode(const QJsonValue &node, const QString &dict, bool media)
{
    if (node.isNull() || node.isUndefined()) return {};
    if (node.isString()) return escBr(node.toString());
    if (node.isDouble()) return QString::number(node.toDouble());
    if (node.isArray()) {
        QString out;
        for (const QJsonValue &v : node.toArray()) out += cardNode(v, dict, media);
        return out;
    }
    const QJsonObject n = node.toObject();
    const QString tag = n.value("tag").toString();
    if (tag.isEmpty() || !cardTags().contains(tag)) return cardNode(n.value("content"), dict, media);
    if (tag == u"br") return QStringLiteral("<br>");
    QString attrs = QStringLiteral(" class=\"gloss-sc-%1\"").arg(tag);
    static const QRegularExpression keyRe(QStringLiteral("^[A-Za-z][\\w-]*$"));
    const QJsonObject data = n.value("data").toObject();
    for (auto it = data.begin(); it != data.end(); ++it)
        if (keyRe.match(it.key()).hasMatch())
            attrs += QStringLiteral(" data-sc-%1=\"%2\"").arg(kebab(it.key()), esc(it->toVariant().toString()));
    if (n.contains("style")) attrs += styleAttr(n.value("style").toObject());
    if (n.contains("lang")) attrs += QStringLiteral(" lang=\"%1\"").arg(esc(n.value("lang").toString()));
    if (n.contains("title")) attrs += QStringLiteral(" title=\"%1\"").arg(esc(n.value("title").toString()));
    if (n.value("colSpan").toInt()) attrs += QStringLiteral(" colspan=\"%1\"").arg(n.value("colSpan").toInt());
    if (n.value("rowSpan").toInt()) attrs += QStringLiteral(" rowspan=\"%1\"").arg(n.value("rowSpan").toInt());
    if (tag == u"details" && n.value("open").toBool()) attrs += QStringLiteral(" open");
    if (tag == u"img") {
        const QString path = n.value("path").toString();
        if (!media || path.isEmpty()) return esc(n.value("alt").toString());
        const QString unit = n.value("sizeUnits").toString() == u"em" ? QStringLiteral("em") : QStringLiteral("px");
        QString size;
        if (n.value("width").toDouble()) size += QStringLiteral("width:%1%2;").arg(n.value("width").toDouble()).arg(unit);
        if (n.value("height").toDouble()) size += QStringLiteral("height:%1%2;").arg(n.value("height").toDouble()).arg(unit);
        return QStringLiteral("<img%1 data-dict=\"%2\" data-path=\"%3\" alt=\"%4\" style=\"%5\">")
            .arg(attrs, esc(dict), esc(path), esc(n.value("alt").toString()), size);
    }
    if (tag == u"a") {
        const QString href = n.value("href").toString();
        if (href.startsWith(u"http://") || href.startsWith(u"https://"))
            attrs += QStringLiteral(" href=\"%1\"").arg(esc(href));
    }
    return QStringLiteral("<%1%2>%3</%1>").arg(tag, attrs, cardNode(n.value("content"), dict, media));
}

QString cardDef(const QJsonValue &def, const QString &dict, bool media)
{
    if (def.isString()) return escBr(def.toString());
    if (def.isObject()) {
        const QJsonObject d = def.toObject();
        const QString type = d.value("type").toString();
        if (type == u"text") return escBr(d.value("text").toString());
        if (type == u"structured-content") return cardNode(d.value("content"), dict, media);
        if (type == u"image") {
            QJsonObject img = d;
            img.insert("tag", "img");
            return cardNode(img, dict, media);
        }
    }
    return {};
}

// Display rendering: Qt rich text. Colours stand in for the dictionaries' CSS classes.
struct Display {
    int dictId;
    QString node(const QJsonValue &v, int depth = 0) const;
};

QString tagStyle(const QJsonObject &data)
{
    // Jitendex/JMdict-style tag chips and notes, recognised by their data attributes.
    const QString content = data.value("content").toString();
    if (content.contains(u"part-of-speech") || content == u"tag" || data.contains("code"))
        return QStringLiteral("background-color:#3c3c78;color:#cfd3ff;font-size:small;font-weight:600;");
    if (content.contains(u"example")) return QStringLiteral("color:#9aa0c8;font-size:small;");
    if (content.contains(u"xref") || content.contains(u"antonym")) return QStringLiteral("color:#8fb6ff;");
    if (content.contains(u"info") || content.contains(u"note") || content.contains(u"forms"))
        return QStringLiteral("color:#a9a9c9;font-size:small;");
    if (content.contains(u"attribution")) return QStringLiteral("color:#77779a;font-size:small;");
    return {};
}

QString Display::node(const QJsonValue &v, int depth) const
{
    if (v.isNull() || v.isUndefined()) return {};
    if (v.isString()) return escBr(v.toString());
    if (v.isDouble()) return QString::number(v.toDouble());
    if (v.isArray()) {
        QString out;
        for (const QJsonValue &c : v.toArray()) out += node(c, depth);
        return out;
    }
    const QJsonObject n = v.toObject();
    const QString tag = n.value("tag").toString();
    const QString inner = node(n.value("content"), depth + 1);
    if (tag == u"br") return QStringLiteral("<br>");
    if (tag == u"rt" || tag == u"rp") return {};
    if (tag == u"ruby") return inner;
    if (tag == u"img") {
        const QString path = n.value("path").toString();
        if (path.isEmpty()) return esc(n.value("alt").toString());
        // Images used as inline glyphs stay text-sized.
        const double h = n.value("height").toDouble();
        const bool em = n.value("sizeUnits").toString() == u"em";
        const int px = h ? int(em ? h * 16 : h) : 0;
        return QStringLiteral("<img src=\"image://dict/%1/%2\"%3>")
            .arg(dictId)
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(path, "/")), px ? QStringLiteral(" height=\"%1\"").arg(px) : QString());
    }
    QString style = tagStyle(n.value("data").toObject());
    const QJsonObject st = n.value("style").toObject();
    if (st.value("fontWeight").toString() == u"bold") style += QStringLiteral("font-weight:600;");
    if (st.value("fontStyle").toString() == u"italic") style += QStringLiteral("font-style:italic;");
    if (st.value("fontSize").toString().contains(u"small") || st.value("fontSize").toString().contains(u"0.8"))
        style += QStringLiteral("font-size:small;");
    if (st.contains("color")) style += QStringLiteral("color:%1;").arg(esc(st.value("color").toString()));
    const QString sAttr = style.isEmpty() ? QString() : QStringLiteral(" style=\"%1\"").arg(style);
    const QString dc = n.value("data").toObject().value("content").toString();
    // Meaning lists read as "to begin; to start" rather than a nested bullet list.
    if ((tag == u"ul" || tag == u"ol") && dc == u"glossary") {
        QStringList items;
        const QJsonValue c = n.value("content");
        for (const QJsonValue &li : c.isArray() ? c.toArray() : QJsonArray{c}) items << node(li.toObject().value("content"), depth + 1);
        return QStringLiteral("<span>%1</span>").arg(items.join(QStringLiteral("; ")));
    }
    // A numbered sense is its own paragraph with its marker (①, ②...).
    if (tag == u"li" && dc == u"sense") {
        QString marker = st.value("listStyleType").toString();
        marker.remove(u'"');
        return QStringLiteral("<p style=\"margin-top:4px;margin-bottom:4px;\">%1%2</p>").arg(marker.isEmpty() ? QString() : u"<b>" + marker + u"</b> ", inner);
    }
    if (tag == u"ul" || tag == u"ol") {
        // Lists of senses or other blocks are unwrapped; real lists keep their bullets.
        const QJsonValue c = n.value("content");
        bool blocks = true;
        for (const QJsonValue &li : c.isArray() ? c.toArray() : QJsonArray{c}) {
            const QString cd = li.toObject().value("data").toObject().value("content").toString();
            if (cd != u"sense" && cd != u"sense-group") blocks = false;
        }
        if (blocks) return inner;
        return QStringLiteral("<%1 style=\"margin-left:-20px;\">%2</%1>").arg(tag, inner);
    }
    if (tag == u"li") return QStringLiteral("<li%1>%2</li>").arg(sAttr, inner);
    if (tag == u"table") return QStringLiteral("<table cellpadding=\"2\">%1</table>").arg(inner);
    if (tag == u"tr" || tag == u"td" || tag == u"th" || tag == u"thead" || tag == u"tbody")
        return QStringLiteral("<%1>%2</%1>").arg(tag, inner);
    if (tag == u"div" || tag == u"details" || tag == u"summary") return QStringLiteral("<div%1>%2</div>").arg(sAttr, inner);
    if (tag == u"a") {
        const QString href = n.value("href").toString();
        QString target;
        if (href.startsWith(u"http")) target = href;
        else if (auto m = QRegularExpression(QStringLiteral("[?&]query=([^&]*)")).match(href); m.hasMatch())
            target = QStringLiteral("togu:lookup?") + m.captured(1);
        return QStringLiteral("<a href=\"%1\" style=\"color:#8fb6ff;text-decoration:none;\">%2</a>").arg(esc(target), inner);
    }
    if (!sAttr.isEmpty()) {
        // Chips get a little room around them.
        if (style.startsWith(u"background")) return QStringLiteral("<span%1>&nbsp;%2&nbsp;</span> ").arg(sAttr, inner);
        return QStringLiteral("<span%1>%2</span>").arg(sAttr, inner);
    }
    return inner;
}

QString plainText(const QJsonValue &v)
{
    if (v.isString()) return v.toString();
    if (v.isArray()) {
        QString out;
        for (const QJsonValue &c : v.toArray()) out += plainText(c);
        return out;
    }
    if (v.isObject()) {
        const QJsonObject n = v.toObject();
        const QString tag = n.value("tag").toString();
        if (tag == u"rt" || tag == u"rp" || tag == u"img" || tag == u"table") return {};
        if (n.value("type").toString() == u"text") return n.value("text").toString();
        return plainText(n.value("content"));
    }
    return {};
}

// The first definition in the first list item, where structured entries keep their meanings.
QString firstMeaning(const QJsonValue &v)
{
    if (v.isString()) return v.toString();
    if (v.isArray()) {
        for (const QJsonValue &c : v.toArray())
            if (QString t = firstMeaning(c); !t.isEmpty()) return t;
        return {};
    }
    if (v.isObject()) {
        const QJsonObject n = v.toObject();
        const QString type = n.value("type").toString();
        const QString tag = n.value("tag").toString();
        if (type == u"text") return n.value("text").toString();
        if (type == u"structured-content") return firstMeaning(n.value("content"));
        if (tag == u"li") return plainText(n.value("content"));
        if (n.contains("data")) {
            QStringList vals;
            const QJsonObject d = n.value("data").toObject();
            for (auto it = d.begin(); it != d.end(); ++it) vals << it->toString();
            static const QRegularExpression skip(QStringLiteral("example|note|xref|info|attribution|forms"), QRegularExpression::CaseInsensitiveOption);
            if (skip.match(vals.join(u' ')).hasMatch()) return {};
        }
        if (tag == u"rt" || tag == u"rp" || tag == u"img" || tag == u"details") return {};
        return firstMeaning(n.value("content"));
    }
    return {};
}

// Jitendex-style entries mark their meaning lists; the first one is the cleanest source.
QString firstGlossaryList(const QJsonValue &v)
{
    if (v.isArray()) {
        for (const QJsonValue &c : v.toArray())
            if (QString t = firstGlossaryList(c); !t.isEmpty()) return t;
        return {};
    }
    if (!v.isObject()) return {};
    const QJsonObject n = v.toObject();
    if (n.value("data").toObject().value("content").toString() == u"glossary") {
        QStringList items;
        const QJsonValue c = n.value("content");
        for (const QJsonValue &li : c.isArray() ? c.toArray() : QJsonArray{c}) items << plainText(li.toObject().value("content"));
        return items.join(QStringLiteral("; "));
    }
    if (n.value("type").toString() == u"structured-content") return firstGlossaryList(n.value("content"));
    return firstGlossaryList(n.value("content"));
}

} // namespace

QString cardHtml(const QJsonArray &defs, const QString &dict, bool media)
{
    QStringList parts;
    bool simple = true;
    for (const QJsonValue &d : defs) {
        if (!d.isString()) simple = false;
        if (QString h = cardDef(d, dict, media); !h.isEmpty()) parts << h;
    }
    if (parts.isEmpty()) return {};
    if (simple) return QStringLiteral("<ul><li>%1</li></ul>").arg(parts.join(QStringLiteral("</li><li>")));
    return parts.join(QString());
}

QString displayHtml(const QJsonArray &defs, int dictId)
{
    Display d{dictId};
    QStringList parts;
    bool simple = true;
    for (const QJsonValue &def : defs) {
        QString h;
        if (def.isString()) h = escBr(def.toString());
        else {
            simple = false;
            const QJsonObject o = def.toObject();
            const QString type = o.value("type").toString();
            if (type == u"text") h = escBr(o.value("text").toString());
            else if (type == u"structured-content") h = d.node(o.value("content"));
            else if (type == u"image") {
                QJsonObject img = o;
                img.insert("tag", "img");
                h = d.node(img);
            }
        }
        if (!h.isEmpty()) parts << h;
    }
    if (parts.isEmpty()) return {};
    if (simple && parts.size() > 1) return QStringLiteral("<ol><li>%1</li></ol>").arg(parts.join(QStringLiteral("</li><li>")));
    return parts.join(QStringLiteral("<br>"));
}

QString shortGloss(const QJsonArray &defs)
{
    QString text = firstGlossaryList(defs);
    if (text.isEmpty()) text = firstMeaning(defs);
    text = text.split(QRegularExpression(QStringLiteral("[;\\n]"))).value(0);
    text = text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    text = text.replace(QRegularExpression(QStringLiteral("\\s*\\([^)]*\\)\\s*$")), QString()).trimmed();
    return text.size() > 26 ? text.left(25) + u'…' : text;
}

QString htmlToText(const QString &html)
{
    QString s = html;
    s.replace(QRegularExpression(QStringLiteral("<(br|/li|/div|/tr|/p)[^>]*>")), QStringLiteral("\n"));
    s.replace(QRegularExpression(QStringLiteral("<rt[^>]*>.*?</rt>")), QString());
    s.replace(QRegularExpression(QStringLiteral("<[^>]+>")), QString());
    s.replace(QStringLiteral("&lt;"), QStringLiteral("<")).replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    s.replace(QStringLiteral("&quot;"), QStringLiteral("\"")).replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    s.replace(QRegularExpression(QStringLiteral("\\n{2,}")), QStringLiteral("\n"));
    return s.trimmed();
}

} // namespace glossary
