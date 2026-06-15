/*
 * tst_zwift_protocol.cpp
 *
 * Qt Test suite for the Zwift virtual-shifting protocol codec (Phase 0).
 * Pure byte-level encode/decode — no hardware. Expected wire bytes are
 * hand-computed from the protobuf field numbers documented in
 * notes/zwift-virtual-shifting-plan.md.
 */

#include <QtTest/QtTest>

#include "zwift_protocol.h"
#include "virtual_gear.h"

using namespace ZwiftProtocol;

class TstZwiftProtocol : public QObject
{
    Q_OBJECT

private slots:
    // ── encodeControlCommand (0x04) ──────────────────────────────────────────
    void testEncode_gearRatioOnly();
    void testEncode_powerTargetOnly();
    void testEncode_negativeIncline_zigzag();
    void testEncode_empty();
    void testEncode_fieldOrdering();

    // ── decodeRidingData (0x03) ──────────────────────────────────────────────
    void testDecode_basicFields();
    void testDecode_wrongCommandByte();
    void testDecode_skipsUnknownLengthDelimited();
    void testDecode_truncatedVarint();

    // ── gear table ───────────────────────────────────────────────────────────
    void testGears_endpoints();
    void testGears_monotonic();
    void testGears_clamped();

    // ── misc ─────────────────────────────────────────────────────────────────
    void testHandshakePrefix();

    // ── virtual gear model (#293) ────────────────────────────────────────────
    void testGearWatts_monotonicInGear();
    void testGearWatts_risesWithCadence();
    void testGearWatts_cadenceFallbackAndClamp();
    void testGearWatts_ftpScaling();
    void testGearWatts_gearClamped();
};

void TstZwiftProtocol::testEncode_gearRatioOnly()
{
    HubCommand cmd;
    cmd.physical.gearRatioX10000 = 10000;   // ratio 1.0
    // [04] [2A 03] field5(msg,len3){ [10] field2=varint(10000=90 4E) }
    QCOMPARE(encodeControlCommand(cmd).toHex(), QByteArray("042a0310904e"));
}

void TstZwiftProtocol::testEncode_powerTargetOnly()
{
    HubCommand cmd;
    cmd.powerTargetW = 250;
    // [04] [18] field3=varint(250=FA 01)
    QCOMPARE(encodeControlCommand(cmd).toHex(), QByteArray("0418fa01"));
}

void TstZwiftProtocol::testEncode_negativeIncline_zigzag()
{
    HubCommand cmd;
    cmd.sim.inclineX100 = -150;             // -1.50 % grade; zigzag(-150) = 299
    // [04] [22 03] field4(msg,len3){ [10] field2=varint(299=AB 02) }
    QCOMPARE(encodeControlCommand(cmd).toHex(), QByteArray("04220310ab02"));
}

void TstZwiftProtocol::testEncode_empty()
{
    // No fields set — just the command byte, no protobuf body.
    HubCommand cmd;
    QCOMPARE(encodeControlCommand(cmd).toHex(), QByteArray("04"));
}

void TstZwiftProtocol::testEncode_fieldOrdering()
{
    // power (f3) then sim (f4) then physical (f5), in ascending field order.
    HubCommand cmd;
    cmd.powerTargetW             = 1;       // [18 01]
    cmd.sim.inclineX100          = 0;        // f4 msg, len 2: [22 02 10 00]
    cmd.physical.gearRatioX10000 = 1;        // f5 msg, len 2: [2A 02 10 01]
    QCOMPARE(encodeControlCommand(cmd).toHex(),
             QByteArray("04" "1801" "2202" "1000" "2a02" "1001"));
}

void TstZwiftProtocol::testDecode_basicFields()
{
    // 03 | f1=200 (C8 01) | f2=90 (5A) | f3=3050 (EA 17) | f4=145 (91 01)
    const QByteArray frame = QByteArray::fromHex("0308c801105a18ea17209101");
    RidingData d;
    QVERIFY(decodeRidingData(frame, d));
    QCOMPARE(d.power,     quint32(200));
    QCOMPARE(d.cadence,   quint32(90));
    QCOMPARE(d.speedX100, quint32(3050));
    QCOMPARE(d.hr,        quint32(145));
}

