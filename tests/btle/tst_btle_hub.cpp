/*
 * tst_btle_hub.cpp
 *
 * Qt Test suite for BtleHub – the Bluetooth Low Energy sensor hub.
 *
 * All tests run without real hardware by injecting raw BLE characteristic
 * notification bytes via BtleHub::simulateNotification().  QSignalSpy
 * captures the signals emitted so we can assert on parsed values.
 *
 * Test groups
 * ──────────────────────────────────────────────────────────────────
 * HR parsing     – 8-bit, 16-bit, with RR-interval field
 * CSC parsing    – crank-only, wheel-only, combined, uint16 rollover,
 *                  first-measurement is discarded (delta-only protocol)
 * Power parsing  – basic watts value, negative power (track-stand)
 * FTMS parsing   – speed only, cadence only, power only, all three,
 *                  zero values
 * Trainer sims   – Elite (FTMS), Wahoo KICKR (power+CSC), Garmin Tacx (FTMS+CSC)
 */

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "../../src/btle/btle_hub.h"
#include "../../src/btle/simulator_hub.h"
#include "../../src/model/intervalsummaryutil.h"
#include "btle_device_simulator.h"

// ─────────────────────────────────────────────────────────────────────────────
// Tolerance for floating-point comparisons (km/h or RPM)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr double SPEED_TOLERANCE   = 0.1;  // km/h
static constexpr double CADENCE_TOLERANCE = 1.0;  // RPM

class TstBtleHub : public QObject
{
    Q_OBJECT

private slots:
    // ── Test initialisation ──────────────────────────────────────────────────
    void init();

    // ── Heart Rate parsing ───────────────────────────────────────────────────
    void testHr_uint8();
    void testHr_uint16();
    void testHr_uint8WithRR();
    void testHr_zero();
    void testHr_maxBpm();

    // ── CSC (Speed & Cadence) parsing ────────────────────────────────────────
    void testCsc_firstMeasurementDiscarded();
    void testCsc_crankOnly();
    void testCsc_wheelOnly();
    void testCsc_combined();
    void testCsc_uint16Rollover();
    void testCsc_standstill();

    // ── Cycling Power parsing ────────────────────────────────────────────────
    void testPower_positive();
    void testPower_zero();
    void testPower_negative();
    void testPower_tooShort_ignored();

    // ── FTMS Indoor Bike Data parsing ────────────────────────────────────────
    void testFtms_speedOnly();
    void testFtms_cadenceOnly();
    void testFtms_powerOnly();
    void testFtms_allThree();
    void testFtms_zeroValues();
    void testFtms_tooShort_ignored();

    // ── Brand-specific trainer simulations ───────────────────────────────────
    void testEliteTrainer_singlePacket();
    void testEliteTrainer_sequence();
    void testWahooKickr_powerAndCsc();
    void testWahooKickr_cscRollover();
    void testGarminTacx_ftmsPlusCsc();

    // ── SimulatorHub verification ────────────────────────────────────────────────
    void testSimulator_emitsHr();
    void testSimulator_emitsCadence();
    void testSimulator_emitsSpeed();
    void testSimulator_emitsPower();
    void testSimulator_valuesInRange();
    void testSimulator_stopStopsSignals();

    // ── Short-packet rejection (no crash on truncated data) ──────────────────
    void testHr_tooShort_ignored();
    void testCsc_tooShort_ignored();

    // ── FTMS multi-field layout (field skipping) ─────────────────────────────
    void testFtms_allOptionalFieldsBeforePowerSkipped();
    void testFtms_negativeInstantPower();
    void testFtms_heartRate();
    void testFtms_heartRateAfterEnergy();
    void testFtms_heartRateAfterAvgPower();
    void testFtms_heartRateFlagButTruncated_ignored();
    void testFtms_trainerProvidesHrSignal_firesOnce();
    void testFtms_noHr_noTrainerProvidesHrSignal();

    // ── SimulatorHub no-op slot verification ─────────────────────────────────
    void testSimulator_noOpSetLoad();
    void testSimulator_noOpSetSlope();

    // ── Battery Level parsing ─────────────────────────────────────────────────
    void testBattery_aboveThreshold_emits();
    void testBattery_atThreshold_emits();
    void testBattery_belowThreshold_emits();
    void testBattery_tooShort_ignored();
    void testBattery_clamped();

    // ── Interval summary power-adherence classification ───────────────────────
    void testIntervalSummary_met_exactTarget();
    void testIntervalSummary_met_upperBoundary();
    void testIntervalSummary_met_lowerBoundary();
    void testIntervalSummary_nearMiss_upperBoundary();
    void testIntervalSummary_nearMiss_lowerBoundary();
    void testIntervalSummary_missed_above();
    void testIntervalSummary_missed_below();
    void testIntervalSummary_zeroTarget_returnsMet();

private:
    BtleHub *hub = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
void TstBtleHub::init()
{
    delete hub;
    hub = new BtleHub();
}

// ─────────────────────────────────────────────────────────────────────────────
// Heart Rate tests
// ─────────────────────────────────────────────────────────────────────────────

void TstBtleHub::testHr_uint8()
{
    QSignalSpy spy(hub, &BtleHub::signal_hr);
    hub->simulateNotification(BTLE_UUID_HR_MEASUREMENT,
                              BtlePacketBuilder::heartRate(145));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 145);
}

