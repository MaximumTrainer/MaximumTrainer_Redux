#ifndef ZWIFT_PROTOCOL_H
#define ZWIFT_PROTOCOL_H

#include <QByteArray>
#include <QtGlobal>
#include <optional>

/*
 * Zwift proprietary *trainer* protocol — virtual shifting (issue #293).
 *
 * This is NOT FTMS. Virtual-shifting trainers (e.g. JetBlack Victory + Zwift
 * Cog) expose a Zwift-custom GATT service; we send a gear ratio and the trainer
 * firmware turns it into resistance. Frame layout is [command_code][protobuf].
 *
 * Field numbers/types below are from public reverse-engineering (Makinolo,
 * "Zwift Trainer protocol", 2024-10-20). The link is unencrypted, like the
 * Zwift Ride. See notes/zwift-virtual-shifting-plan.md for provenance.
 *
 * Phase 0 is the pure codec only — no QtBluetooth, no device I/O — so it is
 * unit-testable against byte fixtures with just Qt Core + Test.
 */
namespace ZwiftProtocol {

// UUIDs. The container service is advertised as the 16-bit Zwift UUID 0xFC82
// (confirmed on a JetBlack Victory fw 4.29 via the Phase 1 probe); the three
// characteristics use the 19ca-… base documented by Makinolo. Earlier drafts
// wrongly assumed the service itself was 00000001-19ca-…; it is not.
namespace Uuid {
inline constexpr char Service[]      = "0000fc82-0000-1000-8000-00805f9b34fb";
inline constexpr char Measurement[]  = "00000002-19ca-4651-86e5-fa29dcdd09d1"; // notify
inline constexpr char ControlPoint[] = "00000003-19ca-4651-86e5-fa29dcdd09d1"; // write
inline constexpr char Response[]     = "00000004-19ca-4651-86e5-fa29dcdd09d1"; // indicate
} // namespace Uuid

// Frame command codes (first byte of every message). Scoped to avoid colliding
// with the RidingData struct below.
enum class Command : quint8 {
    RidingData     = 0x03,  // incoming notification (HubRidingData)
    TrainerControl = 0x04,  // outgoing to control point (HubCommand)
    ButtonStatus   = 0x23,  // incoming Click button bitmap (notify on 0x0002)
};

// The handshake the app writes once subscribed. Trailing bytes (if any) are
// confirmed against real hardware in Phase 1; "RideOn" is the documented prefix.
QByteArray rideOnHandshake();

// ── Outgoing: HubCommand (command 0x04) ──────────────────────────────────────
struct SimulationParam {
    std::optional<qint32>  windX100;      // f1 sint32 — wind speed, m/s * 100
    std::optional<qint32>  inclineX100;   // f2 sint32 — road grade % * 100
    std::optional<quint32> cwaX10000;     // f3        — CW * a * 10000
    std::optional<quint32> crrX100000;    // f4        — Crr * 100000
    bool any() const {
        return windX100 || inclineX100 || cwaX10000 || crrX100000;
    }
};

struct PhysicalParam {
    std::optional<quint32> gearRatioX10000; // f2 — virtual gear ratio * 10000
    std::optional<quint32> bikeWeightX100;  // f4 — kg * 100
    std::optional<quint32> riderWeightX100; // f5 — kg * 100
    bool any() const {
        return gearRatioX10000 || bikeWeightX100 || riderWeightX100;
    }
};

struct HubCommand {
    std::optional<quint32> powerTargetW;  // f3 — ERG target watts
    SimulationParam        sim;           // f4
    PhysicalParam          physical;      // f5
};

// Encode a HubCommand to wire bytes, including the leading 0x04 command code.
QByteArray encodeControlCommand(const HubCommand &cmd);

// ── Incoming: HubRidingData (command 0x03) ───────────────────────────────────
struct RidingData {
    quint32 power      = 0;  // f1 watts
    quint32 cadence    = 0;  // f2 rpm
    quint32 speedX100  = 0;  // f3 km/h * 100 (assumed scale; confirm in Phase 1)
    quint32 hr         = 0;  // f4 bpm
    quint32 unknown1   = 0;  // f5
    quint32 unknown2   = 0;  // f6
};

// Decode a 0x03 riding-data frame. Returns false if the command byte is not
// 0x03 or the protobuf stream is malformed. Unknown fields are skipped.
bool decodeRidingData(const QByteArray &frame, RidingData &out);

// ── Incoming: Zwift Click button status (command 0x23) ───────────────────────
// The Click (fw 1.2.0) streams an UNENCRYPTED status frame ~2×/s on the 0x0002
// measurement char: [0x23][field 1 = varint], where field 1 is a 32-bit
// ACTIVE-LOW button bitmap — idle is 0xFFFFFFFF and a held button clears its
// bit (e.g. 0xFFFFEFFF = bit 12 down). Confirmed via --zwift-probe on hardware.
// Decode a 0x23 frame into the raw bitmap. Returns false if the command byte is
// not 0x23 or the stream is malformed. Map specific bits to +/- with the helper
// below once the per-button bits are nailed down from a hardware capture.
bool decodeClickButtons(const QByteArray &frame, quint32 &bitmapOut);

// Convenience: a button is "pressed" when its bit is 0 (active-low).
inline bool clickButtonPressed(quint32 bitmap, int bitIndex) {
    return (bitmap & (1u << bitIndex)) == 0;
}

// ── Trainer-relayed Click button frame (the real Zwift path) ─────────────────
// A Zwift-protocol trainer (0xFC82) RELAYS a linked Click's frames to the app,
// wrapped as: [0x4e][protobuf{ field2 = <inner Click frame> }]. When the inner
// frame is a 0x23 button frame, extract its active-low bitmap. Returns false if
// this isn't a 0x4e relay carrying a button frame. (Captured from a live Zwift
// session — see notes/zwift-cog-protocol-findings.md.)
bool decodeRelayedClickButtons(const QByteArray &frame, quint32 &bitmapOut);

// The full RideOn handshake Zwift writes to the trainer's FC82 control point to
// start the relay stream: "RideOn" + 0x02 0x03 (bare "RideOn" alone is what the
// trainer's own riding-data needs; the +0203 trailer matches the captured app).
QByteArray rideOnTrainerHandshake();

} // namespace ZwiftProtocol

