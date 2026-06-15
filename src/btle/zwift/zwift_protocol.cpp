#include "zwift_protocol.h"

#include <cmath>

namespace {

// ── Minimal protobuf wire helpers (only what the two messages need) ──────────
void putVarint(QByteArray &out, quint64 value)
{
    do {
        quint8 byte = value & 0x7F;
        value >>= 7;
        if (value)
            byte |= 0x80;
        out.append(static_cast<char>(byte));
    } while (value);
}

quint64 zigzag32(qint32 value)
{
    return static_cast<quint32>((value << 1) ^ (value >> 31));
}

void putVarintField(QByteArray &out, int field, quint64 value)
{
    putVarint(out, (static_cast<quint64>(field) << 3) | 0);   // wire type 0
    putVarint(out, value);
}

void putMessageField(QByteArray &out, int field, const QByteArray &msg)
{
    putVarint(out, (static_cast<quint64>(field) << 3) | 2);   // wire type 2
    putVarint(out, static_cast<quint64>(msg.size()));
    out.append(msg);
}

// Reads a base-128 varint; advances pos. Returns false past the end or on a
// varint longer than 64 bits.
bool getVarint(const QByteArray &data, int &pos, quint64 &out)
{
    out = 0;
    int shift = 0;
    while (pos < data.size()) {
        const quint8 byte = static_cast<quint8>(data.at(pos++));
        out |= (static_cast<quint64>(byte & 0x7F) << shift);
        if (!(byte & 0x80))
            return true;
        shift += 7;
        if (shift >= 64)
            return false;
    }
    return false;
}

} // namespace

namespace ZwiftProtocol {

QByteArray rideOnHandshake()
{
    return QByteArrayLiteral("RideOn");
}

QByteArray encodeControlCommand(const HubCommand &cmd)
{
    QByteArray body;

    if (cmd.powerTargetW)
        putVarintField(body, 3, *cmd.powerTargetW);

    if (cmd.sim.any()) {
        QByteArray sim;
        if (cmd.sim.windX100)    putVarintField(sim, 1, zigzag32(*cmd.sim.windX100));
        if (cmd.sim.inclineX100) putVarintField(sim, 2, zigzag32(*cmd.sim.inclineX100));
        if (cmd.sim.cwaX10000)   putVarintField(sim, 3, *cmd.sim.cwaX10000);
        if (cmd.sim.crrX100000)  putVarintField(sim, 4, *cmd.sim.crrX100000);
        putMessageField(body, 4, sim);
    }

    if (cmd.physical.any()) {
        QByteArray phys;
        if (cmd.physical.gearRatioX10000) putVarintField(phys, 2, *cmd.physical.gearRatioX10000);
        if (cmd.physical.bikeWeightX100)  putVarintField(phys, 4, *cmd.physical.bikeWeightX100);
        if (cmd.physical.riderWeightX100) putVarintField(phys, 5, *cmd.physical.riderWeightX100);
        putMessageField(body, 5, phys);
    }

    QByteArray frame;
    frame.append(static_cast<char>(Command::TrainerControl));
    frame.append(body);
    return frame;
}

bool decodeRidingData(const QByteArray &frame, RidingData &out)
{
    if (frame.isEmpty() ||
        static_cast<quint8>(frame.at(0)) != static_cast<quint8>(Command::RidingData))
        return false;

    int pos = 1;
    while (pos < frame.size()) {
        quint64 tag;
        if (!getVarint(frame, pos, tag))
            return false;
        const int field    = static_cast<int>(tag >> 3);
        const int wireType = static_cast<int>(tag & 0x7);

        switch (wireType) {
        case 0: {                       // varint
            quint64 value;
            if (!getVarint(frame, pos, value))
                return false;
            switch (field) {
            case 1: out.power     = static_cast<quint32>(value); break;
            case 2: out.cadence   = static_cast<quint32>(value); break;
            case 3: out.speedX100 = static_cast<quint32>(value); break;
            case 4: out.hr        = static_cast<quint32>(value); break;
            case 5: out.unknown1  = static_cast<quint32>(value); break;
            case 6: out.unknown2  = static_cast<quint32>(value); break;
            default: break;             // unknown field — ignore value
            }
            break;
        }
        case 2: {                       // length-delimited — skip
            quint64 len;
            if (!getVarint(frame, pos, len))
                return false;
            pos += static_cast<int>(len);
            break;
        }
        case 5: pos += 4; break;        // 32-bit
        case 1: pos += 8; break;        // 64-bit
        default: return false;          // unsupported wire type
        }
        if (pos > frame.size())
            return false;
    }
    return true;
}

} // namespace ZwiftProtocol

namespace ZwiftGears {

double ratioForGear(int gearIndex)
{
    const int g = qBound(0, gearIndex, Count - 1);
    const double t = static_cast<double>(g) / static_cast<double>(Count - 1);
    return MinRatio * std::pow(MaxRatio / MinRatio, t);
}

quint32 ratioX10000ForGear(int gearIndex)
{
    return static_cast<quint32>(std::lround(ratioForGear(gearIndex) * 10000.0));
}

} // namespace ZwiftGears