void TstBtleHub::testHr_uint16()
{
    QSignalSpy spy(hub, &BtleHub::signal_hr);
    hub->simulateNotification(BTLE_UUID_HR_MEASUREMENT,
                              BtlePacketBuilder::heartRate(200, /*use16bit=*/true));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 200);
}

void TstBtleHub::testHr_uint8WithRR()
{
    // RR-interval data should be silently ignored; HR must still be parsed.
    QSignalSpy spy(hub, &BtleHub::signal_hr);
    hub->simulateNotification(BTLE_UUID_HR_MEASUREMENT,
                              BtlePacketBuilder::heartRateWithRR(160, 375));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 160);
}

void TstBtleHub::testHr_zero()
{
    QSignalSpy spy(hub, &BtleHub::signal_hr);
    hub->simulateNotification(BTLE_UUID_HR_MEASUREMENT,
                              BtlePacketBuilder::heartRate(0));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 0);
}

void TstBtleHub::testHr_maxBpm()
{
    QSignalSpy spy(hub, &BtleHub::signal_hr);
    hub->simulateNotification(BTLE_UUID_HR_MEASUREMENT,
                              BtlePacketBuilder::heartRate(255));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 255);
}

// ─────────────────────────────────────────────────────────────────────────────
// CSC tests
// ─────────────────────────────────────────────────────────────────────────────

void TstBtleHub::testCsc_firstMeasurementDiscarded()
{
    // First packet establishes the baseline; no signal should be emitted yet.
    QSignalSpy spyCad(hub, &BtleHub::signal_cadence);
    QSignalSpy spySpd(hub, &BtleHub::signal_speed);

    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT,
        BtlePacketBuilder::csc(100, 1024, 50, 512, true, true));

    QCOMPARE(spyCad.count(), 0);
    QCOMPARE(spySpd.count(), 0);
}

void TstBtleHub::testCsc_crankOnly()
{
    // First packet: baseline.  Second packet: exactly 1 crank revolution in
    // exactly 0.5 s (512 ticks at 1/1024 s/tick) → 120 RPM.
    QSignalSpy spy(hub, &BtleHub::signal_cadence);

    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT,
        BtlePacketBuilder::csc(0, 0, 10, 0, false, true));     // baseline
    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT,
        BtlePacketBuilder::csc(0, 0, 11, 512, false, true));   // +1 rev / 512 ticks

    QCOMPARE(spy.count(), 1);
    // 1 rev × 60 × 1024 / 512 ticks = 120 RPM
    QCOMPARE(spy.at(0).at(1).toInt(), 120);
}

void TstBtleHub::testCsc_wheelOnly()
{
    // 2100 mm wheel (default), 1 rev in 512 ticks (0.5 s) → speed
    // speed_ms = (2.1 m × 1 rev × 1024) / 512 = 4.2 m/s = 15.12 km/h
    hub->setWheelCircumferenceMm(2100);

    QSignalSpy spy(hub, &BtleHub::signal_speed);

    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT,
        BtlePacketBuilder::csc(0, 0, 0, 0, true, false));      // baseline
    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT,
        BtlePacketBuilder::csc(1, 512, 0, 0, true, false));    // +1 rev / 512 ticks

    QCOMPARE(spy.count(), 1);
    double speed = spy.at(0).at(1).toDouble();
    QVERIFY2(qAbs(speed - 15.12) < SPEED_TOLERANCE,
             qPrintable(QString("Expected ~15.12 km/h, got %1").arg(speed)));
}

void TstBtleHub::testCsc_combined()
{
    // Both crank and wheel data in the same packet.
    // 90 RPM cadence: 1 rev in 682 ticks (61440/682 = 90.08, truncated to 90 RPM)
    // 30 km/h speed with 2100 mm wheel:
    //   speed_ms = 30/3.6 = 8.333 m/s
    //   ticks_per_rev = 2100/1000 × 1024 / 8.333 = 258 ticks
    hub->setWheelCircumferenceMm(2100);

    QSignalSpy spyCad(hub, &BtleHub::signal_cadence);
    QSignalSpy spySpd(hub, &BtleHub::signal_speed);

    // Baseline
    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT,
        BtlePacketBuilder::csc(0, 0, 0, 0, true, true));
    // Second packet: +1 crank rev at 682 ticks, +1 wheel rev at 258 ticks
    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT,
        BtlePacketBuilder::csc(1, 258, 1, 682, true, true));

    QCOMPARE(spyCad.count(), 1);
    QCOMPARE(spySpd.count(), 1);

    double cadence = spyCad.at(0).at(1).toDouble();
    // 1 × 60 × 1024 / 682 = 90.08 → int 90 RPM
    QVERIFY2(qAbs(cadence - 90.0) <= CADENCE_TOLERANCE,
             qPrintable(QString("Expected ~90 RPM, got %1").arg(cadence)));

    double speed = spySpd.at(0).at(1).toDouble();
    // speed = 2.1 × 1 × 1024 / 258 × 3.6 ≈ 30.0 km/h
    QVERIFY2(qAbs(speed - 30.0) < SPEED_TOLERANCE,
             qPrintable(QString("Expected ~30 km/h, got %1").arg(speed)));
}

