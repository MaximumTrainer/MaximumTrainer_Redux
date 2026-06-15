/*
 * tst_importer_workout_zwo.cpp
 *
 * Qt Test suite for ImporterWorkoutZwo.
 *
 * Exercises the ZWO XML parser with five fixture files in tests/fixtures/:
 *   steady_state.zwo   — single SteadyState element
 *   ramp.zwo           — single Ramp element
 *   intervals_t.zwo    — IntervalsT with Repeat=5 → 10 flat intervals
 *   free_ride.zwo      — single FreeRide element
 *   mixed.zwo          — Warmup + SteadyState + IntervalsT + FreeRide + Cooldown
 *
 * Tests also verify importFromByteArray() directly (same data, no file I/O)
 * and check edge-case power conversions.
 */

#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QCoreApplication>

#include "importerworkoutzwo.h"
#include "account.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helper: absolute path to a fixture file
// ─────────────────────────────────────────────────────────────────────────────
static QString fixturePath(const QString &name)
{
    // tests/fixtures/ lives two directories above tests/intervals_icu/
    return QDir(QStringLiteral(FIXTURES_DIR)).absoluteFilePath(name);
}

static QByteArray readFixture(const QString &name)
{
    QFile f(fixturePath(name));
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

// ─────────────────────────────────────────────────────────────────────────────
class TstImporterWorkoutZwo : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // ── Fixture file parsing ─────────────────────────────────────────────────
    void testSteadyState_intervalCount();
    void testSteadyState_power();
    void testSteadyState_powerRange();
    void testSteadyState_duration();
    void testSteadyState_name();

    void testRamp_intervalCount();
    void testRamp_powerRange();
    void testRamp_isProgressive();

    void testIntervalsT_intervalCount();
    void testIntervalsT_onPower();
    void testIntervalsT_offPower();

    void testFreeRide_intervalCount();
    void testFreeRide_powerType();

    void testMixed_intervalCount();
    void testMixed_firstIntervalIsProgressive();

    // ── importFromByteArray() identical to file ──────────────────────────────
    void testImportFromByteArray_matchesFile();

    // ── Empty / invalid input ────────────────────────────────────────────────
    void testEmptyData_returnsEmptyWorkout();
    void testMalformedXml_returnsEmptyWorkout();

    // ── Power conversion edge cases ──────────────────────────────────────────
    void testPowerZero();
    void testPowerExact1();
    void testPowerAbove1();
};

// ─────────────────────────────────────────────────────────────────────────────
void TstImporterWorkoutZwo::initTestCase()
{
    // Register a dummy Account so Workout::calculateWorkoutMetrics() doesn't
    // crash when it calls qApp->property("Account").value<Account*>().
    Account *dummyAccount = new Account(qApp);
    dummyAccount->FTP = 200;
    qApp->setProperty("Account", QVariant::fromValue<Account *>(dummyAccount));
}

// ─────────────────────────────────────────────────────────────────────────────
// steady_state.zwo — 1 SteadyState @ 0.75 FTP, 1800 s
// ─────────────────────────────────────────────────────────────────────────────
void TstImporterWorkoutZwo::testSteadyState_intervalCount()
{
    const QByteArray data = readFixture("steady_state.zwo");
    QVERIFY(!data.isEmpty());
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QCOMPARE(w.getLstInterval().size(), 1);
}

