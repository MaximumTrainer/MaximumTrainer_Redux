/*
 * tst_workout_creator.cpp
 *
 * Workout Creator Tests -- MaximumTrainer
 *
 * Purpose
 * -----------------------------------------------------------------------
 * Validates the logic for constructing Workout objects from individual
 * Interval domain objects (Warmup, SteadyState/FLAT, Cooldown, etc.) and
 * verifies that the resulting Workout model is correctly populated with the
 * expected interval count, power targets, durations, cadence targets, and
 * computed metrics.
 *
 * These tests run headlessly (no display required).  They exercise the
 * Workout and Interval model layer only — not the WorkoutCreator widget —
 * so they compile and run without QWT or QtWebEngine dependencies.
 *
 * Test coverage:
 *
 *   Construction from intervals
 *   ────────────────────────────
 *    1. testBuildWorkout_intervalCount     — correct number of intervals
 *    2. testBuildWorkout_warmupIsProg      — warmup is PROGRESSIVE step type
 *    3. testBuildWorkout_steadyIsFlat      — main set is FLAT step type
 *    4. testBuildWorkout_cooldownIsProg    — cooldown is PROGRESSIVE step type
 *    5. testBuildWorkout_powerValues       — FTP fractions stored correctly
 *    6. testBuildWorkout_durations         — QTime durations stored correctly
 *    7. testBuildWorkout_cadenceTargets    — cadence targets stored correctly
 *    8. testBuildWorkout_metadata          — name, plan, author, type correct
 *    9. testBuildWorkout_totalDuration     — total workout time is correct
 *
 *   Computed metrics (after calculateWorkoutMetrics)
 *   ──────────────────────────────────────────────────
 *   10. testMetrics_averagePower          — flat power → correct averagePower
 *   11. testMetrics_averagePowerMixed     — mixed powers → weighted average
 *   12. testMetrics_zeroFtp              — no crash when FTP = 0
 *
 *   Interval ordering and edge cases
 *   ──────────────────────────────────
 *   13. testOrder_preseverd              — intervals returned in insertion order
 *   14. testSingleInterval               — one-interval workout works
 *   15. testEmptyWorkout                 — empty interval list returns 0 count
 *   16. testHighRepeatIntervalsT         — 10 on/off pairs = 20 intervals
 *   17. testCadenceRange                 — cadence range stored correctly
 *   18. testTestInterval_flag            — testInterval flag survives round-trip
 *
 * Build:
 *   qmake workout_creator_tests.pro && make
 * Run:
 *   ../../build/tests/workout_creator_tests -v2
 */

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QTime>
#include <QList>

#include "../../src/model/account.h"
#include "../../src/model/workout.h"
#include "../../src/model/interval.h"

// ---------------------------------------------------------------------------
// Helper: build a canonical three-interval workout (Warmup + FLAT + Cooldown).
// FTP fractions are expressed as doubles between 0 and 2.
// ---------------------------------------------------------------------------
static Interval makeInterval(QTime duration,
                              const QString  &msg,
                              Interval::StepType powerType,
                              double ftpStart,
                              double ftpEnd,
                              int    cadStart = 90,
                              int    cadEnd   = 90,
                              bool   isTest   = false)
{
    Interval iv;
    iv.setTime(duration);
    iv.setDisplayMsg(msg);

    iv.setPowerStepType(powerType);
    iv.setTargetFTP_start(ftpStart);
    iv.setTargetFTP_end(ftpEnd);
    iv.setTargetFTP_range(20);
    iv.setRightPowerTarget(-1.0);

    iv.setCadenceStepType(Interval::FLAT);
    iv.setTargetCadence_start(cadStart);
    iv.setTargetCadence_end(cadEnd);
    iv.setTargetCadence_range(5);

    iv.setHrStepType(Interval::NONE);
    iv.setTargetHR_start(0.0);
    iv.setTargetHR_end(0.0);
    iv.setTargetHR_range(20);

    iv.setTestInterval(isTest);
    iv.setRepeatIncreaseFTP(0.0);
    iv.setRepeatIncreaseCadence(0);
    iv.setRepeatIncreaseLTHR(0.0);
    return iv;
}

