#include "jimaku.h"

#include "core/library/names.h"
#include "core/util/arc.h"
#include "core/util/db.h"
#include "settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QQmlEngine>
#include <QUrlQuery>
#include <QtConcurrent>

Jimaku *Jimaku::create(QQmlEngine *, QJSEngine *)
{
    static Jimaku *j = new Jimaku;
    QQmlEngine::setObjectOwnership(j, QQmlEngine::CppOwnership);
    return j;
}

Jimaku::Jimaku(QObject *parent) : QObject(parent) {}

QNetworkReply *Jimaku::get(const QUrl &url)
{
    QNetworkRequest req(url);
    req.setRawHeader("Authorization", Settings::v("jimakuKey").toString().toUtf8());
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("togu-player"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    ++m_busy;
    emit busyChanged();
    QNetworkReply *r = m_net.get(req);
    connect(r, &QNetworkReply::finished, this, [this] {
        --m_busy;
        emit busyChanged();
    });
    return r;
}

namespace {

QString errorText(QNetworkReply *r)
{
    const int code = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (code == 401 || code == 403) return QObject::tr("Jimaku didn't accept the API key. Check it in Settings → Online.");
    if (code == 429) return QObject::tr("Jimaku says too many requests; try again in a minute.");
    return r->errorString();
}

} // namespace

QVariantMap Jimaku::guess(int itemId) const
{
    QSqlQuery q(db::app());
    q.prepare(QStringLiteral("SELECT COALESCE(s.meta_title, s.title), s.meta_id, i.episode, i.path FROM items i LEFT JOIN series s ON s.id=i.series WHERE i.id=?"));
    q.addBindValue(itemId);
    QVariantMap m;
    if (db::exec(q) && q.next()) {
        const names::Parsed p = names::parse(q.value(3).toString());
        m.insert("query", q.value(0).toString().isEmpty() ? p.title : q.value(0).toString());
        const QString meta = q.value(1).toString();
        m.insert("anilistId", meta.startsWith(u"anilist:") ? meta.mid(8).toInt() : 0);
        m.insert("episode", q.value(2).isNull() ? p.episode : q.value(2).toDouble());
    }
    return m;
}

void Jimaku::search(const QString &query, int anilistId)
{
    if (Settings::v("jimakuKey").toString().isEmpty()) {
        emit failed(tr("Add your Jimaku API key in Settings → Online first. It's free with a jimaku.cc account."));
        return;
    }
    QUrl url(QStringLiteral("https://jimaku.cc/api/entries/search"));
    QUrlQuery qq;
    if (anilistId > 0) qq.addQueryItem(QStringLiteral("anilist_id"), QString::number(anilistId));
    else qq.addQueryItem(QStringLiteral("query"), query);
    url.setQuery(qq);
    QNetworkReply *r = get(url);
    connect(r, &QNetworkReply::finished, this, [this, r, query, anilistId] {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            emit failed(errorText(r));
            return;
        }
        QVariantList out;
        for (const QJsonValue &v : QJsonDocument::fromJson(r->readAll()).array()) {
            const QJsonObject o = v.toObject();
            out << QVariantMap{{"id", o.value("id").toInt()}, {"name", o.value("name").toString()},
                               {"japanese", o.value("japanese_name").toString()}, {"english", o.value("english_name").toString()},
                               {"anilistId", o.value("anilist_id").toInt()}};
        }
        // Nothing under the AniList id: fall back to the title.
        if (out.isEmpty() && anilistId > 0 && !query.isEmpty()) {
            search(query, 0);
            return;
        }
        emit searchResults(out);
    });
}

void Jimaku::files(int entryId, double episode)
{
    QUrl url(QStringLiteral("https://jimaku.cc/api/entries/%1/files").arg(entryId));
    if (episode >= 0) {
        QUrlQuery qq;
        qq.addQueryItem(QStringLiteral("episode"), QString::number(int(episode)));
        url.setQuery(qq);
    }
    QNetworkReply *r = get(url);
    connect(r, &QNetworkReply::finished, this, [this, r] {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            emit failed(errorText(r));
            return;
        }
        QVariantList out;
        for (const QJsonValue &v : QJsonDocument::fromJson(r->readAll()).array()) {
            const QJsonObject o = v.toObject();
            const QString name = o.value("name").toString();
            out << QVariantMap{{"name", name}, {"url", o.value("url").toString()}, {"size", o.value("size").toInteger()},
                               {"modified", o.value("last_modified").toString()},
                               {"archive", QRegularExpression(QStringLiteral("\\.(zip|rar|7z|tar|gz)$"), QRegularExpression::CaseInsensitiveOption).match(name).hasMatch()}};
        }
        emit fileResults(out);
    });
}

void Jimaku::download(const QString &url, const QString &name, int entryId, double episode)
{
    QNetworkReply *r = get(QUrl(url));
    connect(r, &QNetworkReply::finished, this, [this, r, name, entryId, episode] {
        r->deleteLater();
        if (r->error() != QNetworkReply::NoError) {
            emit failed(errorText(r));
            return;
        }
        const QByteArray data = r->readAll();
        (void)QtConcurrent::run([this, data, name, entryId, episode] {
            const QString dir = db::dataDir() + QStringLiteral("/subtitles/jimaku-%1").arg(entryId);
            QDir().mkpath(dir);
            QString result;
            if (names::isSubtitle(name)) {
                result = dir + u'/' + QFileInfo(name).fileName();
                QFile f(result);
                if (f.open(QIODevice::WriteOnly)) f.write(data);
            } else {
                // Archive: keep every subtitle in it, hand back the one for this episode.
                struct Cand {
                    QString path;
                    int score;
                };
                QList<Cand> cands;
                arc::eachInMemory(data, [](const QString &n) { return names::isSubtitle(n); }, [&](const QString &n, const QByteArray &bytes) {
                    const QString out = dir + u'/' + QFileInfo(n).fileName();
                    QFile f(out);
                    if (f.open(QIODevice::WriteOnly)) f.write(bytes);
                    const names::Parsed p = names::parse(QFileInfo(n).fileName());
                    int score = 0;
                    if (episode >= 0 && p.episode == episode) score += 10;
                    if (names::subtitleLang(n) == names::Lang::Japanese) score += 3;
                    if (n.endsWith(u".ass", Qt::CaseInsensitive) || n.endsWith(u".srt", Qt::CaseInsensitive)) score += 1;
                    cands.append({out, score});
                    return true;
                });
                std::sort(cands.begin(), cands.end(), [](auto &a, auto &b) { return a.score > b.score; });
                if (!cands.isEmpty() && (episode < 0 || cands.first().score >= 10 || cands.size() == 1)) result = cands.first().path;
            }
            QMetaObject::invokeMethod(this, [this, result] {
                if (result.isEmpty()) emit failed(tr("No subtitle for this episode in that archive."));
                else emit downloaded(result);
            });
        });
    });
}
