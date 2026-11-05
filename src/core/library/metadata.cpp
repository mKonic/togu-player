#include "metadata.h"

#include "core/util/db.h"
#include "names.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimer>
#include <QUrlQuery>

namespace metadata {

namespace {

QByteArray request(QNetworkAccessManager &net, const QNetworkRequest &req, const QByteArray &body = {}, bool post = false)
{
    QNetworkReply *r = post ? net.post(req, body) : net.get(req);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, r, &QNetworkReply::abort);
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(15000);
    loop.exec();
    const QByteArray data = r->error() == QNetworkReply::NoError ? r->readAll() : QByteArray();
    r->deleteLater();
    return data;
}

QNetworkRequest req(const QUrl &url)
{
    QNetworkRequest r(url);
    r.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("togu-player"));
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return r;
}

QString plain(QString html)
{
    html.replace(QRegularExpression(QStringLiteral("<br\\s*/?>")), QStringLiteral("\n"));
    html.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
    return html.trimmed();
}

int lev(const QString &a, const QString &b)
{
    QList<int> prev(b.size() + 1), cur(b.size() + 1);
    for (qsizetype j = 0; j <= b.size(); ++j) prev[j] = int(j);
    for (qsizetype i = 1; i <= a.size(); ++i) {
        cur[0] = int(i);
        for (qsizetype j = 1; j <= b.size(); ++j)
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

} // namespace

double similarity(const QString &a, const QString &b)
{
    const QString x = names::normalizedKey(a), y = names::normalizedKey(b);
    if (x.isEmpty() || y.isEmpty()) return 0;
    if (x == y) return 1;
    const qsizetype n = std::max(x.size(), y.size());
    return 1.0 - double(lev(x, y)) / n;
}

QList<Candidate> search(const QString &title, const QString &tmdbKey)
{
    QList<Candidate> out;
    QNetworkAccessManager net;
    // AniList: no key needed.
    {
        const QString query = QStringLiteral(
            "query($s:String){Page(perPage:8){media(search:$s,type:ANIME,sort:SEARCH_MATCH){id title{romaji english native} synonyms "
            "coverImage{extraLarge large} bannerImage description(asHtml:false) seasonYear startDate{year}}}}");
        QNetworkRequest r = req(QUrl(QStringLiteral("https://graphql.anilist.co")));
        r.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        const QByteArray body = QJsonDocument(QJsonObject{{"query", query}, {"variables", QJsonObject{{"s", title}}}}).toJson(QJsonDocument::Compact);
        const QJsonObject root = QJsonDocument::fromJson(request(net, r, body, true)).object();
        for (const QJsonValue &v : root.value("data").toObject().value("Page").toObject().value("media").toArray()) {
            const QJsonObject m = v.toObject();
            const QJsonObject t = m.value("title").toObject();
            Candidate c;
            c.source = QStringLiteral("anilist");
            c.id = QString::number(m.value("id").toInt());
            c.title = t.value("romaji").toString();
            c.titleEnglish = t.value("english").toString();
            c.titleNative = t.value("native").toString();
            for (const QJsonValue &s : m.value("synonyms").toArray()) c.synonyms << s.toString();
            const QJsonObject cover = m.value("coverImage").toObject();
            c.poster = cover.value("extraLarge").toString(cover.value("large").toString());
            c.banner = m.value("bannerImage").toString();
            c.description = plain(m.value("description").toString());
            c.year = m.value("seasonYear").toInt(m.value("startDate").toObject().value("year").toInt());
            out.append(c);
        }
    }
    if (!tmdbKey.isEmpty()) {
        QUrl url(QStringLiteral("https://api.themoviedb.org/3/search/multi"));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("query"), title);
        q.addQueryItem(QStringLiteral("api_key"), tmdbKey);
        url.setQuery(q);
        const QJsonObject root = QJsonDocument::fromJson(request(net, req(url))).object();
        for (const QJsonValue &v : root.value("results").toArray()) {
            const QJsonObject m = v.toObject();
            const QString type = m.value("media_type").toString();
            if (type != u"tv" && type != u"movie") continue;
            Candidate c;
            c.source = QStringLiteral("tmdb");
            c.id = type + u':' + QString::number(m.value("id").toInt());
            c.title = m.value(type == u"tv" ? "name" : "title").toString();
            c.titleNative = m.value(type == u"tv" ? "original_name" : "original_title").toString();
            if (!m.value("poster_path").toString().isEmpty()) c.poster = QStringLiteral("https://image.tmdb.org/t/p/w500") + m.value("poster_path").toString();
            if (!m.value("backdrop_path").toString().isEmpty()) c.banner = QStringLiteral("https://image.tmdb.org/t/p/w1280") + m.value("backdrop_path").toString();
            c.description = m.value("overview").toString();
            c.year = m.value(type == u"tv" ? "first_air_date" : "release_date").toString().left(4).toInt();
            out.append(c);
        }
    }
    return out;
}

bool apply(int seriesId, const Candidate &c, bool manual)
{
    QNetworkAccessManager net;
    const QString dir = db::cacheDir() + QStringLiteral("/meta");
    QDir().mkpath(dir);
    auto save = [&](const QString &url, const QString &name) -> QString {
        if (url.isEmpty()) return {};
        const QByteArray data = request(net, req(QUrl(url)));
        if (data.isEmpty()) return {};
        const QString path = dir + QStringLiteral("/%1-%2.%3").arg(seriesId).arg(name, url.section(u'.', -1).left(4));
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return {};
        f.write(data);
        return path;
    };
    const QString poster = save(c.poster, QStringLiteral("poster"));
    const QString banner = save(c.banner, QStringLiteral("banner"));
    return db::exec(db::app(), QStringLiteral("UPDATE series SET meta_id=?, meta_title=?, meta_poster=?, meta_banner=?, meta_desc=?, meta_year=?, meta_manual=? WHERE id=?"),
                    {c.source + u':' + c.id, c.title, poster, banner, c.description, c.year, manual ? 1 : 0, seriesId});
}

bool autoMatch(int seriesId, const QString &tmdbKey)
{
    const QString title = db::scalar(db::app(), QStringLiteral("SELECT title FROM series WHERE id=?"), {seriesId}).toString();
    if (title.isEmpty()) return false;
    const QList<Candidate> cands = search(title, tmdbKey);
    const Candidate *best = nullptr;
    double bestScore = 0;
    for (const Candidate &c : cands) {
        double s = std::max({similarity(title, c.title), similarity(title, c.titleEnglish), similarity(title, c.titleNative)});
        for (const QString &syn : c.synonyms) s = std::max(s, similarity(title, syn));
        if (s > bestScore) {
            bestScore = s;
            best = &c;
        }
    }
    if (!best || bestScore < 0.85) {
        db::exec(db::app(), QStringLiteral("UPDATE series SET meta_id='none' WHERE id=?"), {seriesId});
        return false;
    }
    return apply(seriesId, *best);
}

} // namespace metadata