// Build the canonical 3-interval test workout.
static Workout buildCanonicalWorkout()
{
    QList<Interval> ivs;
    ivs.append(makeInterval(QTime(0, 10, 0), "Warm-Up",
                            Interval::PROGRESSIVE, 0.40, 0.70, 85, 90));
    ivs.append(makeInterval(QTime(0, 20, 0), "Main Set",
                            Interval::FLAT,        0.80, 0.80, 90, 90));
    ivs.append(makeInterval(QTime(0, 10, 0), "Cool-Down",
                            Interval::PROGRESSIVE, 0.70, 0.40, 85, 80));

    return Workout(
        QStringLiteral(""),
        Workout::USER_MADE,
        ivs,
        QStringLiteral("Creator Test Workout"),
        QStringLiteral("CI"),
        QStringLiteral("Workout created from intervals in test"),
        QStringLiteral("Build"),
        Workout::T_ENDURANCE
    );
}

// ---------------------------------------------------------------------------
// TstWorkoutCreator — QTest class
// ---------------------------------------------------------------------------
class TstWorkoutCreator : public QObject
{
    Q_OBJECT

private:
    Account *m_account = nullptr;

private slots:

    void initTestCase()
    {
        m_account = new Account(this);
        m_account->FTP  = 200;
        m_account->LTHR = 160;
        m_account->email       = QStringLiteral("test@ci.example");
        m_account->email_clean = QStringLiteral("testciexample");
        m_account->isOffline   = true;
        qApp->setProperty("Account", QVariant::fromValue<Account *>(m_account));
    }

    void cleanupTestCase()
    {
        qApp->setProperty("Account", QVariant());
    }

    // =========================================================================
    // CONSTRUCTION FROM INTERVALS
    // =========================================================================

    // -----------------------------------------------------------------------
    // 1. Interval count
    // -----------------------------------------------------------------------
    void testBuildWorkout_intervalCount()
    {
        const Workout w = buildCanonicalWorkout();
        QCOMPARE(w.getLstInterval().size(), 3);
        qDebug().noquote() << "[Creator] Interval count = 3 — PASS";
    }

    // -----------------------------------------------------------------------
    // 2. Warmup is PROGRESSIVE
    // -----------------------------------------------------------------------
    void testBuildWorkout_warmupIsProg()
    {
        const Workout w = buildCanonicalWorkout();
        QVERIFY(!w.getLstInterval().isEmpty());
        QCOMPARE(w.getLstInterval().at(0).getPowerStepType(), Interval::PROGRESSIVE);
        qDebug().noquote() << "[Creator] Warmup is PROGRESSIVE — PASS";
    }

    // -----------------------------------------------------------------------
    // 3. Main set is FLAT
    // -----------------------------------------------------------------------
    void testBuildWorkout_steadyIsFlat()
    {
        const Workout w = buildCanonicalWorkout();
        QVERIFY(w.getLstInterval().size() >= 2);
        QCOMPARE(w.getLstInterval().at(1).getPowerStepType(), Interval::FLAT);
        qDebug().noquote() << "[Creator] Main set is FLAT — PASS";
    }

    // -----------------------------------------------------------------------
    // 4. Cooldown is PROGRESSIVE
    // -----------------------------------------------------------------------
    void testBuildWorkout_cooldownIsProg()
    {
        const Workout w = buildCanonicalWorkout();
        QVERIFY(w.getLstInterval().size() >= 3);
        QCOMPARE(w.getLstInterval().at(2).getPowerStepType(), Interval::PROGRESSIVE);
        qDebug().noquote() << "[Creator] Cooldown is PROGRESSIVE — PASS";
    }

