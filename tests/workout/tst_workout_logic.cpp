#include <QtTest>

#include "maptesttracker.h"
#include "sensorsecondaccumulator.h"
#include "targetalert.h"

class TstWorkoutLogic : public QObject
{
    Q_OBJECT

private slots:
    void accumulator_startsEmpty();
    void accumulator_averagesEachMetricPerRider();
    void accumulator_keepsLatestPedalAndOxygen();
    void accumulator_resetClearsEverything();
    void mapTest_onTargetNeverFails();
    void mapTest_failsAfterTenConsecutiveSeconds();
    void mapTest_failsAfterTwentySecondsInAStep();
    void mapTest_unscoredSecondsBreakTheStreak();
    void mapTest_completeStepResetsStepTotal();
    void mapTest_result();
    void targetAlert_inRangeIsSilent();
    void targetAlert_lowAndHighRespectToggles();
    void targetAlert_cooldownPerDirection();
};

void TstWorkoutLogic::accumulator_startsEmpty()
{
    const SensorSecondAccumulator samples(2);
    const RiderSecond &rider = samples.rider(1);
    QCOMPARE(rider.heartRate, -1.0);
    QCOMPARE(rider.cadence, -1.0);
    QCOMPARE(rider.speed, -1.0);
    QCOMPARE(rider.power, -1.0);
    QCOMPARE(rider.rightPedalPercent, -1.0);
    QCOMPARE(rider.saturatedHemoglobinPercent, -1.0);
    QCOMPARE(rider.totalHemoglobinConcentration, -1.0);
}

void TstWorkoutLogic::accumulator_averagesEachMetricPerRider()
{
    using Metric = SensorSecondAccumulator::Metric;
    SensorSecondAccumulator samples(2);
    samples.addSample(0, Metric::Power, 200);
    samples.addSample(0, Metric::Power, 250);
    samples.addSample(0, Metric::Power, 300);
    samples.addSample(0, Metric::HeartRate, 140);
    samples.addSample(1, Metric::Cadence, 80);
    samples.addSample(1, Metric::Cadence, 90);
    samples.addSample(1, Metric::Speed, 30.5);

    QCOMPARE(samples.rider(0).power, 250.0);
    QCOMPARE(samples.rider(0).heartRate, 140.0);
    QCOMPARE(samples.rider(0).cadence, -1.0);
    QCOMPARE(samples.rider(1).cadence, 85.0);
    QCOMPARE(samples.rider(1).speed, 30.5);
    QCOMPARE(samples.rider(1).power, -1.0);
}

void TstWorkoutLogic::accumulator_keepsLatestPedalAndOxygen()
{
    SensorSecondAccumulator samples(1);
    samples.setRightPedalPercent(0, 48);
    samples.setRightPedalPercent(0, 52);
    samples.setHemoglobin(0, 65.0, 12.4);
    QCOMPARE(samples.rider(0).rightPedalPercent, 52.0);
    QCOMPARE(samples.rider(0).saturatedHemoglobinPercent, 65.0);
    QCOMPARE(samples.rider(0).totalHemoglobinConcentration, 12.4);
}

void TstWorkoutLogic::accumulator_resetClearsEverything()
{
    using Metric = SensorSecondAccumulator::Metric;
    SensorSecondAccumulator samples(1);
    samples.addSample(0, Metric::Power, 300);
    samples.setRightPedalPercent(0, 50);
    samples.reset();
    QCOMPARE(samples.rider(0).power, -1.0);
    QCOMPARE(samples.rider(0).rightPedalPercent, -1.0);

    // The next second averages only its own samples.
    samples.addSample(0, Metric::Power, 100);
    QCOMPARE(samples.rider(0).power, 100.0);
}

void TstWorkoutLogic::mapTest_onTargetNeverFails()
{
    MapTestTracker test;
    for (int second = 0; second < MapTestTracker::kStepSeconds; ++second)
        QVERIFY(!test.recordSecond(true, 250, 250, 10));
    QCOMPARE(test.secondsOffTarget(), 0);
    QVERIFY(!test.offTargetThisSecond());
}

