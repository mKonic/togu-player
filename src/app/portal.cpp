#include "portal.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRegularExpression>
#include <QUrl>

namespace {

const QString kService = QStringLiteral("org.freedesktop.portal.Desktop");
const QString kRequest = QStringLiteral("org.freedesktop.portal.Request");

// FileChooser filters are a(sa(us)): a name and (kind, pattern) rules, kind 0 = glob.
struct Rule {
    uint kind = 0;
    QString pattern;
};
struct Filter {
    QString name;
    QList<Rule> rules;
};

QDBusArgument &operator<<(QDBusArgument &a, const Rule &r)
{
    a.beginStructure();
    a << r.kind << r.pattern;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, Rule &r)
{
    a.beginStructure();
    a >> r.kind >> r.pattern;
    a.endStructure();
    return a;
}
QDBusArgument &operator<<(QDBusArgument &a, const Filter &f)
{
    a.beginStructure();
    a << f.name << f.rules;
    a.endStructure();
    return a;
}
const QDBusArgument &operator>>(const QDBusArgument &a, Filter &f)
{
    a.beginStructure();
    a >> f.name >> f.rules;
    a.endStructure();
    return a;
}

// "Video (*.mkv *.mp4)" → {"Video", [*.mkv, *.mp4]}
QList<Filter> parseFilters(const QStringList &specs)
{
    static const QRegularExpression re(QStringLiteral(R"(^(.*?)\s*\(([^)]*)\)\s*$)"));
    QList<Filter> out;
    for (const QString &spec : specs) {
        const auto m = re.match(spec);
        Filter f;
        f.name = m.hasMatch() ? m.captured(1) : spec;
        const QString globs = m.hasMatch() ? m.captured(2) : spec;
        for (const QString &g : globs.split(QLatin1Char(' '), Qt::SkipEmptyParts))
            f.rules.append({0, g});
        if (!f.rules.isEmpty())
            out.append(f);
    }
    return out;
}

}  // namespace

Q_DECLARE_METATYPE(Rule)
Q_DECLARE_METATYPE(Filter)

Portal::Portal(QObject *parent) : QObject(parent)
{
    qDBusRegisterMetaType<Rule>();
    qDBusRegisterMetaType<QList<Rule>>();
    qDBusRegisterMetaType<Filter>();
    qDBusRegisterMetaType<QList<Filter>>();
}

int Portal::open(const QVariantMap &opts)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return -1;

    const int id = m_next++;
    const bool save = opts.value(QStringLiteral("save")).toBool();
    const QString token = QStringLiteral("togu%1").arg(id);

    QVariantMap o;
    o.insert(QStringLiteral("handle_token"), token);
    o.insert(QStringLiteral("modal"), true);
    if (opts.value(QStringLiteral("folder")).toBool())
        o.insert(QStringLiteral("directory"), true);
    if (opts.value(QStringLiteral("multiple")).toBool())
        o.insert(QStringLiteral("multiple"), true);
    const auto filters = parseFilters(opts.value(QStringLiteral("filters")).toStringList());
    if (!filters.isEmpty())
        o.insert(QStringLiteral("filters"), QVariant::fromValue(filters));
    const QString folder = QUrl(opts.value(QStringLiteral("currentFolder")).toString()).toLocalFile();
    if (!folder.isEmpty())
        o.insert(QStringLiteral("current_folder"), QByteArray(folder.toUtf8()).append('\0'));
    const QString name = opts.value(QStringLiteral("currentName")).toString();
    if (save && !name.isEmpty())
        o.insert(QStringLiteral("current_name"), name);

    // Subscribe before calling, so a fast answer can't be missed.
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString expected = QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    listen(expected, id);

    QDBusMessage call = QDBusMessage::createMethodCall(kService, QStringLiteral("/org/freedesktop/portal/desktop"),
                                                       QStringLiteral("org.freedesktop.portal.FileChooser"),
                                                       save ? QStringLiteral("SaveFile") : QStringLiteral("OpenFile"));
    call << QString() << opts.value(QStringLiteral("title")).toString() << o;
    auto *w = new QDBusPendingCallWatcher(bus.asyncCall(call), this);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w, id, expected] {
        w->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *w;
        if (reply.isError()) {
            qWarning("file chooser portal: %s", qPrintable(reply.error().message()));
            forget(expected);
            emit failed(id);
            return;
        }
        // Old portals pick their own request path.
        const QString path = reply.value().path();
        if (path != expected && m_pending.contains(expected)) {
            forget(expected);
            listen(path, id);
        }
    });
    return id;
}

void Portal::listen(const QString &path, int id)
{
    m_pending.insert(path, id);
    QDBusConnection::sessionBus().connect(kService, path, kRequest, QStringLiteral("Response"), this,
                                          SLOT(onResponse(uint, QVariantMap)));
}

void Portal::forget(const QString &path)
{
    m_pending.remove(path);
    QDBusConnection::sessionBus().disconnect(kService, path, kRequest, QStringLiteral("Response"), this,
                                             SLOT(onResponse(uint, QVariantMap)));
}

void Portal::onResponse(uint response, const QVariantMap &results)
{
    const QString path = message().path();
    const int id = m_pending.value(path, -1);
    if (id < 0)
        return;
    forget(path);
    // 0 = chosen, 1 = cancelled, 2 = closed some other way.
    emit finished(id, response == 0 ? results.value(QStringLiteral("uris")).toStringList() : QStringList());
}
