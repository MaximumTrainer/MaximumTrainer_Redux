#ifndef BTLE_SCANNER_DIALOG_H
#define BTLE_SCANNER_DIALOG_H

#include <QDialog>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QStandardItemModel>

#include "btle_sensor_config.h"

namespace Ui {
class BtleScannerDialog;
}

/*
 * BtleScannerDialog
 *
 * Presents a list of nearby BLE devices.  The user selects one and presses
 * "Connect".  The selected QBluetoothDeviceInfo is then available via
 * selectedDevice().
 *
 * When constructed with a sensor role, the list is filtered to devices that
 * advertise the matching GATT service (e.g. only heart-rate monitors for the
 * HeartRate role) so the user isn't wading through every nearby BLE device. A
 * "Show all devices" toggle disables the filter as an escape hatch for sensors
 * that don't advertise their service in the discovery packet.
 */
class BtleScannerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BtleScannerDialog(QWidget *parent = nullptr);
    explicit BtleScannerDialog(BtleSensorRole role, QWidget *parent = nullptr);
    ~BtleScannerDialog();

    QBluetoothDeviceInfo selectedDevice() const { return m_selectedDevice; }
    bool hasSelection() const { return m_selectedDevice.isValid(); }

private slots:
    void startScan();
    void stopScan();
    void onDeviceDiscovered(const QBluetoothDeviceInfo &device);
    void onScanFinished();
    void onScanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void onSelectionChanged();
    void onConnectClicked();

private:
    void init();
    void addOrUpdateDevice(const QBluetoothDeviceInfo &device);
    void rebuildVisibleList();
    bool passesFilter(const QBluetoothDeviceInfo &device) const;
    void updateStatusComplete();

    Ui::BtleScannerDialog *ui;
    QBluetoothDeviceDiscoveryAgent *m_discoveryAgent = nullptr;
    QStandardItemModel *m_model = nullptr;
    QList<QBluetoothDeviceInfo> m_allDiscovered;  // everything seen this scan
    QList<QBluetoothDeviceInfo> m_devices;        // rows currently shown (filtered)
    QBluetoothDeviceInfo m_selectedDevice;

    BtleSensorRole m_role = BtleSensorRole::HeartRate;
    bool m_hasRoleFilter = false;   // false → legacy "show everything" behaviour

    static constexpr int SCAN_TIMEOUT_MS = 10000; // 10 s BLE scan
};

#endif // BTLE_SCANNER_DIALOG_H