void TstBtleHub::testCsc_uint16Rollover()
{
    // Event time is uint16; values wrap around from 65535 to small positive.
    // Rollover-safe subtraction should still yield a correct result.
    // crankTime wraps: prev=65000, next=265 → delta = 65535 - 65000 + 265 + 1 = 801
    // 1 crank rev in 801 ticks → cadence = 1 × 60 × 1024 / 801 ≈ 76.7 RPM
    QSignalSpy spy(hub, &BtleHub::signal_cadence);

    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT,
        BtlePacketBuilder::csc(0, 0, 0, 65000, false, true));  // baseline
    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT,
        BtlePacketBuilder::csc(0, 0, 1, 265, false, true));    // 1 rev, wrapped time

    QCOMPARE(spy.count(), 1);
    int cadence = spy.at(0).at(1).toInt();
    // 1 × 60 × 1024 / (65535 - 65000 + 265 + 1) = 61440 / 801 ≈ 76
    QVERIFY2(qAbs(cadence - 76) <= 1,
             qPrintable(QString("Expected ~76 RPM after rollover, got %1").arg(cadence)));
}

void TstBtleHub::testCsc_standstill()
{
    // When the same crank values are reported twice, delta revs = 0 and
    // delta time = 0 (or very large), so cadence should be emitted as 0.
    QSignalSpy spy(hub, &BtleHub::signal_cadence);

    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT,
        BtlePacketBuilder::csc(0, 0, 5, 100, false, true));  // baseline
    // Same cumulative values → no new crank event
    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT,
        BtlePacketBuilder::csc(0, 0, 5, 100, false, true));  // standstill

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Cycling Power tests
// ─────────────────────────────────────────────────────────────────────────────

void TstBtleHub::testPower_positive()
{
    QSignalSpy spy(hub, &BtleHub::signal_power);
    hub->simulateNotification(BTLE_UUID_POWER_MEASUREMENT,
                              BtlePacketBuilder::cyclingPower(250));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 250);
}

void TstBtleHub::testPower_zero()
{
    QSignalSpy spy(hub, &BtleHub::signal_power);
    hub->simulateNotification(BTLE_UUID_POWER_MEASUREMENT,
                              BtlePacketBuilder::cyclingPower(0));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 0);
}

void TstBtleHub::testPower_negative()
{
    // Negative power is theoretically invalid but some implementations send -1
    // as a "no data" sentinel; the parser must not crash.
    QSignalSpy spy(hub, &BtleHub::signal_power);
    hub->simulateNotification(BTLE_UUID_POWER_MEASUREMENT,
                              BtlePacketBuilder::cyclingPower(-1));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), -1);
}

void TstBtleHub::testPower_tooShort_ignored()
{
    // A packet with fewer than 4 bytes must be silently discarded.
    QSignalSpy spy(hub, &BtleHub::signal_power);
    hub->simulateNotification(BTLE_UUID_POWER_MEASUREMENT,
                              QByteArray::fromHex("0000FF")); // only 3 bytes
    QCOMPARE(spy.count(), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// FTMS Indoor Bike Data tests
// ─────────────────────────────────────────────────────────────────────────────

void TstBtleHub::testFtms_speedOnly()
{
    QSignalSpy spySpd(hub, &BtleHub::signal_speed);
    QSignalSpy spyCad(hub, &BtleHub::signal_cadence);
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA,
        BtlePacketBuilder::ftmsIndoorBikeData(25.0, 0.0, 0,
            /*includeSpeed=*/true, /*includeCadence=*/false, /*includePower=*/false));

    QCOMPARE(spySpd.count(), 1);
    QVERIFY2(qAbs(spySpd.at(0).at(1).toDouble() - 25.0) < SPEED_TOLERANCE,
             "Speed mismatch");
    QCOMPARE(spyCad.count(), 0);
    QCOMPARE(spyPwr.count(), 0);
}

void TstBtleHub::testFtms_cadenceOnly()
{
    QSignalSpy spySpd(hub, &BtleHub::signal_speed);
    QSignalSpy spyCad(hub, &BtleHub::signal_cadence);
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA,
        BtlePacketBuilder::ftmsIndoorBikeData(0.0, 90.0, 0,
            /*includeSpeed=*/false, /*includeCadence=*/true, /*includePower=*/false));

    QCOMPARE(spySpd.count(), 0);
    QCOMPARE(spyCad.count(), 1);
    // 90 RPM encoded as quint16(90 / 0.5) = 180, decoded as int(180 * 0.5) = 90
    QCOMPARE(spyCad.at(0).at(1).toInt(), 90);
    QCOMPARE(spyPwr.count(), 0);
}

void TstBtleHub::testFtms_powerOnly()
{
    QSignalSpy spySpd(hub, &BtleHub::signal_speed);
    QSignalSpy spyCad(hub, &BtleHub::signal_cadence);
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA,
        BtlePacketBuilder::ftmsIndoorBikeData(0.0, 0.0, 300,
            /*includeSpeed=*/false, /*includeCadence=*/false, /*includePower=*/true));

    QCOMPARE(spySpd.count(), 0);
    QCOMPARE(spyCad.count(), 0);
    QCOMPARE(spyPwr.count(), 1);
    QCOMPARE(spyPwr.at(0).at(1).toInt(), 300);
}

