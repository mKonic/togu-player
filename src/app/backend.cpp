#include "backend.h"

#include "core/dict/dict.h"
#include "core/dict/glossary.h"
#include "core/jp/jp.h"
#include "core/words/words.h"
#include "display.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJSValue>
#include <QQmlEngine>
#include <QtConcurrent>

Togu *Togu::instance()
{
    static Togu *t = new Togu;
    return t;
}

Togu *Togu::create(QQmlEngine *, QJSEngine *)
{
    QQmlEngine::setObjectOwnership(instance(), QQmlEngine::CppOwnership);
    return instance();
}

Togu::Togu(QObject *parent) : QObject(parent)
{
    connect(&Words::instance(), &Words::changed, this, [this](const QStringList &keys) {
        emit statusesChanged(keys);
        emit countsChanged();
    });
}

QString Togu::version() const { return QStringLiteral(TOGU_VERSION); }

QVariantMap Togu::counts() const
{
    const auto c = Words::instance().counts();
    return {{"known", c.value(Status::Known)}, {"learning", c.value(Status::Learning)},
            {"tracked", c.value(Status::Tracked)}, {"ignored", c.value(Status::Ignored)}};
}

QVariantList Togu::dicts() const
{
    QVariantList out;
    for (const DictInfo &d : Dict::instance().list())
        out << QVariantMap{{"id", d.id}, {"title", d.title}, {"revision", d.revision}, {"kind", d.kind},
                           {"enabled", d.enabled}, {"terms", d.terms}, {"metas", d.metas}};
    return out;
}

QVariantList Togu::lookup(const QString &textFromWord)
{
    QVariantList out;
    const Words &words = Words::instance();
    for (const DictEntry &e : Dict::instance().lookup(textFromWord)) {
        QVariantMap m;
        m.insert("expression", e.expression);
        m.insert("reading", e.reading);
        m.insert("matched", e.matched);
        m.insert("matchLen", e.matchLen);
        m.insert("rank", e.rank);
        m.insert("stars", e.stars);
        const QString key = jp::wordKey(e.expression, e.reading);
        m.insert("key", key);
        m.insert("status", statusName(words.status(key)));
        m.insert("card", words.hasCard(key));
        m.insert("furigana", furiganaHtml(e.expression, e.reading));
        QVariantList segs;
        for (const jp::FuriSeg &f : jp::furiganaSegments(e.expression, e.reading)) segs << QVariantMap{{"t", f.text}, {"r", f.reading}};
        m.insert("segs", segs);
        QVariantList freqs;
        for (const FreqInfo &f : e.freqs) freqs << QVariantMap{{"dict", f.dict}, {"value", f.value}, {"display", f.display}};
        m.insert("freqs", freqs);
        QVariantList pitches;
        QString pos;
        for (const PitchInfo &p : e.pitches) {
            QVariantList positions;
            for (int x : p.positions) positions << x;
            pitches << QVariantMap{{"dict", p.dict}, {"reading", p.reading}, {"positions", positions}};
        }
        m.insert("pitches", pitches);
        QVariantList blocks;
        QStringList cardParts, textParts;
        for (const GlossBlock &b : e.blocks) {
            const QString html = glossary::displayHtml(b.glossary, b.dictId);
            const QString card = glossary::cardHtml(b.glossary, b.dict, false);
            blocks << QVariantMap{{"dict", b.dict}, {"dictId", b.dictId}, {"html", html}, {"cardHtml", card},
                                  {"tags", b.defTags + b.termTags}};
        }
        m.insert("blocks", blocks);
        out << m;
    }
    return out;
}

QVariantList Togu::kanji(const QString &text)
{
    QVariantList out;
    QSet<QString> seen;
    for (char32_t c : text.toUcs4()) {
        if (!jp::isKanji(c)) continue;
        const QString ch = QString::fromUcs4(&c, 1);
        if (seen.contains(ch)) continue;
        seen.insert(ch);
        for (const KanjiEntry &k : Dict::instance().kanji(ch))
            out << QVariantMap{{"character", ch}, {"dict", k.dict}, {"onyomi", k.onyomi}, {"kunyomi", k.kunyomi},
                               {"meanings", k.meanings}, {"tags", k.tags}};
    }
    return out;
}

QString Togu::status(const QString &key) const { return statusName(Words::instance().status(key)); }

void Togu::setStatus(const QVariant &keys, const QString &status)
{
    QVariant v = keys;
    if (v.canConvert<QJSValue>()) v = v.value<QJSValue>().toVariant();
    QStringList list = v.typeId() == QMetaType::QString ? QStringList{v.toString()} : v.toStringList();
    QList<std::pair<QString, Status>> e;
    const Status s = statusFromName(status);
    for (const QString &k : list) e.append({k, s});
    Words::instance().setManual(e);
}

QVariantList Togu::tokenize(const QString &text) { return display::tokens(jp::tokenize(text)); }

QString Togu::furiganaHtml(const QString &surface, const QString &reading) const
{
    return jp::furiganaHtml(jp::furiganaSegments(surface, reading));
}

