#ifndef BTLE_DEVICE_SIMULATOR_H
#define BTLE_DEVICE_SIMULATOR_H

/*
 * BtleDeviceSimulator
 *
 * Header-only helpers that build correctly-formatted Bluetooth LE
 * characteristic notification payloads as different cycling-trainer brands
 * would produce them.  Used exclusively by the BtleHub test suite – no
 * production code includes this file.
 *
 * Supported brand/profile emulations
 * ────────────────────────────────────
 *  EliteTrainerSim   – Elite Direto / Suito series
 *                      Uses FTMS Indoor Bike Data (0x2AD2) for speed, cadence
 *                      and power.  Flags: speed present, cadence present, power
 *                      present (i.e. bits 0,2,6 = 0 in the FTMS "not-present"
 *                      convention).
 *
 *  WahooKickrSim     – Wahoo KICKR Core / KICKR v5+
 *                      Broadcasts Cycling Power (0x2A63) and a separate
 *                      CSC Measurement (0x2A5B) with both wheel and crank data.
 *
 *  GarminTacxSim     – Garmin Tacx Neo 2 / Flux 2
 *                      Broadcasts FTMS Indoor Bike Data (0x2AD2) *and*
 *                      CSC Measurement (0x2A5B) with crank-only data.
 *
 *  HeartRateSim      – Generic heart-rate strap (e.g. Garmin HRM-Pro)
 *                      Broadcasts Heart Rate Measurement (0x2A37) in both
 *                      8-bit and 16-bit formats.
 *
 * All packet formats follow the BT SIG assigned numbers / FTMS specification
 * (Bluetooth SIG, Fitness Machine Service 1.0).
 */

#include <QByteArray>
#include <QtGlobal>

#include "../../src/btle/btle_uuids.h"

// ─────────────────────────────────────────────────────────────────────────────
// Characteristic UUIDs – imported from btle_uuids.h for convenience
// ─────────────────────────────────────────────────────────────────────────────
// BTLE_UUID_HR_MEASUREMENT    = 0x2A37
// BTLE_UUID_CSC_MEASUREMENT   = 0x2A5B
// BTLE_UUID_POWER_MEASUREMENT = 0x2A63
// BTLE_UUID_FTMS_BIKE_DATA    = 0x2AD2

// ─────────────────────────────────────────────────────────────────────────────
// Low-level packet builders (brand-neutral)
// ─────────────────────────────────────────────────────────────────────────────