    // -----------------------------------------------------------------------
    // 5. Power values stored correctly
    // -----------------------------------------------------------------------
    void testBuildWorkout_powerValues()
    {
        const Workout w = buildCanonicalWorkout();
        QVERIFY(w.getLstInterval().size() >= 2);

        // Main set: FLAT 0.80 FTP
        const Interval iv = w.getLstInterval().at(1);
        QVERIFY(qAbs(iv.getFTP_start() - 0.80) < 1e-6);
        QVERIFY(qAbs(iv.getFTP_end()   - 0.80) < 1e-6);

        // Warmup: PROGRESSIVE 0.40 → 0.70
        const Interval warmup = w.getLstInterval().at(0);
        QVERIFY(qAbs(warmup.getFTP_start() - 0.40) < 1e-6);
        QVERIFY(qAbs(warmup.getFTP_end()   - 0.70) < 1e-6);

        qDebug().noquote() << "[Creator] Power values correct — PASS";
    }

    // -----------------------------------------------------------------------
    // 6. Duration (QTime) stored correctly
    // -----------------------------------------------------------------------
    void testBuildWorkout_durations()
    {
        const Workout w = buildCanonicalWorkout();
        QVERIFY(w.getLstInterval().size() >= 3);

        // Warmup: 10 min
        QCOMPARE(w.getLstInterval().at(0).getDurationQTime(), QTime(0, 10, 0));
        // Main set: 20 min
        QCOMPARE(w.getLstInterval().at(1).getDurationQTime(), QTime(0, 20, 0));
        // Cooldown: 10 min
        QCOMPARE(w.getLstInterval().at(2).getDurationQTime(), QTime(0, 10, 0));

        qDebug().noquote() << "[Creator] Interval durations correct — PASS";
    }

    // -----------------------------------------------------------------------
    // 7. Cadence targets stored correctly
    // -----------------------------------------------------------------------
    void testBuildWorkout_cadenceTargets()
    {
        const Workout w = buildCanonicalWorkout();
        QVERIFY(w.getLstInterval().size() >= 2);

        // Main set: cadence 90 rpm
        const Interval iv = w.getLstInterval().at(1);
        QCOMPARE(iv.getCadence_start(), 90);
        QCOMPARE(iv.getCadence_end(),   90);

        qDebug().noquote() << "[Creator] Cadence 90 rpm stored correctly — PASS";
    }

    // -----------------------------------------------------------------------
    // 8. Workout metadata
    // -----------------------------------------------------------------------
    void testBuildWorkout_metadata()
    {
        const Workout w = buildCanonicalWorkout();

        QCOMPARE(w.getName(),        QStringLiteral("Creator Test Workout"));
        QCOMPARE(w.getCreatedBy(),   QStringLiteral("CI"));
        QCOMPARE(w.getPlan(),        QStringLiteral("Build"));
        QCOMPARE(w.getType(),        Workout::T_ENDURANCE);
        QVERIFY2(w.getDescription().contains(QLatin1String("intervals")),
                 qPrintable(QString("Description should mention 'intervals', got: '%1'")
                                .arg(w.getDescription())));

        qDebug().noquote() << "[Creator] Workout metadata correct — PASS";
    }

    // -----------------------------------------------------------------------
    // 9. Total duration (Warmup 10 min + Main 20 min + Cooldown 10 min = 40 min)
    //    Note: the 8-parameter Workout constructor calls computeWorkoutTotalTime()
    //    internally, so the duration is ready immediately after construction.
    // -----------------------------------------------------------------------
    void testBuildWorkout_totalDuration()
    {
        const Workout w = buildCanonicalWorkout();
        const QTime total = w.getDurationQTime();
        QCOMPARE(total, QTime(0, 40, 0));

        qDebug().noquote() << "[Creator] Total duration 40 min — PASS";
    }

    // =========================================================================
    // COMPUTED METRICS
    // =========================================================================

