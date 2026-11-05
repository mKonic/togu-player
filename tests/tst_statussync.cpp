// Anki card state -> word status, as in the design doc.
#include "core/anki/statussync.h"

#include <QTest>

using statussync::CardState;

class TestStatusSync : public QObject {
    Q_OBJECT
private slots:
    void statusFor_data()
    {
        QTest::addColumn<QString>("type");
        QTest::addColumn<bool>("suspended");
        QTest::addColumn<double>("interval");
        QTest::addColumn<double>("stability");
        QTest::addColumn<int>("lapses");
        QTest::addColumn<int>("previous");
        QTest::addColumn<int>("expected");
        const int U = int(Status::Unknown), T = int(Status::Tracked), L = int(Status::Learning), K = int(Status::Known);
        QTest::newRow("new") << "new" << false << 0.0 << -1.0 << 0 << U << T;
        QTest::newRow("learning") << "learning" << false << 0.0 << -1.0 << 0 << U << L;
        QTest::newRow("relearning drops known") << "relearning" << false << 1.0 << -1.0 << 1 << K << L;
        QTest::newRow("young review") << "review" << false << 10.0 << -1.0 << 0 << U << L;
        QTest::newRow("mature review") << "review" << false << 21.0 << -1.0 << 0 << U << K;
        QTest::newRow("fsrs stability wins") << "review" << false << 30.0 << 12.0 << 0 << U << L;
        QTest::newRow("fsrs strong") << "review" << false << 5.0 << 40.0 << 0 << U << K;
        QTest::newRow("no flapping") << "review" << false << 15.0 << -1.0 << 0 << K << K;
        QTest::newRow("leech") << "review" << false << 15.0 << -1.0 << 8 << K << L;
        QTest::newRow("leech mature") << "review" << false << 25.0 << -1.0 << 9 << U << K;
        QTest::newRow("suspended keeps") << "review" << true << 30.0 << -1.0 << 0 << L << L;
    }
    void statusFor()
    {
        QFETCH(QString, type);
        QFETCH(bool, suspended);
        QFETCH(double, interval);
        QFETCH(double, stability);
        QFETCH(int, lapses);
        QFETCH(int, previous);
        QFETCH(int, expected);
        CardState c{type, suspended, interval, stability, lapses};
        QCOMPARE(int(statussync::statusFor(c, {}, Status(previous))), expected);
    }
    void weakestCard()
    {
        QCOMPARE(statussync::combine({Status::Known, Status::Learning}), Status::Learning);
        QCOMPARE(statussync::combine({Status::Known, Status::Unknown}), Status::Known);
        QCOMPARE(statussync::combine({Status::Tracked, Status::Known}), Status::Tracked);
    }
};

QTEST_GUILESS_MAIN(TestStatusSync)
#include "tst_statussync.moc"
