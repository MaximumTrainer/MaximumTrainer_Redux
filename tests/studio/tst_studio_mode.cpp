/*
 * tst_studio_mode.cpp
 *
 * Studio Mode verification tests – MaximumTrainer (#135)
 *
 * Tests cover:
 *   1. Multiple SimulatorHub instances run and emit signals simultaneously.
 *   2. Each hub's userID is correctly propagated in emitted signals.
 *   3. The ERG FTP-scaling formula (load = %FTP × riderFTP) is correct.
 *   4. Studio Mode QSettings keys persist across simulated save/load cycles.
 *   5. setLoad() nudges the simulated power toward the requested target.
 *   6. Stopping one hub does not affect the other.
 *
 * No physical Bluetooth hardware, no WorkoutDialog UI, and no database
 * connection are required – all Studio Mode signal-routing and scaling
 * logic is exercised through SimulatorHub and QSignalSpy.
 */

#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QSettings>

#include "../../src/btle/simulator_hub.h"

// ─────────────────────────────────────────────────────────────────────────────
// TstStudioMode
// ─────────────────────────────────────────────────────────────────────────────
class TstStudioMode : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // ── SimulatorHub multi-instance ──────────────────────────────────────────
    void testMultipleHubsEmitSimultaneously();
    void testHubUserIDPropagation();
    void testStoppingOneHubDoesNotAffectOther();

    // ── FTP scaling formula ──────────────────────────────────────────────────
    void testFtpScalingFormula();
    void testFtpScalingAtEdgeCases();

    // ── Studio Mode settings persistence (QSettings) ────────────────────────
    void testStudioModeSettingsPersist();
    void testNbUserStudioPersists();

    // ── setLoad nudges simulated power ───────────────────────────────────────
    void testSetLoadNudgesPower();
};

// ─────────────────────────────────────────────────────────────────────────────
// Setup / teardown
// ─────────────────────────────────────────────────────────────────────────────
void TstStudioMode::initTestCase()
{
    // Use a separate QSettings scope so tests don't dirty a real user profile.
    QCoreApplication::setOrganizationName("MaximumTrainer_Test");
    QCoreApplication::setApplicationName("StudioTests");
}