namespace BtlePacketBuilder {

/**
 * Heart Rate Measurement (0x2A37)
 * @param bpm       Heart rate in beats per minute
 * @param use16bit  true → encode bpm as uint16 (flag bit 0 = 1)
 *                  false → encode bpm as uint8  (flag bit 0 = 0)
 */
inline QByteArray heartRate(int bpm, bool use16bit = false)
{
    QByteArray pkt;
    quint8 flags = use16bit ? 0x01 : 0x00;
    pkt.append(static_cast<char>(flags));
    if (use16bit) {
        pkt.append(static_cast<char>(bpm & 0xFF));
        pkt.append(static_cast<char>((bpm >> 8) & 0xFF));
    } else {
        pkt.append(static_cast<char>(bpm & 0xFF));
    }
    return pkt;
}

/**
 * Heart Rate Measurement with RR-interval field appended (flag bit 4 = 1).
 * Tests that RR data is silently ignored and HR is still parsed correctly.
 */
inline QByteArray heartRateWithRR(int bpm, quint16 rrMs)
{
    QByteArray pkt;
    quint8 flags = 0x10; // bit4=RR present, bit0=uint8
    pkt.append(static_cast<char>(flags));
    pkt.append(static_cast<char>(bpm & 0xFF));
    // RR-interval in units of 1/1024 s
    quint16 rrRaw = static_cast<quint16>(rrMs * 1024 / 1000);
    pkt.append(static_cast<char>(rrRaw & 0xFF));
    pkt.append(static_cast<char>((rrRaw >> 8) & 0xFF));
    return pkt;
}

/**
 * CSC Measurement (0x2A5B).
 * @param wheelRevs  Cumulative wheel revolutions (uint32)
 * @param wheelTime  Last wheel event time  (uint16, 1/1024 s)
 * @param crankRevs  Cumulative crank revolutions (uint16)
 * @param crankTime  Last crank event time  (uint16, 1/1024 s)
 * @param hasWheel   Include wheel data (flag bit 0)
 * @param hasCrank   Include crank data (flag bit 1)
 */
inline QByteArray csc(quint32 wheelRevs, quint16 wheelTime,
                      quint16 crankRevs, quint16 crankTime,
                      bool hasWheel, bool hasCrank)
{
    QByteArray pkt;
    quint8 flags = 0;
    if (hasWheel) flags |= 0x01;
    if (hasCrank) flags |= 0x02;
    pkt.append(static_cast<char>(flags));

    if (hasWheel) {
        pkt.append(static_cast<char>( wheelRevs        & 0xFF));
        pkt.append(static_cast<char>((wheelRevs >>  8) & 0xFF));
        pkt.append(static_cast<char>((wheelRevs >> 16) & 0xFF));
        pkt.append(static_cast<char>((wheelRevs >> 24) & 0xFF));
        pkt.append(static_cast<char>( wheelTime        & 0xFF));
        pkt.append(static_cast<char>((wheelTime >>  8) & 0xFF));
    }
    if (hasCrank) {
        pkt.append(static_cast<char>( crankRevs        & 0xFF));
        pkt.append(static_cast<char>((crankRevs >>  8) & 0xFF));
        pkt.append(static_cast<char>( crankTime        & 0xFF));
        pkt.append(static_cast<char>((crankTime >>  8) & 0xFF));
    }
    return pkt;
}

/**
 * Cycling Power Measurement (0x2A63).
 * Minimal packet: 2-byte flags (all zero) + int16 instantaneous power.
 */
inline QByteArray cyclingPower(qint16 watts)
{
    QByteArray pkt;
    pkt.append('\x00'); // flags LSB
    pkt.append('\x00'); // flags MSB
    pkt.append(static_cast<char>( watts        & 0xFF));
    pkt.append(static_cast<char>((watts >>  8) & 0xFF));
    return pkt;
}

/**
 * FTMS Indoor Bike Data (0x2AD2).
 *
 * FTMS flag convention (inverted for speed/cadence/power!):
 *   bit 0 = 0 → Instantaneous Speed present
 *   bit 2 = 0 → Instantaneous Cadence present
 *   bit 6 = 0 → Instantaneous Power present
 *
 * @param speedKmh    Speed in km/h  (encoded as uint16 × 0.01 km/h)
 * @param cadenceRpm  Cadence in RPM (encoded as uint16 × 0.5 rpm)
 * @param powerW      Power in watts (encoded as int16)
 * @param includeSpeed   include speed field
 * @param includeCadence include cadence field
 * @param includePower   include power field
 */
inline QByteArray ftmsIndoorBikeData(double speedKmh,
                                     double cadenceRpm,
                                     qint16 powerW,
                                     bool includeSpeed   = true,
                                     bool includeCadence = true,
                                     bool includePower   = true)
{
    // Build flags: each "NOT present" bit is set when the field is absent.
    quint16 flags = 0;
    if (!includeSpeed)   flags |= 0x0001; // bit 0: "More Data" / speed absent
    if (!includeCadence) flags |= 0x0004; // bit 2: cadence absent
    if (!includePower)   flags |= 0x0040; // bit 6: power absent

    QByteArray pkt;
    pkt.append(static_cast<char>( flags        & 0xFF));
    pkt.append(static_cast<char>((flags >>  8) & 0xFF));

    if (includeSpeed) {
        quint16 rawSpeed = static_cast<quint16>(speedKmh / 0.01);
        pkt.append(static_cast<char>( rawSpeed        & 0xFF));
        pkt.append(static_cast<char>((rawSpeed >>  8) & 0xFF));
    }
    if (includeCadence) {
        quint16 rawCadence = static_cast<quint16>(cadenceRpm / 0.5);
        pkt.append(static_cast<char>( rawCadence        & 0xFF));
        pkt.append(static_cast<char>((rawCadence >>  8) & 0xFF));
    }
    if (includePower) {
        pkt.append(static_cast<char>( powerW        & 0xFF));
        pkt.append(static_cast<char>((powerW >>  8) & 0xFF));
    }
    return pkt;
}

} // namespace BtlePacketBuilder

// ─────────────────────────────────────────────────────────────────────────────
// Brand-specific simulator classes
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Elite Direto / Suito trainer simulator.
 *
 * Profile: FTMS Indoor Bike Data only.
 * Characteristic: 0x2AD2
 * Typical tick: 1 second
 *
 * Real Elite trainers broadcast speed (0.01 km/h res), cadence (0.5 rpm res)
 * and power (1 W res) via the single FTMS Indoor Bike Data characteristic.
 */
