#include "btle_hub_wasm.h"
#include "webbluetooth_bridge.h"
#include "btle_uuids.h"

#include <QDebug>
#include <cstring>
#include <cmath>

BtleHubWasm::BtleHubWasm(QObject *parent) : QObject(parent)
{
    // Expose window.mt_startBleScan() for Playwright test automation.
    WebBluetoothBridge::registerTestScanApi();
    WebBluetoothBridge::setNotificationCallback(
        [this](quint16 uuid16, const QByteArray &data) {
            onBleNotification(uuid16, data);
        });

    // Emit deviceConnected / serviceDiscoveryFinished only once the async
    // js_scanAndConnect has finished connecting and subscribing to all
    // notifications (Gap 2 fix).
    WebBluetoothBridge::setConnectedCallback(
        [this]() {
            m_connected = true;
            emit deviceConnected();
            emit serviceDiscoveryFinished();
            // Send FTMS Request Control (opcode 0x00) now that GATT is ready.
            // This handshake is required by the FTMS spec before ERG commands
            // (Set Target Power 0x05 / Indoor Bike Simulation 0x11) are accepted
            // by the trainer (Gap 6 fix).
            WebBluetoothBridge::requestFtmsControl();
        });

    WebBluetoothBridge::setConnectionErrorCallback(
        [this](const QString &errorString) {
            m_connected = false;
            emit connectionError(errorString);
        });

    // Invoked by bleDisconnectedC when the JS auto-reconnect loop is exhausted
    // (Gap 1 fix – wires the JS callback to the existing C++ handler).
    WebBluetoothBridge::setDisconnectedCallback(
        [this]() {
            onDisconnectedFromBridge();
        });

    // Invoked by bleReconnectRequestC when the user presses the DOM
    // "Reconnect" button (Gap 1 fix – routes the button press back into C++).
    WebBluetoothBridge::setReconnectRequestCallback(
        [this]() {
            m_userDisconnect = false;
            scanForDevice();
        });
}

BtleHubWasm::~BtleHubWasm()
{
    WebBluetoothBridge::disconnectDevice();
}

void BtleHubWasm::scanForDevice()
{
    WebBluetoothBridge::scanForDevices();
    // deviceConnected() and serviceDiscoveryFinished() are deferred to the
    // bleConnectedC callback, which fires only after js_scanAndConnect has
    // successfully connected and subscribed to all characteristics (Gap 2 fix).
}

void BtleHubWasm::disconnectFromDevice()
{
    m_userDisconnect = true;
    WebBluetoothBridge::disconnectDevice();
    m_connected = false;
    emit deviceDisconnected();
}

bool BtleHubWasm::isConnected() const { return m_connected; }

void BtleHubWasm::setWheelCircumferenceMm(int mm) { m_wheelCircMm = mm; }

void BtleHubWasm::setLoad(int /*deviceId*/, double watts)
{
    WebBluetoothBridge::sendFtmsSetTargetPower(static_cast<int>(watts));
}

void BtleHubWasm::setSlope(int /*deviceId*/, double grade)
{
    WebBluetoothBridge::sendFtmsSetIndoorBikeSimulation(static_cast<int>(grade * 100.0));
}

void BtleHubWasm::setResistanceLevel(int /*deviceId*/, int levelTenths)
{
    WebBluetoothBridge::sendFtmsSetResistanceLevel(levelTenths);
}

void BtleHubWasm::stopDecodingMsg()
{
    disconnectFromDevice();
}

void BtleHubWasm::onDisconnectedFromBridge()
{
    // Invoked by the JS layer when gattserverdisconnected fires and all
    // automatic reconnect attempts (3 × 5 s = 15 s) have been exhausted.
    // Intentional disconnects (via disconnectFromDevice()) set m_userDisconnect
    // and are ignored here.
    if (m_userDisconnect)
        return;

    qDebug() << "[BtleHubWasm] Unexpected disconnect – auto-reconnect exhausted";
    m_connected = false;
    emit deviceDisconnected();
    emit connectionError(tr("Bluetooth trainer disconnected. "
                            "Press Reconnect to reconnect your trainer."));
}

