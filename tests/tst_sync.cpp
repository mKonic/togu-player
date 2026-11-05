// Auto-sync on synthetic audio: bursts where the lines really are, subtitles shifted.
#include "core/subs/sync.h"

#include <QRandomGenerator>
#include <QTest>

#include <cmath>

namespace {

struct Fixture {
    QList<SubLine> truth;
    QByteArray pcm;
};

Fixture make(int lines, quint32 seed)
{
    Fixture f;
    QRandomGenerator rng(seed);
    double t = 5;
    for (int i = 0; i < lines; ++i) {
        const double len = 0.8 + rng.bounded(3.5);
        f.truth.append({t, t + len, QStringLiteral("line %1").arg(i)});
        t += len + 0.3 + rng.bounded(4.0);
    }
    const int n = int((t + 10) * 16000);
    f.pcm.resize(n * 2);
    auto *s = reinterpret_cast<int16_t *>(f.pcm.data());
    for (int i = 0; i < n; ++i) s[i] = int16_t(rng.bounded(200) - 100); // room noise
    for (const SubLine &l : f.truth) {
        for (int i = int(l.start * 16000); i < int(l.end * 16000) && i < n; ++i) {
            const double env = 0.5 + 0.5 * std::sin(i / 16000.0 * 2 * M_PI * 4); // syllable-ish
            s[i] = int16_t(std::clamp((rng.bounded(16000) - 8000) * env, -32000.0, 32000.0));
        }
    }
    return f;
}

} // namespace

class TestSync : public QObject {
    Q_OBJECT
private slots:
    void offset()
    {
        Fixture f = make(300, 7);
        auto shifted = sync::apply(f.truth, -2.37);
        auto speech = sync::voiceActivity(f.pcm);
        auto r = sync::align(shifted, speech);
        QVERIFY2(std::abs(r.offset - 2.37) < 0.06, qPrintable(QString::number(r.offset)));
        QVERIFY(r.ok());
        QCOMPARE(r.split, -1);
        QVERIFY(r.after > r.before);
    }
    void inSync()
    {
        Fixture f = make(200, 11);
        auto r = sync::align(f.truth, sync::voiceActivity(f.pcm));
        QVERIFY(std::abs(r.offset) < 0.06);
    }
    void split()
    {
        Fixture f = make(300, 3);
        auto shifted = sync::apply(f.truth, 1.5, 150, -4.0); // an ad break cut differently
        auto r = sync::align(shifted, sync::voiceActivity(f.pcm));
        QVERIFY2(r.split >= 145 && r.split <= 155, qPrintable(QString::number(r.split)));
        QVERIFY(std::abs(r.offset + 1.5) < 0.06);
        QVERIFY(std::abs(r.offset2 - 4.0) < 0.06);
    }
};

QTEST_GUILESS_MAIN(TestSync)
#include "tst_sync.moc"
