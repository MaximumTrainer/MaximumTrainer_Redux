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

// 128-bit UUIDs (full strings; the 32-bit prefix is the only part that varies).
namespace Uuid {
inline constexpr char Service[]      = "00000001-19ca-4651-86e5-fa29dcdd09d1";
inline constexpr char Measurement[]  = "00000002-19ca-4651-86e5-fa29dcdd09d1"; // notify
inline constexpr char ControlPoint[] = "00000003-19ca-4651-86e5-fa29dcdd09d1"; // write
inline constexpr char Response[]     = "00000004-19ca-4651-86e5-fa29dcdd09d1"; // indicate
} // namespace Uuid

// Frame command codes (first byte of every message). Scoped to avoid colliding
// with the RidingData struct below.
enum class Command : quint8 {
    RidingData     = 0x03,  // incoming notification (HubRidingData)
    TrainerControl = 0x04,  // outgoing to control point (HubCommand)
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

} // namespace ZwiftProtocol

// ── Virtual gear table ───────────────────────────────────────────────────────
// Zwift exposes ~24 virtual gears spanning ratios ~0.75 → 5.49 (Zwift Insider).
// Exact per-gear ratios are a product choice; default is a geometric spread so
// each shift is a constant percentage step. Confirm/retune against the device.
namespace ZwiftGears {
constexpr int    Count    = 24;
constexpr double MinRatio = 0.75;
constexpr double MaxRatio = 5.49;

double  ratioForGear(int gearIndex);        // clamped to [0, Count-1]
quint32 ratioX10000ForGear(int gearIndex);  // ratio * 10000, rounded
} // namespace ZwiftGears

#endif // ZWIFT_PROTOCOL_H