void BtleHubWasm::simulateNotification(quint16 characteristicUuid, const QByteArray &data)
{
    onBleNotification(characteristicUuid, data);
}

// ─── Notification dispatch ────────────────────────────────────────────────────

void BtleHubWasm::onBleNotification(quint16 uuid16, const QByteArray &data)
{
    switch (uuid16) {
    case 0x2A37: m_seenHr    = true; parseHrMeasurement(data);       break; // Heart Rate Measurement
    case 0x2A63: m_seenPower = true; parsePowerMeasurement(data);    break; // Cycling Power Measurement
    case 0x2A5B: m_seenCsc   = true; parseCscMeasurement(data);      break; // CSC Measurement
    case 0x2AD2: m_seenFtms  = true; parseFtmsIndoorBikeData(data);  break; // Indoor Bike Data
    case 0xAAB2: m_seenMoxy  = true; parseMoxyMeasurement(data);     break; // Moxy Muscle Oxygen
    case 0x2A19: parseBatteryLevel(data);         break; // Battery Level
    default:
        qDebug() << "[BtleHubWasm] Unknown characteristic UUID:" << Qt::hex << uuid16;
        break;
    }
}

// ─── Parsers (same logic as BtleHub native) ──────────────────────────────────

void BtleHubWasm::parseHrMeasurement(const QByteArray &data)
{
    if (data.isEmpty()) return;
    const quint8 flags = static_cast<quint8>(data[0]);
    int hr = 0;
    if (flags & 0x01) {
        // 16-bit HR value
        if (data.size() < 3) return;
        hr = static_cast<quint8>(data[1]) | (static_cast<quint8>(data[2]) << 8);
    } else {
        if (data.size() < 2) return;
        hr = static_cast<quint8>(data[1]);
    }
    emit signal_hr(SOLO_USER_ID, hr);
}

void BtleHubWasm::parseCscMeasurement(const QByteArray &data)
{
    if (data.isEmpty()) return;
    const quint8 flags = static_cast<quint8>(data[0]);
    int offset = 1;

    double speed = 0.0;
    int cadence = 0;

    if (flags & 0x01) { // Wheel Revolution Data present
        if (data.size() < offset + 6) return;
        quint32 revs;
        std::memcpy(&revs, data.constData() + offset, 4); offset += 4;
        quint16 eventTime;
        std::memcpy(&eventTime, data.constData() + offset, 2); offset += 2;

        if (!m_firstCsc) {
            quint32 revDiff  = revs - m_lastWheelRevs;
            quint16 timeDiff = eventTime - m_lastWheelTime;
            if (timeDiff > 0) {
                double rps = (revDiff * 1024.0) / timeDiff;
                speed = rps * m_wheelCircMm / 1000000.0 * 3600.0; // km/h
            }
        }
        m_lastWheelRevs = revs;
        m_lastWheelTime = eventTime;
    }

    if (flags & 0x02) { // Crank Revolution Data present
        if (data.size() < offset + 4) return;
        quint16 cranks;
        std::memcpy(&cranks, data.constData() + offset, 2); offset += 2;
        quint16 eventTime;
        std::memcpy(&eventTime, data.constData() + offset, 2);

        if (!m_firstCsc) {
            quint16 crankDiff = cranks - m_lastCrankRevs;
            quint16 timeDiff  = eventTime - m_lastCrankTime;
            if (timeDiff > 0)
                cadence = static_cast<int>(std::round((crankDiff * 1024.0 * 60.0) / timeDiff));
        }
        m_lastCrankRevs = cranks;
        m_lastCrankTime = eventTime;
    }

    m_firstCsc = false;
    if (speed > 0.0) emit signal_speed(SOLO_USER_ID, speed);
    if (cadence > 0)  emit signal_cadence(SOLO_USER_ID, cadence);
}