class EliteTrainerSim
{
public:
    struct Reading { double speedKmh; double cadenceRpm; qint16 powerW; };

    /** Build the FTMS Indoor Bike Data packet for a given reading. */
    static QByteArray buildPacket(const Reading &r)
    {
        return BtlePacketBuilder::ftmsIndoorBikeData(r.speedKmh, r.cadenceRpm, r.powerW);
    }

    static constexpr quint16 characteristicUuid() { return BTLE_UUID_FTMS_BIKE_DATA; }
};

/**
 * Wahoo KICKR Core / KICKR v5+ trainer simulator.
 *
 * Profile: Cycling Power (0x2A63) + CSC (0x2A5B).
 * The KICKR broadcasts power via the standard Cycling Power characteristic and
 * wheel/crank revolutions via CSC.  Speed is derived from wheel rev data.
 */
class WahooKickrSim
{
public:
    /** Build a Cycling Power measurement packet. */
    static QByteArray buildPowerPacket(qint16 watts)
    {
        return BtlePacketBuilder::cyclingPower(watts);
    }

    /**
     * Build a CSC measurement packet.
     * @param totalWheelRevs  Cumulative wheel revolutions (monotonically increasing)
     * @param wheelEventTime  1/1024-s timestamp of last wheel event (uint16, wraps)
     * @param totalCrankRevs  Cumulative crank revolutions (uint16, wraps)
     * @param crankEventTime  1/1024-s timestamp of last crank event (uint16, wraps)
     */
    static QByteArray buildCscPacket(quint32 totalWheelRevs, quint16 wheelEventTime,
                                     quint16 totalCrankRevs, quint16 crankEventTime)
    {
        return BtlePacketBuilder::csc(totalWheelRevs, wheelEventTime,
                                      totalCrankRevs, crankEventTime,
                                      /*hasWheel=*/true, /*hasCrank=*/true);
    }

    static constexpr quint16 powerCharUuid() { return BTLE_UUID_POWER_MEASUREMENT; }
    static constexpr quint16 cscCharUuid()   { return BTLE_UUID_CSC_MEASUREMENT; }
};

/**
 * Garmin Tacx Neo 2 / Flux 2 trainer simulator.
 *
 * Profile: FTMS Indoor Bike Data (0x2AD2) + CSC Measurement (0x2A5B, crank only).
 * The Tacx broadcasts speed/power via FTMS and cadence separately via CSC.
 */
class GarminTacxSim
{
public:
    struct FtmsReading { double speedKmh; qint16 powerW; };

    /** Build FTMS Indoor Bike Data packet (speed + power, no cadence). */
    static QByteArray buildFtmsPacket(const FtmsReading &r)
    {
        return BtlePacketBuilder::ftmsIndoorBikeData(r.speedKmh, 0.0, r.powerW,
                                                     /*includeSpeed=*/true,
                                                     /*includeCadence=*/false,
                                                     /*includePower=*/true);
    }

    /** Build CSC packet with crank-only data (no wheel revolutions). */
    static QByteArray buildCscCrankPacket(quint16 crankRevs, quint16 crankTime)
    {
        return BtlePacketBuilder::csc(0, 0, crankRevs, crankTime,
                                      /*hasWheel=*/false, /*hasCrank=*/true);
    }

    static constexpr quint16 ftmsCharUuid() { return BTLE_UUID_FTMS_BIKE_DATA; }
    static constexpr quint16 cscCharUuid()  { return BTLE_UUID_CSC_MEASUREMENT; }
};

/**
 * Generic Heart Rate Strap simulator (e.g. Garmin HRM-Pro / Polar H10).
 *
 * Profile: Heart Rate Service (0x180D), Heart Rate Measurement (0x2A37).
 */
class HeartRateSim
{
public:
    /** Build an 8-bit Heart Rate Measurement packet. */
    static QByteArray buildPacket(int bpm)
    {
        return BtlePacketBuilder::heartRate(bpm, /*use16bit=*/false);
    }

    /** Build a 16-bit Heart Rate Measurement packet (used for HR > 255 bpm). */
    static QByteArray buildPacket16(int bpm)
    {
        return BtlePacketBuilder::heartRate(bpm, /*use16bit=*/true);
    }

    static constexpr quint16 characteristicUuid() { return BTLE_UUID_HR_MEASUREMENT; }
};

#endif // BTLE_DEVICE_SIMULATOR_H
