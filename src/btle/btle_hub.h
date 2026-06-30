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

    // 1-based rider id emitted in every data signal. Defaults to 1 (solo); in
    // Studio mode each rider's hub is given a distinct id so WorkoutDialog
    // routes the data to the correct rider box. Mirrors SimulatorHub::setUserID.
    // Setting it also restricts setLoad()/setSlope() to commands addressed to
    // this rider, so one rider's ERG target does not drive every trainer. Solo
    // never calls this, so its trainer keeps accepting every command (the
    // broadcast deviceId from the legacy sensor list is ignored, as before).
    void setUserID(int id) { m_userID = id; m_filterControlByUserID = true; }

signals:
    // ----------- Data signals (same signatures as Hub) ---------------------
    void signal_hr(int userID, int hr);
    void signal_cadence(int userID, int cadence);
    void signal_speed(int userID, double speed);       // km/h
    void signal_power(int userID, int power);          // watts
    void signal_balance(int userID, int rightPedalPercentage); // L/R pedal balance, right pedal %
    // Torque effectiveness / pedal smoothness. Decoding these from a real sensor
    // needs the Cycling Power Vector characteristic (0x2A64) — not yet parsed, so
    // BtleHub does not emit this today; the simulator does, to exercise the UI.
    void signal_pedal(int userID, double leftTorqueEff, double rightTorqueEff,
                      double leftPedalSmooth, double rightPedalSmooth, double combinedPedalSmooth);
    void signal_oxygen(int userID, double smo2Percent, double thbGdL);
    /// Emitted when the connected device's Battery Service reports a level.
    /// \param sensorType  Human-readable sensor name ("Heart Rate", "Power", …)
    /// \param percentage  Battery level 0–100 (%)
    void signal_battery(QString sensorType, int percentage);

    /// Emitted once per connection, the first time it produces a heart-rate
    /// reading. Trainers that bridge an HR strap expose it as a standard 0x180D
    /// Heart Rate service on the trainer connection; wired only for the trainer
    /// hub, this lets the UI mark the Heart Rate slot "provided by trainer".
    void signal_trainerProvidesHr(int userID);

    // ----------- Connection status signals ---------------------------------
    void deviceConnected();
    void deviceDisconnected();
    void connectionError(const QString &errorString);
    void serviceDiscoveryFinished();
    /// Emitted once the FTMS Feature char is read: whether the trainer supports
    /// Set Target Resistance Level (0x04). Drives ERG-vs-resistance gear mode.
    void signal_resistanceLevelSupported(bool supported);

public slots:
    // Slots with same signatures as Hub::setLoad / Hub::setSlope so they can
    // be wired up identically from WorkoutDialog signals.
    void setLoad(int deviceId, double watts);
    void setSlope(int deviceId, double grade);
    /// Whether the connected trainer advertised Set Target Resistance Level
    /// (0x04) support in its FTMS feature char. Valid after discovery.
    bool resistanceLevelSupported() const { return m_resistanceLevelSupported; }
    // FTMS Set Target Resistance Level (opcode 0x04). levelTenths is the raw
    // resistance level in 0.1 units (the 2AD6 "Supported Resistance Level Range"
    // representation). Unlike ERG, this sets a fixed brake — resistance changes
    // instantly and power follows effort, so virtual gears feel like real gears.
    void setResistanceLevel(int deviceId, int levelTenths);
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
    void onDescriptorWritten(const QLowEnergyDescriptor &descriptor,
                             const QByteArray &value);
    void onCscStopTimer();
    void onReconnectTimer();

