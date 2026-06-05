#ifndef BTLE_SENSOR_CONFIG_H
#define BTLE_SENSOR_CONFIG_H

#include <QString>

/*
 * BtleSensorRole
 *
 * One role per "slot" in the Sensors preferences page. Each role can have a
 * single BLE device saved against it. A physical smart trainer may legitimately
 * be saved under both Trainer (resistance control) and Power; the connect logic
 * deduplicates those onto one BtleHub.
 */
enum class BtleSensorRole {
    HeartRate,    // Heart Rate service (0x180D)
    Power,        // Cycling Power service (0x1818)
    CadenceSpeed, // Cycling Speed & Cadence service (0x1816) or FTMS cadence/speed
    Trainer,      // Fitness Machine / FTMS (0x1826) – resistance control
    Oxygen        // Moxy Muscle Oxygen service (0xAAB0)
};

/*
 * BtleSavedSensor
 *
 * A device the user has chosen for a given role, persisted across sessions.
 *
 * Both address and deviceUuid are kept because the connectable identifier is
 * platform-dependent: Linux/Windows expose a stable MAC address, while macOS
 * (CoreBluetooth) hides it and only offers a per-host peripheral UUID. Exactly
 * one of the two is populated when a device is saved (see BtleSensorStore).
 */
struct BtleSavedSensor {
    BtleSensorRole role        = BtleSensorRole::HeartRate;
    QString        name;        // human-readable device name
    QString        address;     // MAC address (Linux/Windows), empty on macOS
    QString        deviceUuid;  // CoreBluetooth peripheral UUID (macOS), empty elsewhere
    bool           enabled = true; // when false, the slot is skipped at connect time

    bool isValid() const {
        return !name.isEmpty() && (!address.isEmpty() || !deviceUuid.isEmpty());
    }
};

#endif // BTLE_SENSOR_CONFIG_H
