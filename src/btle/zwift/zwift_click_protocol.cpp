#include "zwift_click_protocol.h"

namespace {
// Minimal protobuf varint reader. Advances pos; returns false on truncation.
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

namespace ZwiftClickProtocol {

QByteArray rideOnHandshake()
{
    return QByteArrayLiteral("RideOn");
}

bool decodeClickButtons(const QByteArray &frame, quint32 &bitmapOut)
{
    if (frame.isEmpty() ||
        static_cast<quint8>(frame.at(0)) != ButtonStatusCommand)
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
            if (field == 1)             // the button bitmap
                bitmapOut = static_cast<quint32>(value);
            break;
        }
        case 2: {                       // length-delimited (e.g. analog stick) — skip
            quint64 len;
            if (!getVarint(frame, pos, len))
                return false;
            pos += static_cast<int>(len);
            break;
        }
        case 5: pos += 4; break;        // 32-bit
        case 1: pos += 8; break;        // 64-bit
        default: return false;
        }
        if (pos > frame.size())
            return false;
    }
    return true;
}

} // namespace ZwiftClickProtocol