void TstBtleHub::testFtms_allThree()
{
    QSignalSpy spySpd(hub, &BtleHub::signal_speed);
    QSignalSpy spyCad(hub, &BtleHub::signal_cadence);
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA,
        BtlePacketBuilder::ftmsIndoorBikeData(32.4, 88.0, 195));

    QCOMPARE(spySpd.count(), 1);
    QCOMPARE(spyCad.count(), 1);
    QCOMPARE(spyPwr.count(), 1);

    QVERIFY2(qAbs(spySpd.at(0).at(1).toDouble() - 32.4) < SPEED_TOLERANCE,
             "Speed mismatch");
    // 88.0 / 0.5 = 176, decode: int(176 * 0.5) = 88
    QCOMPARE(spyCad.at(0).at(1).toInt(), 88);
    QCOMPARE(spyPwr.at(0).at(1).toInt(), 195);
}

void TstBtleHub::testFtms_zeroValues()
{
    // Trainer at rest must emit zeros (not crash or skip).
    QSignalSpy spySpd(hub, &BtleHub::signal_speed);
    QSignalSpy spyCad(hub, &BtleHub::signal_cadence);
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA,
        BtlePacketBuilder::ftmsIndoorBikeData(0.0, 0.0, 0));

    QCOMPARE(spySpd.count(), 1);
    QCOMPARE(spyCad.count(), 1);
    QCOMPARE(spyPwr.count(), 1);
    QCOMPARE(spySpd.at(0).at(1).toDouble(), 0.0);
    QCOMPARE(spyCad.at(0).at(1).toInt(),    0);
    QCOMPARE(spyPwr.at(0).at(1).toInt(),    0);
}

void TstBtleHub::testFtms_tooShort_ignored()
{
    QSignalSpy spySpd(hub, &BtleHub::signal_speed);
    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA, QByteArray(1, '\x00'));
    QCOMPARE(spySpd.count(), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Trainer simulation tests
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Elite Direto / Suito – single FTMS packet at a realistic indoor workout value.
 * Cadence: 95 RPM, Speed: 34.5 km/h, Power: 230 W.
 */
void TstBtleHub::testEliteTrainer_singlePacket()
{
    QSignalSpy spySpd(hub, &BtleHub::signal_speed);
    QSignalSpy spyCad(hub, &BtleHub::signal_cadence);
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);

    EliteTrainerSim::Reading r{ 34.5, 95.0, 230 };
    hub->simulateNotification(EliteTrainerSim::characteristicUuid(),
                              EliteTrainerSim::buildPacket(r));

    QCOMPARE(spySpd.count(), 1);
    QCOMPARE(spyCad.count(), 1);
    QCOMPARE(spyPwr.count(), 1);
    QVERIFY2(qAbs(spySpd.at(0).at(1).toDouble() - 34.5) < SPEED_TOLERANCE,
             "Elite speed mismatch");
    QCOMPARE(spyCad.at(0).at(1).toInt(), 95);
    QCOMPARE(spyPwr.at(0).at(1).toInt(), 230);
}

/**
 * Elite trainer – a short sequence of increasing-power packets simulating
 * a ramp-up phase (think SFR intervals).
 */
void TstBtleHub::testEliteTrainer_sequence()
{
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);

    QList<qint16> powerValues = { 150, 175, 200, 225, 250 };
    for (qint16 w : powerValues) {
        EliteTrainerSim::Reading r{ 25.0 + w / 25.0, 85.0, w };
        hub->simulateNotification(EliteTrainerSim::characteristicUuid(),
                                  EliteTrainerSim::buildPacket(r));
    }

    QCOMPARE(spyPwr.count(), powerValues.size());
    for (int i = 0; i < powerValues.size(); ++i) {
        QCOMPARE(spyPwr.at(i).at(1).toInt(), static_cast<int>(powerValues[i]));
    }
}

/**
 * Wahoo KICKR Core – power via Cycling Power + cadence/speed via CSC.
 *
 * 90 RPM cadence: 1 crank rev in 1024/1.5 ≈ 683 ticks
 * 300 W output at 90 RPM
 * Speed: 2100 mm wheel at 90 RPM → ~28.3 km/h (gear dependent in reality,
 *         but the KICKR reports wheel revs directly from flywheel encoder)
 */
void TstBtleHub::testWahooKickr_powerAndCsc()
{
    hub->setWheelCircumferenceMm(2100);
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);
    QSignalSpy spyCad(hub, &BtleHub::signal_cadence);

    // Power packet
    hub->simulateNotification(WahooKickrSim::powerCharUuid(),
                              WahooKickrSim::buildPowerPacket(300));

    // CSC baseline then one interval packet (1 crank rev in 683 ticks)
    hub->simulateNotification(WahooKickrSim::cscCharUuid(),
                              WahooKickrSim::buildCscPacket(0, 0, 0, 0));
    hub->simulateNotification(WahooKickrSim::cscCharUuid(),
                              WahooKickrSim::buildCscPacket(1, 258, 1, 683));

    QCOMPARE(spyPwr.count(), 1);
    QCOMPARE(spyPwr.at(0).at(1).toInt(), 300);

    QCOMPARE(spyCad.count(), 1);
    int cadence = spyCad.at(0).at(1).toInt();
    // 1 × 60 × 1024 / 683 ≈ 89.9 → 89 or 90
    QVERIFY2(qAbs(cadence - 90) <= 1,
             qPrintable(QString("Expected ~90 RPM, got %1").arg(cadence)));
}

/**
 * Wahoo KICKR – verify uint16 event-time rollover is handled correctly.
 * This is a common failure point at high cadence (>120 RPM for long sessions).
 */
