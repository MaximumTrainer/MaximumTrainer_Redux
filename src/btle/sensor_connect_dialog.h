#ifndef SENSOR_CONNECT_DIALOG_H
#define SENSOR_CONNECT_DIALOG_H

#include <QDialog>
#include <QMap>
#include <QVector>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>

#include "btle_sensor_config.h"

class BtleHub;
class ZwiftClickManager;
class QGridLayout;
class QLabel;
class QPushButton;
class QTimer;

/*
 * SensorConnectDialog
 *
 * Pre-workout connection screen for the multi-device flow. Given the user's
 * saved sensors, it runs a single shared BLE discovery scan, matches each
 * discovered device against the saved set, and connects one BtleHub per
 * physical device (deduplicated by address/UUID). Each saved sensor shows a
 * live status: Searching → Connected / Not found / Error.
 *
 * On accept the caller takes the connected hubs via connectedHubs() and calls
 * detachHubs() to assume ownership; otherwise the dialog tears them down.
 *
 * Desktop only – not compiled on WASM (where BtleHub aliases BtleHubWasm and
 * device discovery is browser-driven).
 */
class SensorConnectDialog : public QDialog
{
    Q_OBJECT

public:
    SensorConnectDialog(const QMap<BtleSensorRole, BtleSavedSensor> &savedSensors,
                        QWidget *parent = nullptr);
    ~SensorConnectDialog() override;

    /// Connected hubs keyed by role. Two roles may share one BtleHub* when the
    /// same physical device is saved for both (e.g. Trainer + Power).
    QMap<BtleSensorRole, BtleHub*> connectedHubs() const;

    /// Transfer hub ownership to the caller so the destructor leaves them alive.
    /// Reparents the hubs off this dialog so Qt's parent-child cleanup does not
    /// delete them when the dialog is destroyed.
    void detachHubs();

    /// Adds a Zwift Click row and starts pairing the controllers now, so the
    /// rider can see them connect before the workout starts.
    void enableZwiftClick();
    /// Hands the (possibly connected) controllers to the caller; nullptr if
    /// Zwift Click was not enabled.
    ZwiftClickManager *takeZwiftClickManager();

signals:
    /// Emitted when the user clicks "Manage Sensors" – the caller opens the
    /// Sensors preferences page.
    void openSensorPreferences();

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onScanFinished();
    void onScanError(QBluetoothDeviceDiscoveryAgent::Error error);
    void onContinueClicked();
    void onManageClicked();

private:
    enum class SlotStatus { Searching, Connecting, Connected, NotFound, Error };

    struct ConnectSlot {
        BtleSensorRole  role;
        BtleSavedSensor saved;
        SlotStatus      status = SlotStatus::Searching;
        BtleHub        *hub    = nullptr; // shared with other slots when same device
        QLabel         *statusLabel = nullptr;
    };

    void startDiscovery();
    void teardownHubs();
    void updateSlotUi(int slotIndex);
    void refreshButtons();
    bool anyConnected() const;
    void markSlotConnected(BtleHub *hub);
    void markSlotError(BtleHub *hub);

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    QVector<ConnectSlot>            m_slots;
    QGridLayout                    *m_grid = nullptr;
    ZwiftClickManager              *m_click = nullptr;
    QLabel                         *m_clickStatus = nullptr;

    void updateClickUi();
    bool clickReady() const;   // no Click wanted, or the right controller is in

    // One hub per physical device, keyed by its saved identifier (address/UUID).
    QMap<QString, BtleHub*>         m_hubByDevice;

    bool  m_hubsDetached = false;

    QPushButton *m_btnContinue = nullptr;
    QPushButton *m_btnManage   = nullptr;

    QTimer *m_autoContinueTimer = nullptr;

    static constexpr int SCAN_TIMEOUT_MS      = 15000; // 15 s shared discovery
    static constexpr int AUTO_CONTINUE_MS     = 5000;  // grace after scan ends
};

#endif // SENSOR_CONNECT_DIALOG_H