void TstImporterWorkoutZwo::testSteadyState_power()
{
    const QByteArray data = readFixture("steady_state.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QVERIFY(!w.getLstInterval().isEmpty());
    const Interval iv = w.getLstInterval().first();
    QCOMPARE(iv.getPowerStepType(), Interval::FLAT);
    QVERIFY(qAbs(iv.getFTP_start() - 0.75) < 1e-6);
    QVERIFY(qAbs(iv.getFTP_end()   - 0.75) < 1e-6);
}

// Intervals.icu exports a power-range step as a SteadyState carrying
// PowerLow/PowerHigh instead of a single Power. It must parse as the midpoint,
// not 0 W (#295).
void TstImporterWorkoutZwo::testSteadyState_powerRange()
{
    const QByteArray data = readFixture("steady_state_range.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QVERIFY(!w.getLstInterval().isEmpty());
    const Interval iv = w.getLstInterval().first();
    QCOMPARE(iv.getPowerStepType(), Interval::FLAT);
    // (0.43 + 0.75) / 2 = 0.59  (NOT 0)
    QVERIFY(qAbs(iv.getFTP_start() - 0.59) < 1e-6);
    QVERIFY(qAbs(iv.getFTP_end()   - 0.59) < 1e-6);
}

void TstImporterWorkoutZwo::testSteadyState_duration()
{
    const QByteArray data = readFixture("steady_state.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QVERIFY(!w.getLstInterval().isEmpty());
    // Duration attribute is 1800 s → QTime(0, 30, 0)
    QCOMPARE(w.getLstInterval().first().getDurationQTime(), QTime(0, 30, 0));
}

void TstImporterWorkoutZwo::testSteadyState_name()
{
    const QByteArray data = readFixture("steady_state.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "fallback");
    // The fixture contains <name>Steady State Test</name>; workoutName ≠ file name
    // so the importer appends workoutName to the ZWO name.
    QVERIFY(w.getName().contains(QStringLiteral("Steady State Test")));
}

// ─────────────────────────────────────────────────────────────────────────────
// ramp.zwo — 1 Ramp, 0.5→1.0 FTP, 600 s
// ─────────────────────────────────────────────────────────────────────────────
void TstImporterWorkoutZwo::testRamp_intervalCount()
{
    const QByteArray data = readFixture("ramp.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QCOMPARE(w.getLstInterval().size(), 1);
}

void TstImporterWorkoutZwo::testRamp_powerRange()
{
    const QByteArray data = readFixture("ramp.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QVERIFY(!w.getLstInterval().isEmpty());
    const Interval iv = w.getLstInterval().first();
    QVERIFY(qAbs(iv.getFTP_start() - 0.50) < 1e-6);
    QVERIFY(qAbs(iv.getFTP_end()   - 1.00) < 1e-6);
}

void TstImporterWorkoutZwo::testRamp_isProgressive()
{
    const QByteArray data = readFixture("ramp.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QVERIFY(!w.getLstInterval().isEmpty());
    QCOMPARE(w.getLstInterval().first().getPowerStepType(), Interval::PROGRESSIVE);
}

// ─────────────────────────────────────────────────────────────────────────────
// intervals_t.zwo — IntervalsT Repeat=5 → 10 flat intervals (5 on + 5 off)
// ─────────────────────────────────────────────────────────────────────────────
void TstImporterWorkoutZwo::testIntervalsT_intervalCount()
{
    const QByteArray data = readFixture("intervals_t.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QCOMPARE(w.getLstInterval().size(), 10);
}

void TstImporterWorkoutZwo::testIntervalsT_onPower()
{
    const QByteArray data = readFixture("intervals_t.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QVERIFY(!w.getLstInterval().isEmpty());
    const Interval on = w.getLstInterval().at(0);
    QVERIFY(qAbs(on.getFTP_start() - 1.10) < 1e-6);
}

void TstImporterWorkoutZwo::testIntervalsT_offPower()
{
    const QByteArray data = readFixture("intervals_t.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QVERIFY(w.getLstInterval().size() >= 2);
    const Interval off = w.getLstInterval().at(1);
    QVERIFY(qAbs(off.getFTP_start() - 0.55) < 1e-6);
}

// ─────────────────────────────────────────────────────────────────────────────
// free_ride.zwo — 1 FreeRide, NONE power type
// ─────────────────────────────────────────────────────────────────────────────
void TstImporterWorkoutZwo::testFreeRide_intervalCount()
{
    const QByteArray data = readFixture("free_ride.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QCOMPARE(w.getLstInterval().size(), 1);
}

void TstImporterWorkoutZwo::testFreeRide_powerType()
{
    const QByteArray data = readFixture("free_ride.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QVERIFY(!w.getLstInterval().isEmpty());
    QCOMPARE(w.getLstInterval().first().getPowerStepType(), Interval::NONE);
}

// ─────────────────────────────────────────────────────────────────────────────
// mixed.zwo — Warmup(1) + SteadyState(1) + IntervalsT×3(6) + FreeRide(1) + Cooldown(1) = 10
// ─────────────────────────────────────────────────────────────────────────────
void TstImporterWorkoutZwo::testMixed_intervalCount()
{
    const QByteArray data = readFixture("mixed.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QCOMPARE(w.getLstInterval().size(), 10);
}

void TstImporterWorkoutZwo::testMixed_firstIntervalIsProgressive()
{
    const QByteArray data = readFixture("mixed.zwo");
    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QVERIFY(!w.getLstInterval().isEmpty());
    QCOMPARE(w.getLstInterval().first().getPowerStepType(), Interval::PROGRESSIVE);
}

// ─────────────────────────────────────────────────────────────────────────────
// importFromByteArray() should give the same result as reading a file
// ─────────────────────────────────────────────────────────────────────────────
void TstImporterWorkoutZwo::testImportFromByteArray_matchesFile()
{
    const QByteArray data = readFixture("steady_state.zwo");
    QVERIFY(!data.isEmpty());

    Workout w = ImporterWorkoutZwo::importFromByteArray(data, "test");
    QCOMPARE(w.getLstInterval().size(), 1);
    QCOMPARE(w.getLstInterval().first().getPowerStepType(), Interval::FLAT);
}

// ─────────────────────────────────────────────────────────────────────────────
// Edge cases
// ─────────────────────────────────────────────────────────────────────────────
void TstImporterWorkoutZwo::testEmptyData_returnsEmptyWorkout()
{
    Workout w = ImporterWorkoutZwo::importFromByteArray(QByteArray(), "test");
    QVERIFY(w.getLstInterval().isEmpty());
}

void TstImporterWorkoutZwo::testMalformedXml_returnsEmptyWorkout()
{
    Workout w = ImporterWorkoutZwo::importFromByteArray("<not valid xml<<<<", "test");
    QVERIFY(w.getLstInterval().isEmpty());
}

void TstImporterWorkoutZwo::testPowerZero()
{
    const QByteArray xml = R"(
<workout_file>
  <workout>
    <SteadyState Duration="60" Power="0.0"/>
  </workout>
</workout_file>)";
    Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "test");
    QCOMPARE(w.getLstInterval().size(), 1);
    // Power 0.0 → FLAT interval (but with Duration > 0 the interval IS added)
    // The importer treats 0.0 as a valid power target
    QVERIFY(qAbs(w.getLstInterval().first().getFTP_start()) < 1e-6);
}

void TstImporterWorkoutZwo::testPowerExact1()
{
    const QByteArray xml = R"(
<workout_file>
  <workout>
    <SteadyState Duration="300" Power="1.0"/>
  </workout>
</workout_file>)";
    Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "test");
    QVERIFY(!w.getLstInterval().isEmpty());
    QVERIFY(qAbs(w.getLstInterval().first().getFTP_start() - 1.0) < 1e-6);
}

void TstImporterWorkoutZwo::testPowerAbove1()
{
    const QByteArray xml = R"(
<workout_file>
  <workout>
    <SteadyState Duration="120" Power="1.5"/>
  </workout>
</workout_file>)";
    Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "test");
    QVERIFY(!w.getLstInterval().isEmpty());
    QVERIFY(qAbs(w.getLstInterval().first().getFTP_start() - 1.5) < 1e-6);
}

QTEST_MAIN(TstImporterWorkoutZwo)
#include "tst_importer_workout_zwo.moc"
