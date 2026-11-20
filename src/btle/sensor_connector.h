#ifndef SENSOR_CONNECTOR_H
#define SENSOR_CONNECTOR_H

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QMap>
#include <QObject>
#include <QVector>

#include "btle_sensor_config.h"

class BtleHub;

/*
 * SensorConnector
 *
 * Finds and connects a rider's saved sensors: one shared BLE scan, one BtleHub
 * per physical device (two roles saved on the same device share it), and a
 * status per role. It has no UI; the pre-workout SensorConnectDialog and the
 * workout window's sensor strip both show its state.
 *
 * Hubs belong to the connector until takeHubs(); the destructor disconnects
 * and deletes whatever it still owns.
 */
class SensorConnector : public QObject
{
    Q_OBJECT

public:
    enum class Status { Searching, Connecting, Connected, NotFound, Lost };
    Q_ENUM(Status)

    struct Slot {
        BtleSensorRole  role;
        BtleSavedSensor saved;
        Status          status = Status::Searching;
        BtleHub        *hub    = nullptr;
    };

    /// Only valid, enabled sensors become slots; the FTMS slot follows
    /// \a rowingMode (Rower pairing when rowing, Trainer pairing otherwise).
    SensorConnector(const QMap<BtleSensorRole, BtleSavedSensor> &savedSensors, bool rowingMode,
                    QObject *parent = nullptr);
    ~SensorConnector() override;

    static QVector<Slot> slotsFor(const QMap<BtleSensorRole, BtleSavedSensor> &savedSensors, bool rowingMode);

    const QVector<Slot> &sensorSlots() const { return m_slots; }
    bool isEmpty() const { return m_slots.isEmpty(); }
    bool anyConnected() const;
    bool isScanning() const;

    /// Connected hubs keyed by role (two roles may share one hub).
    QMap<BtleSensorRole, BtleHub*> connectedHubs() const;

    /// Every hub created so far, connected or not.
    QList<BtleHub*> hubs() const { return m_hubByDevice.values(); }

    /// Hand the hubs to the caller: they are reparented off the connector and
    /// no longer deleted by it.
    void takeHubs();

public slots:
    void start();
    /// Reconnect a lost role, or scan again for one that was not found.
    void retry(BtleSensorRole role);
    void stopScan();

signals:
    void statusChanged(BtleSensorRole role, SensorConnector::Status status);
    /// A hub finished connecting (first time or after a reconnect).
    void hubConnected(BtleHub *hub);
    void scanFinished();

private:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onScanFinished();
    void setStatus(int slotIndex, Status status);
    void setStatusForHub(BtleHub *hub, Status status);
    BtleHub *hubForDevice(const QString &deviceKey, const QBluetoothDeviceInfo &info);

    QVector<Slot>                         m_slots;
    QBluetoothDeviceDiscoveryAgent       *m_agent = nullptr;
    QMap<QString, BtleHub*>               m_hubByDevice;
    QMap<QString, QBluetoothDeviceInfo>   m_infoByDevice;
    bool                                  m_hubsTaken = false;

    static constexpr int kScanTimeoutMs = 15000;
};

#endif // SENSOR_CONNECTOR_H