void TstBtleHub::testWahooKickr_cscRollover()
{
    QSignalSpy spy(hub, &BtleHub::signal_cadence);

    // Start near the uint16 boundary: prevTime = 65400
    hub->simulateNotification(WahooKickrSim::cscCharUuid(),
                              WahooKickrSim::buildCscPacket(0, 0, 0, 65400));
    // After rollover: nextTime = 200 (wrapped), 1 rev
    // deltaTime = (65535 - 65400) + 200 + 1 = 336 ticks
    // cadence = 1 × 60 × 1024 / 336 ≈ 182.9 RPM
    hub->simulateNotification(WahooKickrSim::cscCharUuid(),
                              WahooKickrSim::buildCscPacket(0, 0, 1, 200));

    QCOMPARE(spy.count(), 1);
    int cadence = spy.at(0).at(1).toInt();
    // Expected: ~182 RPM  (very high – simulating a sprint)
    QVERIFY2(cadence > 170 && cadence < 195,
             qPrintable(QString("Expected 170-195 RPM after rollover, got %1").arg(cadence)));
}

/**
 * Garmin Tacx Neo 2 – FTMS for speed + power, separate CSC for cadence.
 */
void TstBtleHub::testGarminTacx_ftmsPlusCsc()
{
    hub->setWheelCircumferenceMm(2100);
    QSignalSpy spySpd(hub, &BtleHub::signal_speed);
    QSignalSpy spyCad(hub, &BtleHub::signal_cadence);
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);

    // FTMS packet: 38.0 km/h, 280 W, no cadence from FTMS
    GarminTacxSim::FtmsReading r{ 38.0, 280 };
    hub->simulateNotification(GarminTacxSim::ftmsCharUuid(),
                              GarminTacxSim::buildFtmsPacket(r));

    // CSC crank-only: baseline + 1 rev in 512 ticks = 120 RPM
    hub->simulateNotification(GarminTacxSim::cscCharUuid(),
                              GarminTacxSim::buildCscCrankPacket(0, 0));
    hub->simulateNotification(GarminTacxSim::cscCharUuid(),
                              GarminTacxSim::buildCscCrankPacket(1, 512));

    QCOMPARE(spySpd.count(), 1);
    QVERIFY2(qAbs(spySpd.at(0).at(1).toDouble() - 38.0) < SPEED_TOLERANCE,
             "Tacx speed mismatch");

    QCOMPARE(spyPwr.count(), 1);
    QCOMPARE(spyPwr.at(0).at(1).toInt(), 280);

    QCOMPARE(spyCad.count(), 1);
    QCOMPARE(spyCad.at(0).at(1).toInt(), 120);
}

// ─────────────────────────────────────────────────────────────────────────────
// SimulatorHub verification tests
// ─────────────────────────────────────────────────────────────────────────────

void TstBtleHub::testSimulator_emitsHr()
{
    SimulatorHub sim;
    QSignalSpy spy(&sim, &SimulatorHub::signal_hr);
    sim.start();
    QVERIFY(spy.wait(2000));
    sim.stop();
    QVERIFY(spy.count() > 0);
}

void TstBtleHub::testSimulator_emitsCadence()
{
    SimulatorHub sim;
    QSignalSpy spy(&sim, &SimulatorHub::signal_cadence);
    sim.start();
    QVERIFY(spy.wait(2000));
    sim.stop();
    QVERIFY(spy.count() > 0);
}

void TstBtleHub::testSimulator_emitsSpeed()
{
    SimulatorHub sim;
    QSignalSpy spy(&sim, &SimulatorHub::signal_speed);
    sim.start();
    QVERIFY(spy.wait(2000));
    sim.stop();
    QVERIFY(spy.count() > 0);
}

void TstBtleHub::testSimulator_emitsPower()
{
    SimulatorHub sim;
    QSignalSpy spy(&sim, &SimulatorHub::signal_power);
    sim.start();
    QVERIFY(spy.wait(2000));
    sim.stop();
    QVERIFY(spy.count() > 0);
}

void TstBtleHub::testSimulator_valuesInRange()
{
    SimulatorHub sim;
    QSignalSpy spyHr(&sim,      &SimulatorHub::signal_hr);
    QSignalSpy spyCad(&sim,     &SimulatorHub::signal_cadence);
    QSignalSpy spySpd(&sim,     &SimulatorHub::signal_speed);
    QSignalSpy spyPwr(&sim,     &SimulatorHub::signal_power);

    sim.start();
    QTest::qWait(1500);
    sim.stop();

    QVERIFY(spyHr.count() > 0);
    int hr = spyHr.last().at(1).toInt();
    QVERIFY2(hr >= 110 && hr <= 180,
             qPrintable(QString("HR %1 out of range [110,180]").arg(hr)));

    QVERIFY(spyCad.count() > 0);
    int cad = spyCad.last().at(1).toInt();
    QVERIFY2(cad >= 70 && cad <= 110,
             qPrintable(QString("Cadence %1 out of range [70,110]").arg(cad)));

    QVERIFY(spySpd.count() > 0);
    double spd = spySpd.last().at(1).toDouble();
    QVERIFY2(spd >= 20.0 && spd <= 38.0,
             qPrintable(QString("Speed %1 out of range [20,38]").arg(spd)));

    QVERIFY(spyPwr.count() > 0);
    int pwr = spyPwr.last().at(1).toInt();
    QVERIFY2(pwr >= 150 && pwr <= 260,
             qPrintable(QString("Power %1 out of range [150,260]").arg(pwr)));
}

