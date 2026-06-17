#ifndef ZWIFT_CLICK_PROTOCOL_H
#define ZWIFT_CLICK_PROTOCOL_H

#include <QByteArray>
#include <QtGlobal>

/*
 * Zwift Click / Play / Ride *controller* input codec — buttons only.
 *
 * This is the INPUT side of Zwift's proprietary BLE protocol and is deliberately
 * scoped to reading button presses from a controller connected as its OWN BLE
 * peripheral. It does NOT control a trainer: there is no FC82 trainer relay and
 * no gear/ERG/SIM command here (those were removed — the app drives resistance
 * over standard FTMS). Reading the controller this way never touches the
 * trainer connection, so ERG/FTMS stays live.
 *
 * On the firmware tested (Click/Play fw 1.2.0) the link is UNENCRYPTED: write
 * "RideOn" to the control point and the device streams a 0x23 button-status
 * frame (field 1 = a 32-bit active-low bitmap) on the measurement char ~2×/s.
 * Provenance: Makinolo, "Zwift Ride protocol" (2024-07-26); confirmed on
 * hardware. See notes/zwift-cog-protocol-findings.md.
 */
namespace ZwiftClickProtocol {

// The controller advertises the 16-bit Zwift service 0xFC82 (Jan-2025 firmware;
// older units used a 128-bit UUID). The three characteristics use the 19ca-…
// base documented by Makinolo.
namespace Uuid {
inline constexpr char Service[]      = "0000fc82-0000-1000-8000-00805f9b34fb";
inline constexpr char Measurement[]  = "00000002-19ca-4651-86e5-fa29dcdd09d1"; // notify
inline constexpr char ControlPoint[] = "00000003-19ca-4651-86e5-fa29dcdd09d1"; // write
inline constexpr char Response[]     = "00000004-19ca-4651-86e5-fa29dcdd09d1"; // indicate
} // namespace Uuid

// Button-status frame command code (first byte of the notify payload).
inline constexpr quint8 ButtonStatusCommand = 0x23;

// The handshake the app writes once subscribed; "RideOn" kicks the stream.
QByteArray rideOnHandshake();

// Decode a 0x23 button frame into the raw active-low bitmap. Returns false if
// the command byte is not 0x23 or the protobuf stream is malformed.
bool decodeClickButtons(const QByteArray &frame, quint32 &bitmapOut);

// A button is "pressed" when its bit is 0 (active-low).
inline bool clickButtonPressed(quint32 bitmap, int bitIndex) {
    return (bitmap & (1u << bitIndex)) == 0;
}

} // namespace ZwiftClickProtocol

// ── Zwift Click / Play shifter button bits ───────────────────────────────────
// Each button occupies a FIXED bit of the 0x23 active-low bitmap, confirmed on
// hardware (fw 1.2.0). On a Ride pair all presses tunnel through the left unit
// over one connection; older Play pairs may need both units (each mirrors the
// other's shift bit), deduped per bit.
namespace ZwiftClick {
constexpr int UpShiftBit   = 12;  // right paddle → shift up   (+1)
constexpr int DownShiftBit = 8;   // left  paddle → shift down (-1)
// Left controller d-pad.
constexpr int DpadLeftBit  = 0;
constexpr int DpadUpBit    = 1;
constexpr int DpadRightBit = 2;
constexpr int DpadDownBit  = 3;
// Right controller action buttons.
constexpr int ButtonABit   = 4;
constexpr int ButtonBBit   = 5;
constexpr int ButtonYBit   = 6;
constexpr int ButtonZBit   = 7;
} // namespace ZwiftClick

#endif // ZWIFT_CLICK_PROTOCOL_H