void TstWorkoutLogic::mapTest_failsAfterTenConsecutiveSeconds()
{
    MapTestTracker test;
    for (int second = 1; second < MapTestTracker::kMaxConsecutiveOffTarget; ++second)
        QVERIFY(!test.recordSecond(true, 200, 250, 10));
    QVERIFY(test.offTargetThisSecond());
    QVERIFY(test.recordSecond(true, 200, 250, 10));
}

void TstWorkoutLogic::mapTest_failsAfterTwentySecondsInAStep()
{
    MapTestTracker test;
    // 9 seconds off target then 1 on: never 10 in a row, so only the step
    // total (20) can end the test, on the 2nd off second of the 3rd round.
    for (int round = 0; round < 2; ++round) {
        for (int second = 0; second < 9; ++second)
            QVERIFY(!test.recordSecond(true, 200, 250, 10));
        QVERIFY(!test.recordSecond(true, 260, 250, 10));
    }
    QCOMPARE(test.secondsOffTarget(), 18);
    QVERIFY(!test.recordSecond(true, 200, 250, 10));
    QVERIFY(test.recordSecond(true, 200, 250, 10));
    QCOMPARE(test.secondsOffTarget(), MapTestTracker::kMaxSecondsOffTarget);
}

void TstWorkoutLogic::mapTest_unscoredSecondsBreakTheStreak()
{
    MapTestTracker test;
    for (int second = 0; second < 5; ++second)
        test.recordSecond(true, 200, 250, 10);
    QCOMPARE(test.consecutiveOffTarget(), 5);
    QVERIFY(!test.recordSecond(false, 200, 250, 10));
    QCOMPARE(test.consecutiveOffTarget(), 0);
    QCOMPARE(test.secondsOffTarget(), 5);
}

void TstWorkoutLogic::mapTest_completeStepResetsStepTotal()
{
    MapTestTracker test;
    for (int second = 0; second < 5; ++second)
        test.recordSecond(true, 200, 250, 10);
    QCOMPARE(test.completeStep(), 1);
    QCOMPARE(test.step(), 2);
    QCOMPARE(test.secondsOffTarget(), 0);
}

void TstWorkoutLogic::mapTest_result()
{
    // Failing a 300 W step halfway: (300 - 30) + 0.5 * 30 = 285 W MAP.
    QCOMPARE(MapTestTracker::mapWatts(300, 90), 285.0);
    QCOMPARE(MapTestTracker::mapWatts(300, 0), 270.0);
    QCOMPARE(MapTestTracker::estimatedFtp(285.0), 214);
}

QTEST_GUILESS_MAIN(TstWorkoutLogic)
void TstWorkoutLogic::targetAlert_inRangeIsSilent()
{
    TargetAlert alert;
    QCOMPARE(alert.check(200, 200, 10, true, true, 0), TargetAlert::Breach::None);
    QCOMPARE(alert.check(190, 200, 10, true, true, 0), TargetAlert::Breach::None);
    QCOMPARE(alert.check(210, 200, 10, true, true, 0), TargetAlert::Breach::None);
}

void TstWorkoutLogic::targetAlert_lowAndHighRespectToggles()
{
    TargetAlert alert;
    QCOMPARE(alert.check(150, 200, 10, false, true, 0), TargetAlert::Breach::None);
    QCOMPARE(alert.check(150, 200, 10, true, true, 0), TargetAlert::Breach::Low);
    QCOMPARE(alert.check(250, 200, 10, true, false, 0), TargetAlert::Breach::None);
    QCOMPARE(alert.check(250, 200, 10, true, true, 0), TargetAlert::Breach::High);
}

void TstWorkoutLogic::targetAlert_cooldownPerDirection()
{
    TargetAlert alert;
    QCOMPARE(alert.check(150, 200, 10, true, true, 1000), TargetAlert::Breach::Low);
    // Same direction stays quiet for the cooldown, the other one is independent.
    QCOMPARE(alert.check(150, 200, 10, true, true, 1000 + TargetAlert::kCooldownMs - 1),
             TargetAlert::Breach::None);
    QCOMPARE(alert.check(250, 200, 10, true, true, 2000), TargetAlert::Breach::High);
    QCOMPARE(alert.check(150, 200, 10, true, true, 1000 + TargetAlert::kCooldownMs),
             TargetAlert::Breach::Low);
}

#include "tst_workout_logic.moc"