void TstBtleHub::testSimulator_stopStopsSignals()
{
    SimulatorHub sim;
    QSignalSpy spyHr(&sim, &SimulatorHub::signal_hr);

    sim.start();
    QVERIFY(spyHr.wait(2000));  // at least one signal received
    sim.stop();

    int countAfterStop = spyHr.count();
    QTest::qWait(500);          // wait a bit more – no new signals should arrive
    QCOMPARE(spyHr.count(), countAfterStop);
}

// ─────────────────────────────────────────────────────────────────────────────
// Short-packet rejection tests — no crash on truncated characteristic data
// ─────────────────────────────────────────────────────────────────────────────

/**
 * HR Measurement that is too short to hold even the flags byte should be
 * silently ignored without emitting any signal or crashing.
 */
void TstBtleHub::testHr_tooShort_ignored()
{
    QSignalSpy spy(hub, &BtleHub::signal_hr);
    hub->simulateNotification(BTLE_UUID_HR_MEASUREMENT, QByteArray()); // empty
    hub->simulateNotification(BTLE_UUID_HR_MEASUREMENT, QByteArray(1, 0x00)); // flags-only
    QCOMPARE(spy.count(), 0);
}

/**
 * CSC Measurement with too-short payload for the flags it claims to contain
 * should be silently ignored.
 */
void TstBtleHub::testCsc_tooShort_ignored()
{
    QSignalSpy spy(hub, &BtleHub::signal_cadence);
    // Flags byte claims crank data (bit1), but only 2 bytes follow instead of 5
    QByteArray shortCsc;
    shortCsc.append(char(0x02)); // flags: crank data present
    shortCsc.append(char(0xFF)); // partial crank revs (only 1 byte, needs 4)
    hub->simulateNotification(BTLE_UUID_CSC_MEASUREMENT, shortCsc);
    QCOMPARE(spy.count(), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// FTMS multi-field layout tests
// ─────────────────────────────────────────────────────────────────────────────

/**
 * FTMS Indoor Bike Data with all optional fields before "Instantaneous Power"
 * present: speed (bit0=0), average speed (bit1=1), cadence (bit2=0),
 * average cadence (bit3=1), total distance (bit4=1), resistance (bit5=1).
 * Verifies the parser correctly skips 2+2+2+2+3+2 = 13 bytes before reaching
 * the power field at offset 15 (2 flags + 13 skipped bytes).
 *
 * Raw packet layout (little-endian):
 *   [0-1]  flags  = 0x003A  (bits 1,3,4,5 set; bits 0,2,6 clear)
 *   [2-3]  speed  = 3800  (38.00 km/h)
 *   [4-5]  avgSpd = 3600
 *   [6-7]  cad    = 180   (90.0 rpm in 0.5 rpm units)
 *   [8-9]  avgCad = 160
 *   [10-12] totalDist = 0x0F4240 = 1 000 000 m (3-byte LE)
 *   [13-14] resistance = 50
 *   [15-16] power = 250 W
 */
void TstBtleHub::testFtms_allOptionalFieldsBeforePowerSkipped()
{
    QSignalSpy spySpd(hub, &BtleHub::signal_speed);
    QSignalSpy spyCad(hub, &BtleHub::signal_cadence);
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);

    // flags: bit0=0 (spd present), bit1=1 (avgSpd present), bit2=0 (cad present),
    //        bit3=1 (avgCad present), bit4=1 (dist present), bit5=1 (res present)
    // => 0b00111010 = 0x3A, high byte = 0x00
    QByteArray pkt;
    auto appendLE16 = [&](quint16 v) {
        pkt.append(char(v & 0xFF));
        pkt.append(char((v >> 8) & 0xFF));
    };
    appendLE16(0x003A);     // flags
    appendLE16(3800);       // speed: 38.00 km/h
    appendLE16(3600);       // average speed (skipped)
    appendLE16(180);        // cadence: 90.0 rpm
    appendLE16(160);        // average cadence (skipped)
    // total distance (3 bytes LE, skipped)
    pkt.append(char(0x40)); pkt.append(char(0x42)); pkt.append(char(0x0F));
    appendLE16(50);         // resistance level (skipped)
    appendLE16(250);        // instantaneous power: 250 W

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA, pkt);

    QCOMPARE(spySpd.count(), 1);
    QVERIFY2(qAbs(spySpd.at(0).at(1).toDouble() - 38.0) < SPEED_TOLERANCE,
             "FTMS speed with all-fields present mismatch");

    QCOMPARE(spyCad.count(), 1);
    QCOMPARE(spyCad.at(0).at(1).toInt(), 90);

    QCOMPARE(spyPwr.count(), 1);
    QCOMPARE(spyPwr.at(0).at(1).toInt(), 250);
}

/**
 * FTMS Indoor Bike Data with a negative Instantaneous Power value.
 * Negative power can occur on trainers with regenerative braking or
 * when the rider free-wheels against a strong tailwind in slope mode.
 * The field is a signed int16 — verify it is decoded as negative watts.
 */