void TstZwiftProtocol::testDecode_wrongCommandByte()
{
    // A 0x04 (control) frame must not parse as riding data.
    const QByteArray frame = QByteArray::fromHex("0408c801");
    RidingData d;
    QVERIFY(!decodeRidingData(frame, d));
    QVERIFY(!decodeRidingData(QByteArray(), d));
}

void TstZwiftProtocol::testDecode_skipsUnknownLengthDelimited()
{
    // 03 | f1=100 (64) | f7 len-delimited 2 bytes (3A 02 AA BB) — skipped.
    const QByteArray frame = QByteArray::fromHex("03" "0864" "3a02aabb");
    RidingData d;
    QVERIFY(decodeRidingData(frame, d));
    QCOMPARE(d.power, quint32(100));
}

void TstZwiftProtocol::testDecode_truncatedVarint()
{
    // Continuation bit set on the final byte → incomplete varint → reject.
    const QByteArray frame = QByteArray::fromHex("0308ff");
    RidingData d;
    QVERIFY(!decodeRidingData(frame, d));
}

void TstZwiftProtocol::testGears_endpoints()
{
    QCOMPARE(ZwiftGears::ratioX10000ForGear(0), quint32(7500));
    QCOMPARE(ZwiftGears::ratioX10000ForGear(ZwiftGears::Count - 1), quint32(54900));
}

void TstZwiftProtocol::testGears_monotonic()
{
    for (int g = 1; g < ZwiftGears::Count; ++g)
        QVERIFY(ZwiftGears::ratioForGear(g) > ZwiftGears::ratioForGear(g - 1));
}

void TstZwiftProtocol::testGears_clamped()
{
    QCOMPARE(ZwiftGears::ratioForGear(-5),  ZwiftGears::ratioForGear(0));
    QCOMPARE(ZwiftGears::ratioForGear(999), ZwiftGears::ratioForGear(ZwiftGears::Count - 1));
}

void TstZwiftProtocol::testHandshakePrefix()
{
    QVERIFY(rideOnHandshake().startsWith(QByteArrayLiteral("RideOn")));
}

void TstZwiftProtocol::testGearWatts_monotonicInGear()
{
    // Harder gear ⇒ strictly more watts at a fixed cadence/FTP.
    for (int g = 2; g <= VirtualGear::Count; ++g)
        QVERIFY(VirtualGear::targetWatts(g, 85, 250)
                > VirtualGear::targetWatts(g - 1, 85, 250));
}

void TstZwiftProtocol::testGearWatts_risesWithCadence()
{
    // Same gear, faster pedaling ⇒ more watts (the "feels like a gear" property).
    QVERIFY(VirtualGear::targetWatts(8, 100, 250)
            > VirtualGear::targetWatts(8, 70, 250));
}

void TstZwiftProtocol::testGearWatts_cadenceFallbackAndClamp()
{
    // cadence<=0 falls back to the reference cadence (== explicit RefCadence).
    QCOMPARE(VirtualGear::targetWatts(8, 0,   250),
             VirtualGear::targetWatts(8, VirtualGear::RefCadence, 250));
    QCOMPARE(VirtualGear::targetWatts(8, -1,  250),
             VirtualGear::targetWatts(8, VirtualGear::RefCadence, 250));
    // Cadence is clamped to [40,120], so extremes saturate rather than explode.
    QCOMPARE(VirtualGear::targetWatts(8, 200, 250),
             VirtualGear::targetWatts(8, 120, 250));
    QCOMPARE(VirtualGear::targetWatts(8, 10,  250),
             VirtualGear::targetWatts(8, 40,  250));
}

void TstZwiftProtocol::testGearWatts_ftpScaling()
{
    // Higher FTP ⇒ higher targets; ftp<=0 uses the default.
    QVERIFY(VirtualGear::targetWatts(10, 85, 300)
            > VirtualGear::targetWatts(10, 85, 150));
    QCOMPARE(VirtualGear::targetWatts(10, 85, 0),
             VirtualGear::targetWatts(10, 85, VirtualGear::DefaultFtp));
}

void TstZwiftProtocol::testGearWatts_gearClamped()
{
    QCOMPARE(VirtualGear::targetWatts(0,   85, 250),
             VirtualGear::targetWatts(1,   85, 250));
    QCOMPARE(VirtualGear::targetWatts(999, 85, 250),
             VirtualGear::targetWatts(VirtualGear::Count, 85, 250));
}

QTEST_MAIN(TstZwiftProtocol)
#include "tst_zwift_protocol.moc"
