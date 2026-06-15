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

QTEST_MAIN(TstZwiftProtocol)
#include "tst_zwift_protocol.moc"