void TstBtleHub::testFtms_negativeInstantPower()
{
    QSignalSpy spy(hub, &BtleHub::signal_power);

    // flags 0x0040 = bit6 set (power NOT present) → must clear bit6 for power
    // Use flags = 0x0000: speed absent flag (bit0=0 means speed IS present),
    // cadence absent (bit2=0 means cadence IS present), power bit6=0 (power present)
    // Build a packet with speed+cadence+power, power = -10 W (int16 LE 0xFFF6)
    QByteArray pkt;
    pkt.append(char(0x00)); pkt.append(char(0x00)); // flags: spd, cad, pwr present
    pkt.append(char(0x00)); pkt.append(char(0x00)); // speed = 0 km/h
    pkt.append(char(0x00)); pkt.append(char(0x00)); // cadence = 0 rpm
    pkt.append(char(0xF6)); pkt.append(char(0xFF)); // power = -10 (int16 LE)

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA, pkt);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), -10);
}

/**
 * A trainer that bridges an HR strap folds the bpm into the FTMS Indoor Bike
 * Data stream (bit 9) instead of exposing a separate 0x180D service. Verify the
 * HR field is decoded and re-emitted on signal_hr alongside the bike metrics.
 */
void TstBtleHub::testFtms_heartRate()
{
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);
    QSignalSpy spyHr(hub,  &BtleHub::signal_hr);

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA,
        BtlePacketBuilder::ftmsIndoorBikeData(32.4, 88.0, 195,
            /*includeSpeed=*/true, /*includeCadence=*/true, /*includePower=*/true,
            /*heartRateBpm=*/152));

    QCOMPARE(spyPwr.count(), 1);
    QCOMPARE(spyPwr.at(0).at(1).toInt(), 195);
    QCOMPARE(spyHr.count(), 1);
    QCOMPARE(spyHr.at(0).at(1).toInt(), 152);
}

/**
 * The Expended Energy field (bit 8, 5 bytes) precedes Heart Rate (bit 9). Verify
 * the parser skips it correctly so the HR byte is read at the right offset.
 */
void TstBtleHub::testFtms_heartRateAfterEnergy()
{
    QSignalSpy spyHr(hub, &BtleHub::signal_hr);

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA,
        BtlePacketBuilder::ftmsIndoorBikeData(0.0, 0.0, 0,
            /*includeSpeed=*/false, /*includeCadence=*/false, /*includePower=*/false,
            /*heartRateBpm=*/171, /*includeEnergy=*/true));

    QCOMPARE(spyHr.count(), 1);
    QCOMPARE(spyHr.at(0).at(1).toInt(), 171);
}

/**
 * Average Power (bit 7, 2 bytes) sits between Instantaneous Power and the energy/
 * HR fields. Verify the parser skips it so the HR byte lands at the right offset.
 * Built raw because the convenience builder doesn't expose an average-power field.
 */
void TstBtleHub::testFtms_heartRateAfterAvgPower()
{
    QSignalSpy spyPwr(hub, &BtleHub::signal_power);
    QSignalSpy spyHr(hub,  &BtleHub::signal_hr);

    // bits: 0 speed-absent, 2 cadence-absent, 6 instant-power-absent,
    //       7 avg-power present, 9 HR present  → 0x0001|0x0004|0x0040|0x0080|0x0200
    QByteArray pkt;
    quint16 flags = 0x02C5;
    pkt.append(char( flags       & 0xFF));
    pkt.append(char((flags >> 8) & 0xFF));
    pkt.append(char(0x00)); pkt.append(char(0x00)); // average power = 0 W
    pkt.append(char(168));                          // heart rate = 168 bpm

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA, pkt);

    QCOMPARE(spyPwr.count(), 0); // instantaneous power absent → no power signal
    QCOMPARE(spyHr.count(), 1);
    QCOMPARE(spyHr.at(0).at(1).toInt(), 168);
}

/**
 * HR flag set but the bpm byte is missing (truncated packet). The parser must not
 * read past the buffer or emit a bogus reading.
 */
void TstBtleHub::testFtms_heartRateFlagButTruncated_ignored()
{
    QSignalSpy spyHr(hub, &BtleHub::signal_hr);

    // Everything before HR absent, HR flagged present, but no HR byte appended.
    QByteArray pkt;
    quint16 flags = 0x0001 | 0x0004 | 0x0040 | 0x0200;
    pkt.append(char( flags       & 0xFF));
    pkt.append(char((flags >> 8) & 0xFF));

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA, pkt);

    QCOMPARE(spyHr.count(), 0);
}

/**
 * When the trainer bridges HR over FTMS, signal_trainerProvidesHr fires once per
 * connection (latched) so the UI can mark the HR slot "provided by trainer"
 * without re-firing on every packet.
 */
void TstBtleHub::testFtms_trainerProvidesHrSignal_firesOnce()
{
    QSignalSpy spy(hub, &BtleHub::signal_trainerProvidesHr);

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA,
        BtlePacketBuilder::ftmsIndoorBikeData(30.0, 85.0, 180,
            true, true, true, /*heartRateBpm=*/140));
    QCOMPARE(spy.count(), 1);

    // A second HR-bearing packet must not re-emit.
    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA,
        BtlePacketBuilder::ftmsIndoorBikeData(30.0, 85.0, 180,
            true, true, true, /*heartRateBpm=*/142));
    QCOMPARE(spy.count(), 1);
}

