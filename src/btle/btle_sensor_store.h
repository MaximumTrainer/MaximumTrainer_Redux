#ifndef BTLE_SENSOR_STORE_H
#define BTLE_SENSOR_STORE_H

#include <QMap>
#include <QList>
#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>

#include "btle_sensor_config.h"

/*
 * BtleSensorStore
 *
 * Persists the per-role BLE sensor selections to QSettings (group "btleSensors")
 * and reconstructs them on load. Also bridges between a discovered
 * QBluetoothDeviceInfo and the stored representation, handling the macOS vs
 * Linux/Windows identifier difference in one place.
 *
 * Stateless – all methods are static.
 */
class BtleSensorStore
{
public:
    /// Read every role slot. Roles with no saved device are omitted from the map.
    /// \a riderIndex selects the sensor package: 0 (default) is the solo set;
    /// 1..N are the per-rider Studio-mode sets, stored in separate sub-groups.
    static QMap<BtleSensorRole, BtleSavedSensor> loadAll(int riderIndex = 0);

    /// Persist a single role slot (see \a riderIndex on loadAll()).
    static void saveSensor(const BtleSavedSensor &sensor, int riderIndex = 0);

    /// Remove the saved device for a role (see \a riderIndex on loadAll()).
    static void clearSensor(BtleSensorRole role, int riderIndex = 0);

    /// Build a BtleSavedSensor from a discovered device, populating the correct
    /// platform identifier (deviceUuid on macOS, address otherwise).
    static BtleSavedSensor fromDeviceInfo(BtleSensorRole role,
                                          const QBluetoothDeviceInfo &info);

    /// True when \a discovered is the device previously saved as \a saved.
    static bool matchesDiscovered(const BtleSavedSensor &saved,
                                  const QBluetoothDeviceInfo &discovered);

    /// Stable QSettings sub-key for a role ("hr", "power", "csc", ...).
    static QString roleKey(BtleSensorRole role);

    /// Human-readable role name for UI ("Heart Rate", "Power", ...).
    static QString roleDisplayName(BtleSensorRole role);

    /// GATT service UUIDs a device must advertise to be relevant for \a role.
    /// Used by the scanner to filter the discovery list down to devices of the
    /// matching type (e.g. only heart-rate monitors when scanning the HR slot).
    /// An empty list means "no filter / accept anything".
    static QList<QBluetoothUuid> serviceUuidsForRole(BtleSensorRole role);

    /// All roles in display order. Single source of truth for iteration.
    static QList<BtleSensorRole> allRoles();

private:
    static constexpr const char *kGroup = "btleSensors";

    /// QSettings group for a sensor package. riderIndex <= 0 is the solo set
    /// ("btleSensors"); 1..N are Studio riders ("btleSensors/riderN").
    static QString groupForRider(int riderIndex);
};

#endif // BTLE_SENSOR_STORE_H