void BtleHubWasm::parsePowerMeasurement(const QByteArray &data)
{
    if (data.size() < 4) return;
    qint16 power;
    std::memcpy(&power, data.constData() + 2, 2);
    emit signal_power(SOLO_USER_ID, static_cast<int>(power));
}

void BtleHubWasm::parseFtmsIndoorBikeData(const QByteArray &data)
{
    if (data.size() < 3) return;
    quint16 flags;
    std::memcpy(&flags, data.constData(), 2);
    int offset = 2;

    // Instantaneous Speed (0.01 km/h resolution) – present when bit 0 is 0
    if (!(flags & 0x0001) && data.size() >= offset + 2) {
        quint16 speedRaw;
        std::memcpy(&speedRaw, data.constData() + offset, 2);
        emit signal_speed(SOLO_USER_ID, speedRaw * 0.01);
        offset += 2;
    }

    // Average Speed – bit 1
    if (flags & 0x0002) offset += 2;

    // Instantaneous Cadence (0.5 rpm) – bit 2
    if (!(flags & 0x0004) && data.size() >= offset + 2) {
        quint16 cadenceRaw;
        std::memcpy(&cadenceRaw, data.constData() + offset, 2);
        emit signal_cadence(SOLO_USER_ID, static_cast<int>(cadenceRaw * 0.5));
        offset += 2;
    }

    // Average Cadence – bit 3
    if (flags & 0x0008) offset += 2;

    // Total Distance – bit 4
    if (flags & 0x0010) offset += 3;

    // Resistance Level – bit 5
    if (flags & 0x0020) offset += 2;

    // Instantaneous Power – bit 6
    if (!(flags & 0x0040) && data.size() >= offset + 2) {
        qint16 powerRaw;
        std::memcpy(&powerRaw, data.constData() + offset, 2);
        emit signal_power(SOLO_USER_ID, static_cast<int>(powerRaw));
    }
}

// Moxy Muscle Oxygen Measurement (0xAAB2)
// Bytes 0-1: flags (uint16 LE) – bit0=SmO2 present, bit1=tHb present
// Bytes 2-3: SmO2 (uint16 LE, units 0.1 %)
// Bytes 4-5: tHb  (uint16 LE, units 0.01 g/dL)
void BtleHubWasm::parseMoxyMeasurement(const QByteArray &data)
{
    if (data.size() < 6)
        return;

    quint16 flags;
    std::memcpy(&flags, data.constData(), 2);
    quint16 rawSmo2;
    std::memcpy(&rawSmo2, data.constData() + 2, 2);
    quint16 rawThb;
    std::memcpy(&rawThb, data.constData() + 4, 2);

    double smo2 = (flags & 0x01) ? rawSmo2 / 10.0  : 0.0;
    double thb  = (flags & 0x02) ? rawThb  / 100.0 : 0.0;

    emit signal_oxygen(SOLO_USER_ID, smo2, thb);
}

// Battery Level (0x2A19)
// Byte 0: Battery percentage 0–100
void BtleHubWasm::parseBatteryLevel(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    int percentage = static_cast<quint8>(data[0]);
    percentage = qBound(0, percentage, 100);

    emit signal_battery(determineSensorType(), percentage);
}

// Return a human-readable sensor type based on which measurement UUIDs have
// been received, mirroring BtleHub::determineSensorType().
QString BtleHubWasm::determineSensorType() const
{
    if (m_seenHr)    return QStringLiteral("Heart Rate");
    if (m_seenPower) return QStringLiteral("Power");
    if (m_seenFtms)  return QStringLiteral("Trainer");
    if (m_seenCsc)   return QStringLiteral("Speed/Cadence");
    if (m_seenMoxy)  return QStringLiteral("Oxygen");
    return QStringLiteral("Sensor");
}
