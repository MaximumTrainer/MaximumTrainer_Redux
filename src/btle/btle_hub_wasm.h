#ifndef BTLE_HUB_WASM_H
#define BTLE_HUB_WASM_H

/*
 * BtleHubWasm
 *
 * WebAssembly replacement for BtleHub.  Presents exactly the same signals and
 * public API as BtleHub so the rest of the application (WorkoutDialog, etc.)
 * can use either hub without modification.
 *
 * Connectivity is provided by the WebBluetooth API via webbluetooth_bridge.h.
 * Raw GATT characteristic bytes are parsed by the same parse*() helpers used
 * in the native BtleHub (copied / shared via a common utility header).
 */

#include <QObject>

class BtleHubWasm : public QObject
{
    Q_OBJECT

public:
    explicit BtleHubWasm(QObject *parent = nullptr);
    ~BtleHubWasm();

    // Request BLE device selection (must be called from a user-initiated slot).
    void scanForDevice();
    void disconnectFromDevice();

    bool isConnected() const;

    void setWheelCircumferenceMm(int mm);

    // Keep API compatible with BtleHub so callers can use either hub
    // via connectToDevice(const QBluetoothDeviceInfo&) on native.
    // On Wasm, device selection is done by the browser; this is a no-op.
    template<typename T>
    void connectToDevice(const T &) { scanForDevice(); }

signals:
    // ── Data signals (same signatures as BtleHub / Hub) ──────────────────
    void signal_hr(int userID, int hr);
    void signal_cadence(int userID, int cadence);
    void signal_speed(int userID, double speed);   // km/h
    void signal_power(int userID, int power);      // watts
    void signal_oxygen(int userID, double smo2Percent, double thbGdL);
    /// Emitted when the connected device's Battery Service reports a level.
    /// \param sensorType  Human-readable sensor name ("Heart Rate", "Power", …)
    /// \param percentage  Battery level 0–100 (%)
    void signal_battery(QString sensorType, int percentage);

    // ── Connection status ─────────────────────────────────────────────────
    void deviceConnected();
    void deviceDisconnected();
    void connectionError(const QString &errorString);
    void serviceDiscoveryFinished();
    /// Mirrors BtleHub: whether the trainer supports Set Target Resistance Level
    /// (0x04). The Wasm hub doesn't read the feature char, so we assume support —
    /// virtual shifting is opt-in and targets FTMS 0x04-capable single-cog trainers.
    void signal_resistanceLevelSupported(bool supported);

public:
    bool resistanceLevelSupported() const { return true; }

public slots:
    void setLoad(int antID, double watts);
    void setSlope(int antID, double grade);
    /// FTMS Set Target Resistance Level (0x04), 0.1 units. Same signature as
    /// BtleHub so WorkoutDialog wiring compiles on Wasm.
    void setResistanceLevel(int antID, int levelTenths);
    void stopDecodingMsg();

    // Test hook – same as BtleHub::simulateNotification
    void simulateNotification(quint16 characteristicUuid, const QByteArray &data);

private:
    void onBleNotification(quint16 uuid16, const QByteArray &data);

    // Invoked by the WebBluetoothBridge callback when an unexpected GATT
    // disconnection occurs and all JS auto-reconnect attempts are exhausted.
    void onDisconnectedFromBridge();

    void parseHrMeasurement(const QByteArray &data);
    void parseCscMeasurement(const QByteArray &data);
    void parsePowerMeasurement(const QByteArray &data);
    void parseFtmsIndoorBikeData(const QByteArray &data);
    void parseMoxyMeasurement(const QByteArray &data);
    void parseBatteryLevel(const QByteArray &data);

    // Infer a human-readable sensor type from the measurement UUIDs seen so
    // far, mirroring BtleHub::determineSensorType() which checks service ptrs.
    QString determineSensorType() const;

    // WorkoutDialog indexes arrDataWorkout[userID-1]; a single rider is userID 1
    // (matching SimulatorHub and the native BtleHub). See btle_hub.h.
    static constexpr int SOLO_USER_ID = 1;

    bool   m_connected         = false;
    bool   m_userDisconnect    = false; // true when disconnectFromDevice() was called intentionally
    int    m_wheelCircMm       = 2100;

    quint32 m_lastWheelRevs    = 0;
    quint16 m_lastWheelTime    = 0;
    quint16 m_lastCrankRevs    = 0;
    quint16 m_lastCrankTime    = 0;
    bool    m_firstCsc         = true;

    // Track which measurement types have been seen for determineSensorType()
    bool    m_seenHr           = false;
    bool    m_seenPower        = false;
    bool    m_seenFtms         = false;
    bool    m_seenCsc          = false;
    bool    m_seenMoxy         = false;
};

// Alias so headers that #include "btle_hub.h" can use BtleHub type on Wasm
// when btle_hub_wasm.h is included in its place.
using BtleHub = BtleHubWasm;

#endif // BTLE_HUB_WASM_H