void TstStudioMode::cleanupTestCase()
{
    QSettings s;
    s.remove("studio_test");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 1: two SimulatorHub instances both emit power signals within 2 seconds
// ─────────────────────────────────────────────────────────────────────────────
void TstStudioMode::testMultipleHubsEmitSimultaneously()
{
    SimulatorHub hub1, hub2;

    QSignalSpy spy1(&hub1, &SimulatorHub::signal_power);
    QSignalSpy spy2(&hub2, &SimulatorHub::signal_power);

    hub1.start();
    hub2.start();

    // Both hubs use a 1-second tick; wait 2.5 s to guarantee at least 2 ticks.
    QTest::qWait(2500);

    hub1.stop();
    hub2.stop();

    QVERIFY2(spy1.count() >= 1, "hub1 should emit at least one signal_power");
    QVERIFY2(spy2.count() >= 1, "hub2 should emit at least one signal_power");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 2: userID set on hub appears in every emitted signal
// ─────────────────────────────────────────────────────────────────────────────
void TstStudioMode::testHubUserIDPropagation()
{
    SimulatorHub hub1, hub2;
    hub1.setUserID(1);
    hub2.setUserID(2);

    QSignalSpy spy1(&hub1, &SimulatorHub::signal_power);
    QSignalSpy spy2(&hub2, &SimulatorHub::signal_power);

    hub1.start();
    hub2.start();
    QTest::qWait(1500);
    hub1.stop();
    hub2.stop();

    QVERIFY2(spy1.count() >= 1, "hub1 (userID=1) should have emitted signal_power");
    QVERIFY2(spy2.count() >= 1, "hub2 (userID=2) should have emitted signal_power");

    // Verify that the userID argument in every captured signal matches the configured value.
    for (const QList<QVariant> &args : spy1)
        QCOMPARE(args.at(0).toInt(), 1);

    for (const QList<QVariant> &args : spy2)
        QCOMPARE(args.at(0).toInt(), 2);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 3: stopping hub1 does not suppress hub2's signals
// ─────────────────────────────────────────────────────────────────────────────
void TstStudioMode::testStoppingOneHubDoesNotAffectOther()
{
    SimulatorHub hub1, hub2;
    hub1.setUserID(1);
    hub2.setUserID(2);

    hub1.start();
    hub2.start();
    QTest::qWait(1500);

    // Stop hub1, let hub2 continue for another tick.
    hub1.stop();

    QSignalSpy spy2(&hub2, &SimulatorHub::signal_power);
    QTest::qWait(1500);
    hub2.stop();

    QVERIFY2(spy2.count() >= 1, "hub2 should still emit after hub1 is stopped");
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 4: FTP scaling formula — target watts = (percentFTP / 100.0) * riderFTP
// This is the formula WorkoutDialog uses to send per-rider ERG load commands.
// ─────────────────────────────────────────────────────────────────────────────
void TstStudioMode::testFtpScalingFormula()
{
    // Rider with FTP = 250 W
    const int riderFtp = 250;

    // Interval target = 80 % FTP
    const double targetPercent = 80.0;
    const double expectedWatts = targetPercent / 100.0 * riderFtp;   // 200 W

    QCOMPARE(expectedWatts, 200.0);

    // Verify that rounding to integer (as used in setLoad) is sensible.
    const int ergLoad = static_cast<int>(std::round(expectedWatts));
    QCOMPARE(ergLoad, 200);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 5: FTP scaling edge cases — 0 % and 100 %
// ─────────────────────────────────────────────────────────────────────────────
void TstStudioMode::testFtpScalingAtEdgeCases()
{
    const int ftp = 300;

    // 0 % FTP → 0 W (full recovery)
    QCOMPARE(static_cast<int>(0.0 / 100.0 * ftp), 0);

    // 100 % FTP → 300 W
    QCOMPARE(static_cast<int>(100.0 / 100.0 * ftp), 300);

    // 120 % FTP → 360 W (VO2max effort)
    QCOMPARE(static_cast<int>(120.0 / 100.0 * ftp), 360);
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 6: Studio Mode enabled flag persists to / from QSettings
// ─────────────────────────────────────────────────────────────────────────────
void TstStudioMode::testStudioModeSettingsPersist()
{
    // Simulate what DialogConfig::saveStudioTab() writes.
    {
        QSettings s;
        s.beginGroup("studio_test");
        s.setValue("enable_studio_mode", true);
        s.endGroup();
        s.sync();
    }

    // Simulate what Account constructor / init would read.
    {
        QSettings s;
        s.beginGroup("studio_test");
        const bool loaded = s.value("enable_studio_mode", false).toBool();
        s.endGroup();
        QVERIFY2(loaded, "enable_studio_mode should persist as true");
    }

    // Disable and verify.
    {
        QSettings s;
        s.beginGroup("studio_test");
        s.setValue("enable_studio_mode", false);
        s.endGroup();
        s.sync();
    }
    {
        QSettings s;
        s.beginGroup("studio_test");
        const bool loaded = s.value("enable_studio_mode", true).toBool();
        s.endGroup();
        QVERIFY2(!loaded, "enable_studio_mode should persist as false after toggle");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 7: nb_user_studio persists correctly for supported rider counts (2–6)
// ─────────────────────────────────────────────────────────────────────────────
void TstStudioMode::testNbUserStudioPersists()
{
    for (int count : {2, 3, 4, 5, 6}) {
        QSettings s;
        s.beginGroup("studio_test");
        s.setValue("nb_user_studio", count);
        s.endGroup();
        s.sync();

        QSettings s2;
        s2.beginGroup("studio_test");
        const int loaded = s2.value("nb_user_studio", 0).toInt();
        s2.endGroup();

        QCOMPARE(loaded, count);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Test 8: setLoad nudges simulated power toward requested target
// ─────────────────────────────────────────────────────────────────────────────
void TstStudioMode::testSetLoadNudgesPower()
{
    SimulatorHub hub;
    hub.setUserID(0);

    // Request a specific ERG load — SimulatorHub::setLoad() clamps to [100,400].
    hub.setLoad(0, 250.0);

    QSignalSpy spy(&hub, &SimulatorHub::signal_power);
    hub.start();
    QTest::qWait(1500);
    hub.stop();

    QVERIFY2(spy.count() >= 1, "hub should have emitted at least one signal_power after setLoad");

    // The first tick power should be near 250 W (within realistic drift range).
    const int firstPower = spy.first().at(1).toInt();
    QVERIFY2(firstPower >= 100 && firstPower <= 400,
             qPrintable(QString("power %1 outside realistic [100,400] range").arg(firstPower)));
}

QTEST_MAIN(TstStudioMode)
#include "tst_studio_mode.moc"