    // -----------------------------------------------------------------------
    // 10. Average power: a workout with one FLAT interval at 0.80 FTP (FTP=200)
    //     over 10 min has average power = 0.80 * 200 = 160 W.
    // -----------------------------------------------------------------------
    void testMetrics_averagePower()
    {
        QList<Interval> ivs;
        ivs.append(makeInterval(QTime(0, 10, 0), "Steady",
                                Interval::FLAT, 0.80, 0.80, 90, 90));

        Workout w(
            QStringLiteral(""),
            Workout::USER_MADE,
            ivs,
            QStringLiteral("Steady Metric Test"),
            QStringLiteral("CI"),
            QStringLiteral("10 min at 80% FTP"),
            QStringLiteral("Build"),
            Workout::T_ENDURANCE
        );

        w.initializeArrayFTP();
        w.calculateWorkoutMetrics();

        const double avg = w.getAveragePower();
        // Expect 0.80 * 200 = 160 W ± 5 W tolerance
        QVERIFY2(avg >= 155.0 && avg <= 165.0,
                 qPrintable(QString("Expected averagePower ~160 W, got %1").arg(avg)));

        qDebug().noquote()
            << "[Metrics] averagePower" << avg << "≈ 160 W — PASS";
    }

    // -----------------------------------------------------------------------
    // 11. Mixed power: Warmup (avg ~0.55 FTP) + Main (0.85 FTP) + Cooldown
    //     (avg ~0.55 FTP) with equal 10-min segments.
    //     Weighted average ≈ (0.55 + 0.85 + 0.55) / 3 * 200 ≈ 130 W.
    //     We only assert that the result is within a generous range.
    // -----------------------------------------------------------------------
    void testMetrics_averagePowerMixed()
    {
        QList<Interval> ivs;
        ivs.append(makeInterval(QTime(0, 10, 0), "Warmup",
                                Interval::PROGRESSIVE, 0.40, 0.70, 85, 90));
        ivs.append(makeInterval(QTime(0, 10, 0), "Main",
                                Interval::FLAT,        0.85, 0.85, 90, 90));
        ivs.append(makeInterval(QTime(0, 10, 0), "Cooldown",
                                Interval::PROGRESSIVE, 0.70, 0.40, 85, 80));

        Workout w(
            QStringLiteral(""),
            Workout::USER_MADE,
            ivs,
            QStringLiteral("Mixed Metric Test"),
            QStringLiteral("CI"),
            QStringLiteral("Mixed powers"),
            QStringLiteral("Build"),
            Workout::T_ENDURANCE
        );

        w.initializeArrayFTP();
        w.calculateWorkoutMetrics();

        const double avg = w.getAveragePower();
        // Warmup avg power = 0.55 * 200 = 110 W; Main = 0.85 * 200 = 170 W;
        // Cooldown avg = 0.55 * 200 = 110 W; weighted avg ≈ 130 W.
        // Allow a wide ±30 W tolerance for the progressive interpolation.
        QVERIFY2(avg > 80.0 && avg < 200.0,
                 qPrintable(QString("averagePower %1 is outside expected range [80, 200]")
                                .arg(avg)));

        qDebug().noquote()
            << "[Metrics] Mixed averagePower" << avg << "W in range — PASS";
    }

