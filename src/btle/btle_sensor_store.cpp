#include "btle_sensor_store.h"

#include <QSettings>
#include <QObject>
#include <QBluetoothAddress>
#include <QBluetoothUuid>

QList<BtleSensorRole> BtleSensorStore::allRoles()
{
    // Trainer first: it is the primary device and, when paired, covers the
    // Power and Cadence/Speed slots listed directly beneath it.
    return {
        BtleSensorRole::Trainer,
        BtleSensorRole::Power,
        BtleSensorRole::CadenceSpeed,
        BtleSensorRole::HeartRate,
        BtleSensorRole::Oxygen
    };
}

bool BtleSensorStore::roleCoveredByTrainer(BtleSensorRole role)
{
    return role == BtleSensorRole::Power || role == BtleSensorRole::CadenceSpeed;
}

QString BtleSensorStore::roleKey(BtleSensorRole role)
{
    switch (role) {
    case BtleSensorRole::HeartRate:    return QStringLiteral("hr");
    case BtleSensorRole::Power:        return QStringLiteral("power");
    case BtleSensorRole::CadenceSpeed: return QStringLiteral("csc");
    case BtleSensorRole::Trainer:      return QStringLiteral("trainer");
    case BtleSensorRole::Oxygen:       return QStringLiteral("oxygen");
    }
    return QStringLiteral("unknown");
}

QString BtleSensorStore::roleDisplayName(BtleSensorRole role)
{
    switch (role) {
    case BtleSensorRole::HeartRate:    return QObject::tr("Heart Rate");
    case BtleSensorRole::Power:        return QObject::tr("Power");
    case BtleSensorRole::CadenceSpeed: return QObject::tr("Cadence / Speed");
    case BtleSensorRole::Trainer:      return QObject::tr("Trainer");
    case BtleSensorRole::Oxygen:       return QObject::tr("Oxygen");
    }
    return QString();
}

QList<QBluetoothUuid> BtleSensorStore::serviceUuidsForRole(BtleSensorRole role)
{
    // 16-bit assigned GATT service UUIDs. A smart trainer (FTMS, 0x1826) also
    // satisfies the Power and Cadence/Speed slots because many trainers report
    // those metrics over FTMS without advertising the dedicated services.
    constexpr quint16 HeartRate           = 0x180D;
    constexpr quint16 CyclingPower        = 0x1818;
    constexpr quint16 CyclingSpeedCadence = 0x1816;
    constexpr quint16 FitnessMachine      = 0x1826;
    constexpr quint16 MoxyOxygen          = 0xAAB0;

    switch (role) {
    case BtleSensorRole::HeartRate:
        return { QBluetoothUuid(HeartRate) };
    case BtleSensorRole::Power:
        return { QBluetoothUuid(CyclingPower), QBluetoothUuid(FitnessMachine) };
    case BtleSensorRole::CadenceSpeed:
        return { QBluetoothUuid(CyclingSpeedCadence), QBluetoothUuid(FitnessMachine) };
    case BtleSensorRole::Trainer:
        return { QBluetoothUuid(FitnessMachine) };
    case BtleSensorRole::Oxygen:
        return { QBluetoothUuid(MoxyOxygen) };
    }
    return {};
}

QString BtleSensorStore::groupForRider(int riderIndex)
{
    if (riderIndex <= 0)
        return QLatin1String(kGroup);
    return QLatin1String(kGroup) + QLatin1String("/rider") + QString::number(riderIndex);
}

QMap<BtleSensorRole, BtleSavedSensor> BtleSensorStore::loadAll(int riderIndex)
{
    QMap<BtleSensorRole, BtleSavedSensor> result;

    QSettings settings;
    settings.beginGroup(groupForRider(riderIndex));
    for (BtleSensorRole role : allRoles()) {
        const QString key = roleKey(role);

        BtleSavedSensor s;
        s.role       = role;
        s.name       = settings.value(key + QLatin1String("/name")).toString();
        s.address    = settings.value(key + QLatin1String("/address")).toString();
        s.deviceUuid = settings.value(key + QLatin1String("/deviceUuid")).toString();
        s.enabled    = settings.value(key + QLatin1String("/enabled"), true).toBool();

        if (s.isValid())
            result.insert(role, s);
    }
    settings.endGroup();

    return result;
}

void BtleSensorStore::saveSensor(const BtleSavedSensor &sensor, int riderIndex)
{
    const QString key = roleKey(sensor.role);

    QSettings settings;
    settings.beginGroup(groupForRider(riderIndex));
    settings.setValue(key + QLatin1String("/name"),       sensor.name);
    settings.setValue(key + QLatin1String("/address"),    sensor.address);
    settings.setValue(key + QLatin1String("/deviceUuid"), sensor.deviceUuid);
    settings.setValue(key + QLatin1String("/enabled"),    sensor.enabled);
    settings.endGroup();
}

void BtleSensorStore::clearSensor(BtleSensorRole role, int riderIndex)
{
    const QString key = roleKey(role);

    QSettings settings;
    settings.beginGroup(groupForRider(riderIndex));
    settings.remove(key);
    settings.endGroup();
}

BtleSavedSensor BtleSensorStore::fromDeviceInfo(BtleSensorRole role,
                                                const QBluetoothDeviceInfo &info)
{
    BtleSavedSensor s;
    s.role    = role;
    s.name    = info.name();
    s.enabled = true;

#ifdef Q_OS_MACOS
    // CoreBluetooth never exposes the hardware MAC; it gives a per-host UUID.
    s.deviceUuid = info.deviceUuid().toString();
#else
    s.address = info.address().toString();
#endif

    return s;
}

bool BtleSensorStore::matchesDiscovered(const BtleSavedSensor &saved,
                                        const QBluetoothDeviceInfo &discovered)
{
#ifdef Q_OS_MACOS
    return !saved.deviceUuid.isEmpty()
        && saved.deviceUuid == discovered.deviceUuid().toString();
#else
    return !saved.address.isEmpty()
        && saved.address == discovered.address().toString();
#endif
}
