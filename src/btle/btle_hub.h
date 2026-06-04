#ifndef BTLE_HUB_H
#define BTLE_HUB_H

#include <QObject>
#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>
#include <QTimer>

/*
 * BtleHub
 *
 * Connects to a Bluetooth Low Energy cycling device and emits
 * sensor-data signals, allowing WorkoutDialog to receive data
 * without knowledge of the underlying connection method.
 *
 * Supported standard BTLE profiles:
 *   - Cycling Speed & Cadence Service  (0x1816)  cadence + speed
 *   - Heart Rate Service               (0x180D)  heart rate
 *   - Cycling Power Service            (0x1818)  power
 *   - Fitness Machine Service / FTMS   (0x1826)  indoor-bike data + resistance control
 *   - Moxy Muscle Oxygen Service (0xAAB0) SmO2 + tHb
 */
class BtleHub : public QObject
{
    Q_OBJECT

public:
    explicit BtleHub(QObject *parent = nullptr);
    ~BtleHub();

    // Connect to the selected BLE device. Call after the scanner dialog has
    // chosen a device.
    void connectToDevice(const QBluetoothDeviceInfo &device);
    void disconnectFromDevice();

    bool isConnected() const;

    // Wheel circumference in mm – used to convert wheel revolutions to speed.
    // Should be set from account->wheel_circ before connecting.
    void setWheelCircumferenceMm(int mm);

signals:
    // ----------- Data signals (same signatures as Hub) ---------------------
    void signal_hr(int userID, int hr);
    void signal_cadence(int userID, int cadence);
    void signal_speed(int userID, double speed);       // km/h
    void signal_power(int userID, int power);          // watts
    void signal_oxygen(int userID, double smo2Percent, double thbGdL);
    /// Emitted when the connected device's Battery Service reports a level.
    /// \param sensorType  Human-readable sensor name ("Heart Rate", "Power", …)
    /// \param percentage  Battery level 0–100 (%)
    void signal_battery(QString sensorType, int percentage);

    // ----------- Connection status signals ---------------------------------
    void deviceConnected();
    void deviceDisconnected();
    void connectionError(const QString &errorString);
    void serviceDiscoveryFinished();

public slots:
    // Slots with same signatures as Hub::setLoad / Hub::setSlope so they can
    // be wired up identically from WorkoutDialog signals.
    void setLoad(int antID, double watts);
    void setSlope(int antID, double grade);
    void stopDecodingMsg();

    // Test hook: inject raw BLE notification bytes as if received from hardware.
    // uuid is the 16-bit BT SIG characteristic UUID (e.g. 0x2A37 = Heart Rate).
    // Signals (signal_hr / signal_cadence / signal_speed / signal_power) are
    // emitted exactly as they would be on real hardware.
    void simulateNotification(quint16 characteristicUuid, const QByteArray &data);

private slots:
    void onControllerConnected();
    void onControllerDisconnected();
    void onControllerError(QLowEnergyController::Error error);
    void onServiceDiscovered(const QBluetoothUuid &serviceUuid);
    void onDiscoveryFinished();

    void onServiceStateChanged(QLowEnergyService::ServiceState state);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic,
                                 const QByteArray &value);
    void onCscStopTimer();
    void onReconnectTimer();

private:
    void setupService(QLowEnergyService *service);
    void enableNotification(QLowEnergyService *service,
                            const QLowEnergyCharacteristic &characteristic);
    void requestFtmsControl();

    void parseHrMeasurement(const QByteArray &data);
    void parseCscMeasurement(const QByteArray &data);
    void parsePowerMeasurement(const QByteArray &data);
    void parseFtmsIndoorBikeData(const QByteArray &data);
    void parseMoxyMeasurement(const QByteArray &data);
    void parseBatteryLevel(const QByteArray &data);
    QString determineSensorType() const; ///< Infer sensor type from connected services

    QLowEnergyController *m_controller = nullptr;

    QLowEnergyService *m_hrService       = nullptr;
    QLowEnergyService *m_cscService      = nullptr;
    QLowEnergyService *m_powerService    = nullptr;
    QLowEnergyService *m_ftmsService     = nullptr;
    QLowEnergyService *m_moxyService     = nullptr;
    QLowEnergyService *m_batteryService  = nullptr;

    bool m_ftmsControlRequested = false;

    // Zero-out cadence / speed after a few missed messages
    QTimer *m_cscStopTimer = nullptr;
    static constexpr int CSC_STOP_TIMEOUT_MS    = 3000;

    // Auto-reconnect
    QTimer              *m_reconnectTimer    = nullptr;
    QBluetoothDeviceInfo m_reconnectDevice;
    int                  m_reconnectAttempts = 0;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 3;
    static constexpr int RECONNECT_INTERVAL_MS  = 5000;

    static constexpr int DEFAULT_WHEEL_CIRC_MM  = 2100; // ~700c / 29" wheel

    // WorkoutDialog indexes its per-user data arrays as arrDataWorkout[userID-1],
    // so a single rider must be userID 1 (matching SimulatorHub). Emitting 0 here
    // produces an out-of-bounds arrDataWorkout[-1] access on every live packet.
    static constexpr int SOLO_USER_ID = 1;

    int m_wheelCircMm = DEFAULT_WHEEL_CIRC_MM;

    // CSC state – mirrors the cadence / speed controller logic
    quint32 m_lastWheelRevolutions  = 0;
    quint16 m_lastWheelEventTime    = 0;
    quint16 m_lastCrankRevolutions  = 0;
    quint16 m_lastCrankEventTime    = 0;
    bool    m_firstCscMeasurement   = true;
};

#endif // BTLE_HUB_H