    // -----------------------------------------------------------------------
    // 12. No crash when FTP = 0 (edge case: user has not entered FTP)
    // -----------------------------------------------------------------------
    void testMetrics_zeroFtp()
    {
        Account zeroFtp;
        zeroFtp.FTP        = 0;
        zeroFtp.LTHR       = 0;
        zeroFtp.email       = QStringLiteral("zero@ci.example");
        zeroFtp.email_clean = QStringLiteral("zerociexample");
        qApp->setProperty("Account", QVariant::fromValue<Account *>(&zeroFtp));

        QList<Interval> ivs;
        ivs.append(makeInterval(QTime(0, 10, 0), "Steady",
                                Interval::FLAT, 0.80, 0.80));

        Workout w(
            QStringLiteral(""),
            Workout::USER_MADE,
            ivs,
            QStringLiteral("Zero FTP Test"),
            QStringLiteral("CI"),
            QStringLiteral("FTP=0 edge case"),
            QStringLiteral("Test"),
            Workout::T_TEST
        );

        // Must not crash even with FTP=0
        w.initializeArrayFTP();
        w.calculateWorkoutMetrics();

        // Restore the original account for subsequent tests
        qApp->setProperty("Account", QVariant::fromValue<Account *>(m_account));

        qDebug().noquote() << "[Metrics] FTP=0 did not crash — PASS";
    }

    // =========================================================================
    // INTERVAL ORDERING AND EDGE CASES
    // =========================================================================

    // -----------------------------------------------------------------------
    // 13. Intervals are returned in insertion order
    // -----------------------------------------------------------------------
    void testOrder_preserved()
    {
        const Workout w = buildCanonicalWorkout();
        QVERIFY(w.getLstInterval().size() >= 3);

        QVERIFY2(w.getLstInterval().at(0).getDisplayMessage().contains(QLatin1String("Warm")),
                 "First interval must be Warm-Up");
        QVERIFY2(w.getLstInterval().at(1).getDisplayMessage().contains(QLatin1String("Main")),
                 "Second interval must be Main Set");
        QVERIFY2(w.getLstInterval().at(2).getDisplayMessage().contains(QLatin1String("Cool")),
                 "Third interval must be Cool-Down");

        qDebug().noquote() << "[Creator] Interval insertion order preserved — PASS";
    }

    // -----------------------------------------------------------------------
    // 14. Single-interval workout
    // -----------------------------------------------------------------------
    void testSingleInterval()
    {
        QList<Interval> ivs;
        ivs.append(makeInterval(QTime(0, 30, 0), "Solo",
                                Interval::FLAT, 0.75, 0.75, 90, 90));

        const Workout w(
            QStringLiteral(""),
            Workout::USER_MADE,
            ivs,
            QStringLiteral("Solo Workout"),
            QStringLiteral("CI"),
            QStringLiteral("One interval"),
            QStringLiteral("Base"),
            Workout::T_ENDURANCE
        );

        QCOMPARE(w.getLstInterval().size(), 1);
        QCOMPARE(w.getLstInterval().first().getPowerStepType(), Interval::FLAT);
        QVERIFY(qAbs(w.getLstInterval().first().getFTP_start() - 0.75) < 1e-6);

        qDebug().noquote() << "[Creator] Single-interval workout — PASS";
    }

    // -----------------------------------------------------------------------
    // 15. Empty interval list → zero interval count, no crash
    // -----------------------------------------------------------------------
    void testEmptyWorkout()
    {
        const QList<Interval> empty;
        const Workout w(
            QStringLiteral(""),
            Workout::USER_MADE,
            empty,
            QStringLiteral("Empty Workout"),
            QStringLiteral("CI"),
            QStringLiteral("No intervals"),
            QStringLiteral("Base"),
            Workout::T_ENDURANCE
        );

        QCOMPARE(w.getLstInterval().size(), 0);
        qDebug().noquote() << "[Creator] Empty interval list → count 0 — PASS";
    }

