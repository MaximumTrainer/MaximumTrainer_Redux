// libFuzzer target: arbitrary GATT notification payloads through BtleHub's
// parsers. The first byte picks the characteristic, the rest is the payload.
#include <cstddef>
#include <cstdint>

#include <QByteArray>
#include <QCoreApplication>

#include "btle_hub.h"
#include "btle_uuids.h"

namespace {
const quint16 kCharacteristics[] = {
    BTLE_UUID_HR_MEASUREMENT,  BTLE_UUID_CSC_MEASUREMENT, BTLE_UUID_POWER_MEASUREMENT,
    BTLE_UUID_FTMS_BIKE_DATA,  BTLE_UUID_FTMS_ROWER_DATA, BTLE_UUID_FTMS_FEATURE,
    BTLE_UUID_BATTERY_LEVEL,   BTLE_UUID_MOXY_MEASUREMENT,
};

BtleHub &hub()
{
    static int argc = 1;
    static char name[] = "ble_notification_fuzz";
    static char *argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    static BtleHub instance;
    return instance;
}
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size == 0)
        return 0;
    const quint16 characteristic = kCharacteristics[data[0] % (sizeof(kCharacteristics) / sizeof(kCharacteristics[0]))];
    hub().simulateNotification(characteristic, QByteArray(reinterpret_cast<const char *>(data + 1), int(size - 1)));
    return 0;
}