/** No HR field in the FTMS stream → the capability signal never fires. */
void TstBtleHub::testFtms_noHr_noTrainerProvidesHrSignal()
{
    QSignalSpy spy(hub, &BtleHub::signal_trainerProvidesHr);

    hub->simulateNotification(BTLE_UUID_FTMS_BIKE_DATA,
        BtlePacketBuilder::ftmsIndoorBikeData(30.0, 85.0, 180));

    QCOMPARE(spy.count(), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// SimulatorHub no-op slot tests
// ─────────────────────────────────────────────────────────────────────────────

/**
 * SimulatorHub::setLoad() and setSlope() are no-ops (the simulator ignores
 * ERG load commands). Verify they do not crash and do not emit any extra signal.
 */
void TstBtleHub::testSimulator_noOpSetLoad()
{
    SimulatorHub sim;
    QSignalSpy spyPwr(&sim, &SimulatorHub::signal_power);
    // setLoad must not crash even when the simulator is not running
    sim.setLoad(0, 250.0);
    sim.setLoad(1, 0.0);
    // No power signal should be emitted by a load command
    QTest::qWait(50);
    QCOMPARE(spyPwr.count(), 0);
}

void TstBtleHub::testSimulator_noOpSetSlope()
{
    SimulatorHub sim;
    QSignalSpy spySpd(&sim, &SimulatorHub::signal_speed);
    sim.setSlope(0, 5.0);
    sim.setSlope(0, -3.0);
    QTest::qWait(50);
    QCOMPARE(spySpd.count(), 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Battery Level parsing tests (issue #156)
// ─────────────────────────────────────────────────────────────────────────────

/// Empty payload — must be silently ignored, no signal emitted.
void TstBtleHub::testBattery_tooShort_ignored()
{
    QSignalSpy spy(hub, &BtleHub::signal_battery);
    hub->simulateNotification(BTLE_UUID_BATTERY_LEVEL, QByteArray());
    QCOMPARE(spy.count(), 0);
}

/// Battery level at 15% — signal emitted with correct percentage.
void TstBtleHub::testBattery_belowThreshold_emits()
{
    QSignalSpy spy(hub, &BtleHub::signal_battery);
    hub->simulateNotification(BTLE_UUID_BATTERY_LEVEL, QByteArray(1, char(15)));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 15);
}

/// At the threshold boundary (exactly threshold value) — signal emitted.
void TstBtleHub::testBattery_atThreshold_emits()
{
    QSignalSpy spy(hub, &BtleHub::signal_battery);
    hub->simulateNotification(BTLE_UUID_BATTERY_LEVEL, QByteArray(1, char(20)));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 20);
}

/// Above threshold (75%) — signal still emitted (threshold filtering is in WorkoutDialog).
void TstBtleHub::testBattery_aboveThreshold_emits()
{
    QSignalSpy spy(hub, &BtleHub::signal_battery);
    hub->simulateNotification(BTLE_UUID_BATTERY_LEVEL, QByteArray(1, char(75)));
    // BtleHub emits for any reading; WorkoutDialog applies the threshold
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 75);
}

/// Value > 100 must be clamped to 100.
void TstBtleHub::testBattery_clamped()
{
    QSignalSpy spy(hub, &BtleHub::signal_battery);
    hub->simulateNotification(BTLE_UUID_BATTERY_LEVEL, QByteArray(1, char(200)));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 100);
}

// ─────────────────────────────────────────────────────────────────────────────
// Interval summary — power-adherence classification
// ─────────────────────────────────────────────────────────────────────────────
using PA = IntervalSummaryUtil::PowerAdherence;

void TstBtleHub::testIntervalSummary_met_exactTarget()
{
    QCOMPARE(IntervalSummaryUtil::classifyPowerAdherence(200.0, 200.0), PA::Met);
}

void TstBtleHub::testIntervalSummary_met_upperBoundary()
{
    // exactly +5% → still Met
    QCOMPARE(IntervalSummaryUtil::classifyPowerAdherence(210.0, 200.0), PA::Met);
}

void TstBtleHub::testIntervalSummary_met_lowerBoundary()
{
    // exactly -5% → still Met
    QCOMPARE(IntervalSummaryUtil::classifyPowerAdherence(190.0, 200.0), PA::Met);
}

void TstBtleHub::testIntervalSummary_nearMiss_upperBoundary()
{
    // +7% → NearMiss
    QCOMPARE(IntervalSummaryUtil::classifyPowerAdherence(214.0, 200.0), PA::NearMiss);
}

void TstBtleHub::testIntervalSummary_nearMiss_lowerBoundary()
{
    // -7% → NearMiss
    QCOMPARE(IntervalSummaryUtil::classifyPowerAdherence(186.0, 200.0), PA::NearMiss);
}

void TstBtleHub::testIntervalSummary_missed_above()
{
    // +15% → Missed
    QCOMPARE(IntervalSummaryUtil::classifyPowerAdherence(230.0, 200.0), PA::Missed);
}

void TstBtleHub::testIntervalSummary_missed_below()
{
    // -15% → Missed
    QCOMPARE(IntervalSummaryUtil::classifyPowerAdherence(170.0, 200.0), PA::Missed);
}

void TstBtleHub::testIntervalSummary_zeroTarget_returnsMet()
{
    // no power target (e.g. HR-only or free-ride interval) → always Met
    QCOMPARE(IntervalSummaryUtil::classifyPowerAdherence(0.0, 0.0), PA::Met);
}

// ─────────────────────────────────────────────────────────────────────────────
QTEST_MAIN(TstBtleHub)
#include "tst_btle_hub.moc"
