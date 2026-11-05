// File name parsing: series title, season and episode from release names.
#include "core/library/names.h"

#include <QTest>

class TestNames : public QObject {
    Q_OBJECT
private slots:
    void parse_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QString>("title");
        QTest::addColumn<int>("season");
        QTest::addColumn<double>("episode");
        QTest::addColumn<bool>("special");
        QTest::newRow("subsplease") << "[SubsPlease] Sousou no Frieren - 05 (1080p) [A1B2C3D4].mkv" << "Sousou no Frieren" << 0 << 5.0 << false;
        QTest::newRow("sxxeyy") << "Shingeki.no.Kyojin.S03E12.1080p.BluRay.x265.mkv" << "Shingeki no Kyojin" << 3 << 12.0 << false;
        QTest::newRow("version") << "[Erai-raws] Bocchi the Rock! - 12v2 [1080p][Multiple Subtitle].mkv" << "Bocchi the Rock!" << 0 << 12.0 << false;
        QTest::newRow("ep") << "Yuru Camp EP03 [BD 1080p HEVC].mkv" << "Yuru Camp" << 0 << 3.0 << false;
        QTest::newRow("japanese") << "けいおん！ 第07話「クリスマス！」.mp4" << "けいおん！" << 0 << 7.0 << false;
        QTest::newRow("season title") << "[Group] Kaguya-sama S2 - 04 [720p].mkv" << "Kaguya-sama" << 2 << 4.0 << false;
        QTest::newRow("2nd season") << "Mushoku Tensei 2nd Season - 03.mkv" << "Mushoku Tensei" << 2 << 3.0 << false;
        QTest::newRow("ova") << "[Group] K-On! OVA [BD].mkv" << "K-On!" << 0 << -1.0 << true;
        QTest::newRow("bare number") << "Mononoke 08.mkv" << "Mononoke" << 0 << 8.0 << false;
        QTest::newRow("movie year") << "Kimi no Na wa (2016) [BD 1080p].mkv" << "Kimi no Na wa" << 0 << -1.0 << false;
        QTest::newRow("half") << "Show - 12.5 [WEB].mkv" << "Show" << 0 << 12.5 << false;
    }
    void parse()
    {
        QFETCH(QString, file);
        auto p = names::parse(file);
        QTEST(p.title, "title");
        QTEST(p.season, "season");
        QTEST(p.episode, "episode");
        QTEST(p.special, "special");
    }
    void lang()
    {
        QCOMPARE(names::subtitleLang("ep01.ja.srt"), names::Lang::Japanese);
        QCOMPARE(names::subtitleLang("[Group] Show - 01 [jpn].ass"), names::Lang::Japanese);
        QCOMPARE(names::subtitleLang("Show 01.en.ass"), names::Lang::English);
        QCOMPARE(names::subtitleLang("Show 01.ass"), names::Lang::Unknown);
    }
};

QTEST_GUILESS_MAIN(TestNames)
#include "tst_names.moc"