private:
    // (Re)create the controller and start a connection to m_reconnectDevice
    // without touching m_reconnectAttempts, so connect-phase retries keep a
    // running count against MAX_RECONNECT_ATTEMPTS.
    void attemptConnection();
    void setupService(QLowEnergyService *service);
    void enableNotification(QLowEnergyService *service,
                            const QLowEnergyCharacteristic &characteristic);
    void enableIndication(QLowEnergyService *service,
                          const QLowEnergyCharacteristic &characteristic);
    void requestFtmsControl();
    void handleFtmsControlPointResponse(const QByteArray &value);
    /// Queue (or send) an FTMS control-point command.  Ops are serialized —
    /// one in flight until the trainer's response indication — and held back
    /// until control is granted; overlapping writes are answered with ATT
    /// error 0x80 by real trainers.  Queued commands coalesce: only the
    /// newest target matters.
    void sendFtmsCommand(const QByteArray &cmd);
    void writeFtmsCommandNow(const QByteArray &cmd);

    void parseHrMeasurement(const QByteArray &data);
    void parseCscMeasurement(const QByteArray &data);
    void parsePowerMeasurement(const QByteArray &data);
    void parseFtmsIndoorBikeData(const QByteArray &data);
    void parseFtmsFeature(const QByteArray &data);
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
    bool m_ftmsControlGranted   = false;
    bool m_resistanceLevelSupported = false;   // FTMS 0x04 (from 0x2ACC feature)

    // Control-point serialization: one op in flight until the trainer's
    // response indication arrives; the newest deferred command waits in
    // m_ftmsPendingCmd (a newer target supersedes an older queued one).
    bool       m_ftmsOpInFlight = false;
    QByteArray m_ftmsPendingCmd;
    QByteArray m_ftmsLastSentCmd;   ///< payload of the op currently/last on the wire
    QByteArray m_ftmsLastAckedCmd;  ///< last payload the trainer confirmed — duplicates are skipped
    QTimer    *m_ftmsOpTimeout = nullptr; ///< releases a write whose response never came
    static constexpr int FTMS_OP_TIMEOUT_MS = 2500;

    // Zero-out cadence / speed after a few missed messages
    QTimer *m_cscStopTimer = nullptr;
    static constexpr int CSC_STOP_TIMEOUT_MS    = 3000;

    // Auto-reconnect
    QTimer              *m_reconnectTimer    = nullptr;
    QBluetoothDeviceInfo m_reconnectDevice;
    int                  m_reconnectAttempts = 0;
    // Set by disconnectFromDevice() so an intentional teardown (rescan, Skip,
    // workout close, device removed) does NOT schedule an auto-reconnect — a
    // stale hub reconnecting mid-teardown issued reads against a controller
    // being destroyed ("A method was called at an unexpected time"). Mirrors
    // BtleHubWasm. Cleared by connectToDevice() (an intentional connect).
    bool                 m_userDisconnect    = false;
    static constexpr int MAX_RECONNECT_ATTEMPTS = 3;
    static constexpr int RECONNECT_INTERVAL_MS  = 5000;

    // Fixed wheel circumference (700c / 29") used to convert CSC wheel
    // revolutions to speed. There is no UI to change it, so it's a constant.
    static constexpr int DEFAULT_WHEEL_CIRC_MM  = 2100;

    // WorkoutDialog indexes its per-user data arrays as arrDataWorkout[userID-1],
    // so a single rider must be userID 1 (matching SimulatorHub). Emitting 0 here
    // produces an out-of-bounds arrDataWorkout[-1] access on every live packet.
    static constexpr int SOLO_USER_ID = 1;

    // Rider id emitted in every data signal (settable via setUserID()).
    int m_userID = SOLO_USER_ID;
    // When true (Studio mode), setLoad()/setSlope() act only on commands whose
    // deviceId matches m_userID. False for solo so behaviour is unchanged.
    bool m_filterControlByUserID = false;

    int m_wheelCircMm = DEFAULT_WHEEL_CIRC_MM;

    // CSC state – mirrors the cadence / speed controller logic
    quint32 m_lastWheelRevolutions  = 0;
    quint16 m_lastWheelEventTime    = 0;
    quint16 m_lastCrankRevolutions  = 0;
    quint16 m_lastCrankEventTime    = 0;
    bool    m_firstCscMeasurement   = true;

    // Latched once this connection has produced a heart-rate reading (via the
    // 0x180D service), so signal_trainerProvidesHr fires only once.
    bool    m_hrSeen               = false;
};

#endif // BTLE_HUB_H