// ── Virtual gear table ───────────────────────────────────────────────────────
// Zwift exposes ~24 virtual gears spanning ratios ~0.75 → 5.49 (Zwift Insider).
// Exact per-gear ratios are a product choice; default is a geometric spread so
// each shift is a constant percentage step. Confirm/retune against the device.
// Fixed environment coefficients Zwift sends in every SimulationParam. Without
// these, virtual shifting produces no resistance on flat ground: the gear ratio
// drives virtual speed, and the *feel* comes from aero drag (CWa) acting on that
// speed. Confirmed on a JetBlack Victory — sending these is what makes gears
// bite. (See notes/zwift-virtual-shifting-plan.md.)
namespace ZwiftSim {
constexpr quint32 CWaX10000  = 5100;   // CW·a·10000  → 0.51
constexpr quint32 CrrX100000 = 400;    // Crr·100000  → 0.004
} // namespace ZwiftSim

// ── Zwift Click / Play shifter button bits ───────────────────────────────────
// The +/- shifters are two separate single-paddle BLE devices (or the two
// paddles of a Play pair). Each reports its shift paddle on a FIXED bit of the
// 0x23 active-low bitmap, confirmed on hardware (fw 1.2.0): right/up paddle =
// bit 12, left/down paddle = bit 8. Other buttons (d-pad, Y/Z/A/B) occupy other
// bits — surfaced raw for later mapping (radio/volume/pause/etc.).
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

namespace ZwiftGears {
constexpr int    Count    = 24;
constexpr double MinRatio = 0.75;
constexpr double MaxRatio = 5.49;

double  ratioForGear(int gearIndex);        // clamped to [0, Count-1]
quint32 ratioX10000ForGear(int gearIndex);  // ratio * 10000, rounded
} // namespace ZwiftGears

#endif // ZWIFT_PROTOCOL_H