// Morae of a reading with high/low marks for drawing the pitch graph.
QVariantList Togu::pitchMorae(const QString &reading, int position) const
{
    static const QString small = QStringLiteral("ゃゅょぁぃぅぇぉゎャュョァィゥェォヮ");
    QStringList morae;
    for (QChar c : reading) {
        if (small.contains(c) && !morae.isEmpty()) morae.last() += c;
        else morae << QString(c);
    }
    QVariantList out;
    const int n = int(morae.size());
    for (int i = 0; i <= n; ++i) {
        bool high;
        if (position == 0) high = i > 0;
        else if (position == 1) high = i == 0;
        else high = i > 0 && i < position;
        // i == n is the particle after the word
        out << QVariantMap{{"m", i < n ? morae[i] : QString()}, {"high", high}, {"particle", i == n}};
    }
    return out;
}

void Togu::importDicts(const QList<QUrl> &files)
{
    if (m_dictBusy || files.isEmpty()) return;
    m_dictBusy = true;
    m_dictFraction = 0;
    m_dictProgress = tr("Starting…");
    emit dictProgressChanged();
    QStringList paths;
    for (const QUrl &u : files) paths << (u.isLocalFile() ? u.toLocalFile() : u.toString());
    (void)QtConcurrent::run([this, paths] {
        for (qsizetype i = 0; i < paths.size(); ++i) {
            QString res = Dict::instance().import(paths[i], [&](const Dict::Progress &p) {
                const double frac = (i + (p.total ? double(p.done) / p.total : 0)) / paths.size();
                const QString text = QStringLiteral("%1 — %2").arg(p.title, p.phase);
                QMetaObject::invokeMethod(this, [this, frac, text] {
                    m_dictFraction = frac;
                    m_dictProgress = text;
                    emit dictProgressChanged();
                });
            });
            const bool err = res.startsWith(u"error:");
            QMetaObject::invokeMethod(this, [this, res, err, path = paths[i]] {
                emit dictImported(err ? QFileInfo(path).fileName() : res, err ? res.mid(6) : QString());
                emit dictsChanged();
            });
        }
        QMetaObject::invokeMethod(this, [this] {
            m_dictBusy = false;
            m_dictProgress.clear();
            emit dictProgressChanged();
            emit dictsChanged();
            emit statusesChanged({});
        });
    });
}

void Togu::removeDict(int id)
{
    Dict::instance().remove(id);
    emit dictsChanged();
}

void Togu::setDictEnabled(int id, bool on)
{
    Dict::instance().setEnabled(id, on);
    emit dictsChanged();
}

void Togu::moveDict(int id, int delta)
{
    Dict::instance().move(id, delta);
    emit dictsChanged();
}

void Togu::importWordList(const QString &text, const QString &status)
{
    QStringList keys;
    for (QString line : text.split(u'\n')) {
        line = line.trimmed();
        if (line.isEmpty()) continue;
        // "word<TAB>reading", "word reading" or a bare word.
        QStringList parts = line.split(QRegularExpression(QStringLiteral("[\\t,;]+")), Qt::SkipEmptyParts);
        keys << (parts.size() >= 2 ? jp::wordKey(parts[0].trimmed(), parts[1].trimmed()) : parts.value(0));
    }
    Words::instance().importList(keys, statusFromName(status));
    emit toast(tr("%n word(s) imported", nullptr, int(keys.size())), QStringLiteral("teal"));
}

QString Togu::exportWordList(const QString &status) const
{
    QStringList out;
    const Status want = statusFromName(status);
    for (const auto &[k, s] : Words::instance().all()) {
        if (s != want) continue;
        auto [lemma, reading] = jp::splitKey(k);
        out << (reading.isEmpty() ? lemma : lemma + u'\t' + reading);
    }
    out.sort();
    return out.join(u'\n');
}

void Togu::clearWords() { Words::instance().clear(); }

QString Togu::readTextFile(const QUrl &file) const
{
    QFile f(file.isLocalFile() ? file.toLocalFile() : file.toString());
    return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

bool Togu::writeTextFile(const QUrl &file, const QString &text) const
{
    QFile f(file.isLocalFile() ? file.toLocalFile() : file.toString());
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(text.toUtf8()) >= 0;
}

bool Togu::exists(const QString &path) const { return QFileInfo::exists(path); }

void Togu::openExternally(const QString &urlOrPath) const
{
    QDesktopServices::openUrl(urlOrPath.contains(u"://") ? QUrl(urlOrPath) : QUrl::fromLocalFile(urlOrPath));
}

void Togu::copyText(const QString &text) const { QGuiApplication::clipboard()->setText(text); }

QString Togu::formatTime(double seconds) const
{
    const qint64 s = qint64(std::max(0.0, seconds));
    if (s >= 3600) return QStringLiteral("%1:%2:%3").arg(s / 3600).arg(s / 60 % 60, 2, 10, QChar(u'0')).arg(s % 60, 2, 10, QChar(u'0'));
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar(u'0'));
}