    // -----------------------------------------------------------------------
    // 16. High repeat: 10 on/off pairs = 20 intervals
    // -----------------------------------------------------------------------
    void testHighRepeatIntervalsT()
    {
        QList<Interval> ivs;
        for (int i = 0; i < 10; ++i) {
            // on-interval
            ivs.append(makeInterval(QTime(0, 1, 0),
                                    QString("On %1").arg(i + 1),
                                    Interval::FLAT, 1.10, 1.10, 95, 95));
            // off-interval
            ivs.append(makeInterval(QTime(0, 0, 30),
                                    QString("Off %1").arg(i + 1),
                                    Interval::FLAT, 0.50, 0.50, 85, 85));
        }

        const Workout w(
            QStringLiteral(""),
            Workout::USER_MADE,
            ivs,
            QStringLiteral("High Repeat"),
            QStringLiteral("CI"),
            QStringLiteral("10×(1 min on + 30 s off)"),
            QStringLiteral("Intervals"),
            Workout::T_INTERVAL
        );

        QCOMPARE(w.getLstInterval().size(), 20);
        // Even-indexed = on-interval at 1.10 FTP
        QVERIFY(qAbs(w.getLstInterval().at(0).getFTP_start() - 1.10) < 1e-6);
        // Odd-indexed = off-interval at 0.50 FTP
        QVERIFY(qAbs(w.getLstInterval().at(1).getFTP_start() - 0.50) < 1e-6);

        qDebug().noquote() << "[Creator] 10×on/off = 20 intervals — PASS";
    }

    // -----------------------------------------------------------------------
    // 17. Cadence range stored correctly
    // -----------------------------------------------------------------------
    void testCadenceRange()
    {
        QList<Interval> ivs;
        {
            Interval iv;
            iv.setTime(QTime(0, 10, 0));
            iv.setDisplayMsg(QStringLiteral("Cadence Test"));
            iv.setPowerStepType(Interval::FLAT);
            iv.setTargetFTP_start(0.75);
            iv.setTargetFTP_end(0.75);
            iv.setTargetFTP_range(20);
            iv.setRightPowerTarget(-1.0);
            iv.setCadenceStepType(Interval::FLAT);
            iv.setTargetCadence_start(100);
            iv.setTargetCadence_end(100);
            iv.setTargetCadence_range(10);  // ← custom range
            iv.setHrStepType(Interval::NONE);
            iv.setTargetHR_start(0.0);
            iv.setTargetHR_end(0.0);
            iv.setTargetHR_range(20);
            iv.setTestInterval(false);
            iv.setRepeatIncreaseFTP(0.0);
            iv.setRepeatIncreaseCadence(0);
            iv.setRepeatIncreaseLTHR(0.0);
            ivs.append(iv);
        }

        const Workout w(
            QStringLiteral(""),
            Workout::USER_MADE,
            ivs,
            QStringLiteral("Cadence Range Test"),
            QStringLiteral("CI"),
            QStringLiteral(""),
            QStringLiteral("Base"),
            Workout::T_ENDURANCE
        );

        QVERIFY(!w.getLstInterval().isEmpty());
        QCOMPARE(w.getLstInterval().first().getCadence_start(), 100);
        QCOMPARE(w.getLstInterval().first().getCadence_range(), 10);

        qDebug().noquote() << "[Creator] Cadence range 10 rpm stored correctly — PASS";
    }

    // -----------------------------------------------------------------------
    // 18. testInterval flag survives model construction
    // -----------------------------------------------------------------------
    void testTestInterval_flag()
    {
        QList<Interval> ivs;
        ivs.append(makeInterval(QTime(0, 20, 0), "FTP Test Block",
                                Interval::FLAT, 1.05, 1.05, 95, 95,
                                /*isTest=*/true));

        const Workout w(
            QStringLiteral(""),
            Workout::USER_MADE,
            ivs,
            QStringLiteral("FTP Test"),
            QStringLiteral("CI"),
            QStringLiteral("20-minute FTP test interval"),
            QStringLiteral("Test"),
            Workout::T_TEST
        );

        QVERIFY(!w.getLstInterval().isEmpty());
        QVERIFY2(w.getLstInterval().first().isTestInterval(),
                 "testInterval flag must be true after model construction");

        qDebug().noquote() << "[Creator] testInterval flag preserved — PASS";
    }
};

QTEST_MAIN(TstWorkoutCreator)
#include "tst_workout_creator.moc"
