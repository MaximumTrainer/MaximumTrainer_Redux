#include "btle_hub.h"
#include "btle_uuids.h"
#include "ftms_control.h"

#include "logger.h"
#include <QLowEnergyDescriptor>
#include <limits>

// Standard BLE service and characteristic UUIDs (defined locally in the TU)
// Qt6 moved the enum values into nested enums inside QBluetoothUuid.
namespace BtleUuid {
// Services
static const QBluetoothUuid HeartRate             (QBluetoothUuid::ServiceClassUuid::HeartRate);
static const QBluetoothUuid CyclingSpeedCadence   (QBluetoothUuid::ServiceClassUuid::CyclingSpeedAndCadence);
static const QBluetoothUuid CyclingPower          (QBluetoothUuid::ServiceClassUuid::CyclingPower);
// Fitness Machine Service (FTMS) – not in Qt enum, use raw 16-bit UUID
static const QBluetoothUuid FitnessMachine        (quint16(0x1826));
// Battery Service – standard BT SIG service for battery level percentage
static const QBluetoothUuid BatteryService        (quint16(0x180F));

// Characteristics
static const QBluetoothUuid HeartRateMeasurement    (QBluetoothUuid::CharacteristicType::HeartRateMeasurement);
static const QBluetoothUuid CSCMeasurement          (QBluetoothUuid::CharacteristicType::CSCMeasurement);
static const QBluetoothUuid CyclingPowerMeasurement (QBluetoothUuid::CharacteristicType::CyclingPowerMeasurement);
static const QBluetoothUuid FtmsIndoorBikeData      (quint16(0x2AD2));
static const QBluetoothUuid FtmsRowerData           (quint16(0x2AD1));
static const QBluetoothUuid FtmsControlPoint        (quint16(0x2AD9));
static const QBluetoothUuid FtmsFeature             (quint16(0x2ACC));
static const QBluetoothUuid FtmsMachineStatus       (quint16(0x2ADA));
static const QBluetoothUuid BatteryLevel            (quint16(0x2A19));
// Moxy Muscle Oxygen Monitor (proprietary UUIDs - not in BT SIG enum)
static const QBluetoothUuid MoxyService             (quint16(0xAAB0));
static const QBluetoothUuid MoxyMeasurement         (quint16(0xAAB2));

// Descriptors
static const QBluetoothUuid ClientCharacteristicConfig
    (QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
}

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────
BtleHub::BtleHub(QObject *parent)
    : QObject(parent)
{
    m_cscStopTimer = new QTimer(this);
    m_cscStopTimer->setSingleShot(true);
    connect(m_cscStopTimer, &QTimer::timeout, this, &BtleHub::onCscStopTimer);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &BtleHub::onReconnectTimer);

    m_ftmsControl = new FtmsControl(this);
    connect(m_ftmsControl, &FtmsControl::writeRequested, this, &BtleHub::writeFtmsControlPoint);
    connect(m_ftmsControl, &FtmsControl::granted, this, [this]() {
        m_ftmsSetupWatchdog->stop();
        m_ftmsSetupReconnects = 0;
    });

    m_ftmsSetupWatchdog = new QTimer(this);
    m_ftmsSetupWatchdog->setSingleShot(true);
    connect(m_ftmsSetupWatchdog, &QTimer::timeout, this, &BtleHub::onFtmsSetupWatchdog);
}

BtleHub::~BtleHub()
{
    disconnectFromDevice();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void BtleHub::connectToDevice(const QBluetoothDeviceInfo &device)
{
    // Fresh, user-initiated connect: remember the target and reset the retry
    // budget. The actual controller setup (and every retry) goes through
    // attemptConnection(), which must NOT reset the budget.
    m_reconnectDevice   = device;
    m_reconnectAttempts = 0;
    m_ftmsSetupReconnects = 0;
    m_userDisconnect    = false;
    attemptConnection();
}

void BtleHub::attemptConnection()
{
    if (m_controller) {
        m_controller->disconnectFromDevice();
        delete m_controller;
        m_controller = nullptr;
    }

    // Clean up any previously-created service objects
    delete m_hrService;       m_hrService       = nullptr;
    delete m_cscService;      m_cscService      = nullptr;
    delete m_powerService;    m_powerService    = nullptr;
    delete m_ftmsService;     m_ftmsService     = nullptr;
    delete m_moxyService;     m_moxyService     = nullptr;
    delete m_batteryService;  m_batteryService  = nullptr;

    m_wheelBaselineSet      = false;
    m_crankBaselineSet      = false;
    m_cpsPowerSeen          = false;
    m_cscCadenceSeen        = false;
    m_ftmsControl->reset();
    m_ftmsSetupWatchdog->stop();

    m_controller = QLowEnergyController::createCentral(m_reconnectDevice, this);

    connect(m_controller, &QLowEnergyController::connected,
            this, &BtleHub::onControllerConnected);
    connect(m_controller, &QLowEnergyController::disconnected,
            this, &BtleHub::onControllerDisconnected);
    connect(m_controller, &QLowEnergyController::errorOccurred,
            this, &BtleHub::onControllerError);
    connect(m_controller, &QLowEnergyController::serviceDiscovered,
            this, &BtleHub::onServiceDiscovered);
    connect(m_controller, &QLowEnergyController::discoveryFinished,
            this, &BtleHub::onDiscoveryFinished);

    m_controller->connectToDevice();
}

void BtleHub::disconnectFromDevice()
{
    // Intentional teardown: cancel any pending reconnect and suppress the one
    // that onControllerDisconnected() would otherwise schedule.
    m_userDisconnect = true;
    if (m_reconnectTimer)
        m_reconnectTimer->stop();
    if (m_ftmsSetupWatchdog)
        m_ftmsSetupWatchdog->stop();
    if (m_controller) {
        m_controller->disconnectFromDevice();
    }
}

bool BtleHub::isConnected() const
{
    return m_controller &&
           m_controller->state() == QLowEnergyController::DiscoveredState;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public slots – Trainer control (FTMS)
// ─────────────────────────────────────────────────────────────────────────────
void BtleHub::setLoad(int deviceId, double watts)
{
    Q_UNUSED(deviceId)

    // FTMS: Set Target Power (opcode 0x05), value = int16 watts
    qint16 w = static_cast<qint16>(qBound(-32768.0, watts, 32767.0));
    QByteArray cmd(3, '\0');
    cmd[0] = 0x05;                      // Set Target Power opcode
    cmd[1] = static_cast<char>(w & 0xFF);
    cmd[2] = static_cast<char>((w >> 8) & 0xFF);
    m_ftmsControl->setTarget(cmd);
}

void BtleHub::setSlope(int deviceId, double grade)
{
    Q_UNUSED(deviceId)

    // FTMS: Set Indoor Bike Simulation (opcode 0x11)
    // Parameters: wind speed (int16, 0.001 m/s), grade (int16, 0.01 %), crr, cw
    // grade parameter is int16 in units of 0.01%, range ±327.67% (FTMS spec max is ±200%)
    qint16 gradeParam = static_cast<qint16>(
        qBound(-32768.0, grade * 100.0, 32767.0));
    QByteArray cmd(7, '\0');
    cmd[0] = 0x11;                             // Simulation opcode
    cmd[1] = 0x00; cmd[2] = 0x00;             // wind speed = 0
    cmd[3] = static_cast<char>(gradeParam & 0xFF);
    cmd[4] = static_cast<char>((gradeParam >> 8) & 0xFF);
    cmd[5] = 0x00;                             // crr (uint8, 0.0001)
    cmd[6] = 0x00;                             // cw  (uint8, 0.01)
    m_ftmsControl->setTarget(cmd);
}

void BtleHub::setResistanceLevel(int deviceId, int levelTenths)
{
    Q_UNUSED(deviceId)

    // FTMS: Set Target Resistance Level (opcode 0x04), value = int16 in 0.1 units
    // (same representation as the Supported Resistance Level Range, 0x2AD6).
    qint16 lvl = static_cast<qint16>(qBound(-32768, levelTenths, 32767));
    QByteArray cmd(3, '\0');
    cmd[0] = 0x04;                          // Set Target Resistance Level opcode
    cmd[1] = static_cast<char>(lvl & 0xFF);
    cmd[2] = static_cast<char>((lvl >> 8) & 0xFF);
    m_ftmsControl->setTarget(cmd);
}

void BtleHub::writeFtmsControlPoint(const QByteArray &payload)
{
    QLowEnergyService *service = m_ftmsService;
    const QLowEnergyCharacteristic controlPoint =
        service ? service->characteristic(BtleUuid::FtmsControlPoint) : QLowEnergyCharacteristic();
    if (!service || !controlPoint.isValid()) {
        m_ftmsControl->onWriteFailed();
        return;
    }
    service->writeCharacteristic(controlPoint, payload, QLowEnergyService::WriteWithResponse);
}

void BtleHub::reassertControl()
{
    if (!m_ftmsService)
        return;
    m_ftmsControl->reassert();
    // Control never came up on this connection: do not wait out the full
    // watchdog before the ride needs it.
    if (!m_ftmsControl->controlGranted() && !m_ftmsSetupWatchdog->isActive())
        m_ftmsSetupWatchdog->start(FTMS_SETUP_TIMEOUT_MS);
}

bool BtleHub::ftmsControlGranted() const
{
    return m_ftmsControl->controlGranted();
}

void BtleHub::onFtmsSetupWatchdog()
{
    if (!m_ftmsService || m_ftmsControl->controlGranted() || m_userDisconnect)
        return;
    // A data-only FTMS device has no Control Point: nothing to set up.
    if (m_ftmsService->state() == QLowEnergyService::RemoteServiceDiscovered
            && !m_ftmsService->characteristic(BtleUuid::FtmsControlPoint).isValid())
        return;
    if (m_ftmsSetupReconnects >= MAX_FTMS_SETUP_RECONNECTS) {
        LOG_WARN("BtleHub", QStringLiteral("FTMS control still not established after %1 reconnects")
                                .arg(m_ftmsSetupReconnects));
        return;
    }
    LOG_WARN("BtleHub", QStringLiteral("FTMS control not established (control point %1), reconnecting the trainer")
                            .arg(m_ftmsControl->isReady() ? QStringLiteral("ready") : QStringLiteral("not set up")));
    reconnectForFtmsSetup();
}

void BtleHub::reconnectForFtmsSetup()
{
    ++m_ftmsSetupReconnects;
    // The old controller's disconnected() must not start the regular
    // auto-reconnect on top of this one.
    if (m_controller)
        disconnect(m_controller, nullptr, this, nullptr);
    m_cscStopTimer->stop();
    emit deviceDisconnected();
    attemptConnection();
}

void BtleHub::stopDecodingMsg()
{
    disconnectFromDevice();
}

void BtleHub::simulateNotification(quint16 characteristicUuid, const QByteArray &data)
{
    switch (characteristicUuid) {
    case BTLE_UUID_HR_MEASUREMENT:    parseHrMeasurement(data);      break;
    case BTLE_UUID_CSC_MEASUREMENT:   parseCscMeasurement(data);     break;
    case BTLE_UUID_POWER_MEASUREMENT: parsePowerMeasurement(data);   break;
    case BTLE_UUID_FTMS_BIKE_DATA:    parseFtmsIndoorBikeData(data); break;
    case BTLE_UUID_FTMS_ROWER_DATA:   parseFtmsRowerData(data);      break;
    case BTLE_UUID_FTMS_FEATURE:      parseFtmsFeature(data);        break;
    case BTLE_UUID_MOXY_MEASUREMENT:  parseMoxyMeasurement(data);    break;
    case BTLE_UUID_BATTERY_LEVEL:     parseBatteryLevel(data);       break;
    default: break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Controller slots
// ─────────────────────────────────────────────────────────────────────────────
void BtleHub::onControllerConnected()
{
    LOG_INFO("BtleHub", QStringLiteral("Device connected — discovering services"));
    // A successful link refreshes the retry budget so a later drop gets its
    // own full set of attempts.
    m_reconnectAttempts = 0;
    m_controller->discoverServices();
}

void BtleHub::onControllerDisconnected()
{
    LOG_INFO("BtleHub", QStringLiteral("Device disconnected"));
    m_cscStopTimer->stop();
    m_ftmsSetupWatchdog->stop();
    m_ftmsControl->reset();
    m_hrSeen = false;
    emit deviceDisconnected();

    // Only an unexpected drop should auto-reconnect; an intentional
    // disconnectFromDevice() must stay disconnected.
    if (!m_userDisconnect && m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS)
        m_reconnectTimer->start(RECONNECT_INTERVAL_MS);
}

void BtleHub::onReconnectTimer()
{
    ++m_reconnectAttempts;
    LOG_INFO("BtleHub", QStringLiteral("Reconnect attempt ") + QString::number(m_reconnectAttempts));
    // Go through attemptConnection() (not connectToDevice) so the running
    // attempt count is preserved and MAX_RECONNECT_ATTEMPTS actually caps.
    attemptConnection();
}

void BtleHub::onControllerError(QLowEnergyController::Error error)
{
    const QString errStr = m_controller ? m_controller->errorString()
                                        : QStringLiteral("controller error");
    LOG_WARN("BtleHub",
             QStringLiteral("BLE controller error ") + QString::number(static_cast<int>(error))
             + QStringLiteral(": ") + errStr);

    // A connect-phase failure (BlueZ connect timeout / adapter busy) is often
    // transient. Retry a few times before surfacing the error to the UI rather
    // than failing the workout on the first miss — onControllerDisconnected only
    // auto-reconnects drops that happen AFTER a successful connect.
    const bool connectPhase =
        !m_controller
        || (m_controller->state() != QLowEnergyController::DiscoveredState
            && m_controller->state() != QLowEnergyController::ConnectedState);

    if (connectPhase && !m_userDisconnect
        && m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
        if (m_reconnectTimer && !m_reconnectTimer->isActive())
            m_reconnectTimer->start(RECONNECT_INTERVAL_MS);
        return;
    }

    emit connectionError(errStr);
}

void BtleHub::onServiceDiscovered(const QBluetoothUuid &serviceUuid)
{
    LOG_DEBUG("BtleHub", QStringLiteral("Service discovered: ") + serviceUuid.toString());

    if (serviceUuid == BtleUuid::HeartRate) {
        m_hrService = m_controller->createServiceObject(serviceUuid, this);
        if (m_hrService)
            setupService(m_hrService);
    }
    else if (serviceUuid == BtleUuid::CyclingSpeedCadence) {
        m_cscService = m_controller->createServiceObject(serviceUuid, this);
        if (m_cscService)
            setupService(m_cscService);
    }
    else if (serviceUuid == BtleUuid::CyclingPower) {
        m_powerService = m_controller->createServiceObject(serviceUuid, this);
        if (m_powerService)
            setupService(m_powerService);
    }
    else if (serviceUuid == BtleUuid::FitnessMachine) {
        m_ftmsService = m_controller->createServiceObject(serviceUuid, this);
        if (m_ftmsService) {
            // Handle the one-shot read of the Fitness Machine Feature (0x2ACC)
            // so we learn whether Set Target Resistance Level (0x04) is supported.
            connect(m_ftmsService, &QLowEnergyService::characteristicRead,
                    this, &BtleHub::onCharacteristicChanged);
            setupService(m_ftmsService);
        }
    }
    else if (serviceUuid == BtleUuid::MoxyService) {
        m_moxyService = m_controller->createServiceObject(serviceUuid, this);
        if (m_moxyService)
            setupService(m_moxyService);
    }
    else if (serviceUuid == BtleUuid::BatteryService) {
        m_batteryService = m_controller->createServiceObject(serviceUuid, this);
        if (m_batteryService) {
            // Also handle characteristicRead for the initial one-shot read
            connect(m_batteryService, &QLowEnergyService::characteristicRead,
                    this, &BtleHub::onCharacteristicChanged);
            setupService(m_batteryService);
        }
    }
}

void BtleHub::onDiscoveryFinished()
{
    LOG_INFO("BtleHub", QStringLiteral("Service discovery finished"));
    if (m_ftmsService)
        m_ftmsSetupWatchdog->start(FTMS_SETUP_TIMEOUT_MS);
    emit serviceDiscoveryFinished();
    emit deviceConnected();
}

// ─────────────────────────────────────────────────────────────────────────────
// Service helpers
// ─────────────────────────────────────────────────────────────────────────────
void BtleHub::setupService(QLowEnergyService *service)
{
    connect(service, &QLowEnergyService::stateChanged,
            this, &BtleHub::onServiceStateChanged);
    connect(service, &QLowEnergyService::characteristicChanged,
            this, &BtleHub::onCharacteristicChanged);
    connect(service, &QLowEnergyService::descriptorWritten,
            this, &BtleHub::onDescriptorWritten);
    connect(service, &QLowEnergyService::errorOccurred,
            this, [this, service](QLowEnergyService::ServiceError error) {
                LOG_WARN("BtleHub", QStringLiteral("Service error %1 on %2")
                                        .arg(int(error))
                                        .arg(service->serviceUuid().toString()));
                if (service != m_ftmsService)
                    return;
                if (error == QLowEnergyService::CharacteristicWriteError) {
                    m_ftmsControl->onWriteFailed();
                } else if (!m_ftmsControl->isReady()) {
                    // The Control Point never got set up on this connection
                    // (seen on Windows: details discovery fails with error 4),
                    // so control can never be granted: reconnect soon.
                    m_ftmsSetupWatchdog->start(FTMS_SETUP_RETRY_MS);
                }
            });

    if (service->state() == QLowEnergyService::RemoteService)
        service->discoverDetails();
}

void BtleHub::enableNotification(QLowEnergyService *service,
                                  const QLowEnergyCharacteristic &characteristic)
{
    if (!characteristic.isValid())
        return;

    QLowEnergyDescriptor cccd =
        characteristic.descriptor(BtleUuid::ClientCharacteristicConfig);
    if (cccd.isValid())
        service->writeDescriptor(cccd, QByteArray::fromHex("0100")); // enable notifications
}

void BtleHub::enableIndication(QLowEnergyService *service,
                               const QLowEnergyCharacteristic &characteristic)
{
    if (!characteristic.isValid())
        return;

    QLowEnergyDescriptor cccd =
        characteristic.descriptor(BtleUuid::ClientCharacteristicConfig);
    if (cccd.isValid())
        service->writeDescriptor(cccd, QByteArray::fromHex("0200")); // enable indications
}

void BtleHub::onDescriptorWritten(const QLowEnergyDescriptor &descriptor,
                                  const QByteArray &value)
{
    // The FTMS Control Point requires indications (CCCD = 0x0200) to be
    // enabled before the trainer will process commands; Request Control is
    // therefore only sent once that descriptor write is confirmed.
    if (m_ftmsService &&
        descriptor.uuid() == BtleUuid::ClientCharacteristicConfig &&
        value == QByteArray::fromHex("0200")) {
        LOG_INFO("BtleHub", QStringLiteral("FTMS Control Point indications enabled"));
        m_ftmsControl->setReady();
    }
}

void BtleHub::parseFtmsFeature(const QByteArray &value)
{
    // 0x2ACC: [Fitness Machine Features (uint32 LE)][Target Setting Features (uint32 LE)]
    if (value.size() < 8)
        return;
    const quint32 targetFeatures =
          static_cast<quint32>(quint8(value[4]))
        | (static_cast<quint32>(quint8(value[5])) << 8)
        | (static_cast<quint32>(quint8(value[6])) << 16)
        | (static_cast<quint32>(quint8(value[7])) << 24);
    // Bit 2 = Resistance Target Setting Supported (i.e. Set Target Resistance
    // Level, opcode 0x04 — the channel that gives instant, gear-like feel).
    m_resistanceLevelSupported = (targetFeatures & (1u << 2)) != 0;
    LOG_INFO("BtleHub", QStringLiteral("FTMS resistance-level (0x04) control %1")
                            .arg(m_resistanceLevelSupported ? QStringLiteral("supported")
                                                            : QStringLiteral("not supported")));
    emit signal_resistanceLevelSupported(m_resistanceLevelSupported);
}

void BtleHub::onServiceStateChanged(QLowEnergyService::ServiceState state)
{
    QLowEnergyService *service = qobject_cast<QLowEnergyService*>(sender());
    if (!service)
        return;

    if (state != QLowEnergyService::RemoteServiceDiscovered)
        return;

    // Enable notifications for every measurement characteristic we care about
    if (service == m_hrService) {
        enableNotification(service,
            service->characteristic(BtleUuid::HeartRateMeasurement));
    }
    else if (service == m_cscService) {
        enableNotification(service,
            service->characteristic(BtleUuid::CSCMeasurement));
    }
    else if (service == m_powerService) {
        enableNotification(service,
            service->characteristic(BtleUuid::CyclingPowerMeasurement));
    }
    else if (service == m_ftmsService) {
        enableNotification(service,
            service->characteristic(BtleUuid::FtmsIndoorBikeData));
        // Rowers expose Rower Data (0x2AD1) on the same FTMS service instead
        // of Indoor Bike Data; subscribe to whichever the device provides.
        enableNotification(service,
            service->characteristic(BtleUuid::FtmsRowerData));
        // Control Point needs INDICATIONS, and Request Control must wait for
        // the descriptor write to be confirmed (see onDescriptorWritten) —
        // trainers ignore/refuse control commands on an unconfigured control
        // point, which left ERG mode stuck at the trainer's default load.
        enableIndication(service,
            service->characteristic(BtleUuid::FtmsControlPoint));
        // Machine Status tells us when the trainer drops our control or resets.
        enableNotification(service,
            service->characteristic(BtleUuid::FtmsMachineStatus));
        // One-shot read of the feature char to learn if Set Target Resistance
        // Level (0x04) is supported (drives virtual-shifting mode selection).
        QLowEnergyCharacteristic feat = service->characteristic(BtleUuid::FtmsFeature);
        if (feat.isValid())
            service->readCharacteristic(feat);
    }
    else if (service == m_moxyService) {
        enableNotification(service,
            service->characteristic(BtleUuid::MoxyMeasurement));
    }
    else if (service == m_batteryService) {
        QLowEnergyCharacteristic battChar =
            service->characteristic(BtleUuid::BatteryLevel);
        if (battChar.isValid()) {
            // Enable notifications if the device supports them
            enableNotification(service, battChar);
            // Also do an immediate read for instant level on connect
            service->readCharacteristic(battChar);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Characteristic notification dispatcher
// ─────────────────────────────────────────────────────────────────────────────
void BtleHub::onCharacteristicChanged(const QLowEnergyCharacteristic &characteristic,
                                      const QByteArray &value)
{
    const QBluetoothUuid uuid = characteristic.uuid();

    if (uuid == BtleUuid::HeartRateMeasurement)
        parseHrMeasurement(value);
    else if (uuid == BtleUuid::CSCMeasurement)
        parseCscMeasurement(value);
    else if (uuid == BtleUuid::CyclingPowerMeasurement)
        parsePowerMeasurement(value);
    else if (uuid == BtleUuid::FtmsIndoorBikeData)
        parseFtmsIndoorBikeData(value);
    else if (uuid == BtleUuid::FtmsRowerData)
        parseFtmsRowerData(value);
    else if (uuid == BtleUuid::FtmsControlPoint)
        m_ftmsControl->onControlPointIndication(value);
    else if (uuid == BtleUuid::FtmsMachineStatus)
        m_ftmsControl->onMachineStatus(value);
    else if (uuid == BtleUuid::FtmsFeature)
        parseFtmsFeature(value);
    else if (uuid == BtleUuid::MoxyMeasurement)
        parseMoxyMeasurement(value);
    else if (uuid == BtleUuid::BatteryLevel)
        parseBatteryLevel(value);
}

// ─────────────────────────────────────────────────────────────────────────────
// Parsers
// ─────────────────────────────────────────────────────────────────────────────

// Heart Rate Measurement (0x2A37)
// Byte 0: flags  bit0=HR-format(0=uint8,1=uint16)  bit4=RR-interval present
// Byte 1(+2): HR value in bpm
void BtleHub::parseHrMeasurement(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    quint8 flags = static_cast<quint8>(data[0]);
    int hr = 0;

    if (flags & 0x01) {
        // 16-bit HR value
        if (data.size() < 3)
            return;
        hr = static_cast<quint8>(data[1]) | (static_cast<quint8>(data[2]) << 8);
    } else {
        // 8-bit HR value
        if (data.size() < 2)
            return;
        hr = static_cast<quint8>(data[1]);
    }

    emit signal_hr(m_userID, hr);

    // A trainer that bridges an HR strap exposes it as a 0x180D service on the
    // trainer connection, so the first reading here flags the capability for the
    // "provided by trainer" UI (the trainer hub is the only listener).
    if (!m_hrSeen) {
        m_hrSeen = true;
        emit signal_trainerProvidesHr(m_userID);
    }
}

// CSC Measurement (0x2A5B) – speed/cadence computation
// Byte 0: flags  bit0=wheel data present  bit1=crank data present
// If wheel: bytes 1-4 cumulative wheel revs (uint32), bytes 5-6 last event time (uint16, 1/1024s)
// If crank: bytes 7-8 (or 1-2) cumul crank revs (uint16), bytes 9-10 (or 3-4) last event time
void BtleHub::parseCscMeasurement(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    quint8 flags = static_cast<quint8>(data[0]);
    bool wheelPresent = (flags & 0x01) != 0;
    bool crankPresent = (flags & 0x02) != 0;

    int offset = 1;

    quint32 wheelRevs = 0;
    quint16 wheelTime = 0;
    quint16 crankRevs = 0;
    quint16 crankTime = 0;

    if (wheelPresent) {
        if (data.size() < offset + 6)
            return;
        wheelRevs  = static_cast<quint8>(data[offset])
                   | (static_cast<quint8>(data[offset+1]) << 8)
                   | (static_cast<quint8>(data[offset+2]) << 16)
                   | (static_cast<quint8>(data[offset+3]) << 24);
        wheelTime  = static_cast<quint8>(data[offset+4])
                   | (static_cast<quint8>(data[offset+5]) << 8);
        offset += 6;
    }

    if (crankPresent) {
        if (data.size() < offset + 4)
            return;
        m_cscCadenceSeen = true;
        crankRevs = static_cast<quint8>(data[offset])
                  | (static_cast<quint8>(data[offset+1]) << 8);
        crankTime = static_cast<quint8>(data[offset+2])
                  | (static_cast<quint8>(data[offset+3]) << 8);
    }

    // Cadence (RPM) – same rollover-safe math as speed/cadence controller
    // Wheel and crank data each need their own baseline: a trainer may send
    // wheel-only packets first, and counting from 0 reads as thousands of rpm.
    if (crankPresent && m_crankBaselineSet) {
        quint16 deltaCrankTime = crankTime - m_lastCrankEventTime; // rollover safe (uint16)
        quint16 deltaCrankRevs = crankRevs - m_lastCrankRevolutions;

        if (deltaCrankTime > 0) {
            // cadence = revs * 60 * 1024 / deltaTime
            // Use quint64 to avoid overflow when deltaCrankRevs is large
            const quint64 numerator = static_cast<quint64>(deltaCrankRevs) * 60ULL * 1024ULL;
            const quint64 raw = numerator / static_cast<quint64>(deltaCrankTime);
            const int cadence = static_cast<int>(qMin(raw, static_cast<quint64>(std::numeric_limits<int>::max())));
            if (cadence <= MAX_PLAUSIBLE_CADENCE_RPM)
                emit signal_cadence(m_userID, cadence);
            else
                LOG_WARN("BtleHub", QStringLiteral("Implausible cadence %1 rpm ignored (%2 revs in %3/1024 s)")
                                        .arg(cadence).arg(deltaCrankRevs).arg(deltaCrankTime));
        } else if (deltaCrankRevs == 0) {
            emit signal_cadence(m_userID, 0);
        }
    }
    if (crankPresent) {
        m_lastCrankRevolutions = crankRevs;
        m_lastCrankEventTime   = crankTime;
        m_crankBaselineSet     = true;
    }

    // Speed (km/h)
    if (wheelPresent && m_wheelBaselineSet) {
        quint16 deltaWheelTime = wheelTime - m_lastWheelEventTime;
        quint32 deltaWheelRevs = wheelRevs - m_lastWheelRevolutions;

        if (deltaWheelTime > 0) {
            // speed (m/s) = circumference(m) * revs / (deltaTime / 1024)
            double speedMs = (static_cast<double>(m_wheelCircMm) / 1000.0)
                           * deltaWheelRevs
                           * 1024.0
                           / deltaWheelTime;
            double speedKmh = speedMs * 3.6;
            emit signal_speed(m_userID, speedKmh);
        } else if (deltaWheelRevs == 0) {
            emit signal_speed(m_userID, 0.0);
        }
    }
    if (wheelPresent) {
        m_lastWheelRevolutions = wheelRevs;
        m_lastWheelEventTime   = wheelTime;
        m_wheelBaselineSet     = true;
    }

    // Restart the stop-timer so we zero-out values if messages cease
    m_cscStopTimer->start(CSC_STOP_TIMEOUT_MS);
}

// Cycling Power Measurement (0x2A63)
// Bytes 0-1: flags (16-bit)
// Bytes 2-3: Instantaneous Power (int16, watts)
// Byte 4   : Pedal Power Balance (uint8, 1/2 %, valid 0-200) — present when flag bit 0 is set
void BtleHub::parsePowerMeasurement(const QByteArray &data)
{
    if (data.size() < 4)
        return;

    quint16 flags = static_cast<quint8>(data[0])
                  | (static_cast<quint8>(data[1]) << 8);

    qint16 power = static_cast<qint16>(
        static_cast<quint8>(data[2]) | (static_cast<quint8>(data[3]) << 8));

    m_cpsPowerSeen = true;
    emit signal_power(m_userID, static_cast<int>(power));

    // Flag bit 0: Pedal Power Balance present. Value is uint8 at byte 4 with
    // 1/2 % resolution. Bit 1 is the reference (1 = Left); meters that set it
    // report the left contribution, so the right pedal share is 100 - value.
    // We treat the unknown-reference case as left too (the common convention).
    if ((flags & 0x0001) && data.size() >= 5 && static_cast<quint8>(data[4]) <= 200) {
        int leftPercentage  = qRound(static_cast<quint8>(data[4]) * 0.5);
        int rightPercentage = 100 - leftPercentage;
        emit signal_balance(m_userID, rightPercentage);
    }
}

// FTMS Indoor Bike Data (0x2AD2)
// Flags (2 bytes) indicate which fields are present; we read what we can.
// Field order (all little-endian):
//   Instantaneous Speed  (uint16, 0.01 km/h) – flag bit 0 = NOT present (inverted!)
//   Average Speed        (uint16) – bit 1
//   Instantaneous Cadence (uint16, 0.5 rpm) – bit 2 = present
//   Average Cadence      (uint16) – bit 3
//   Total Distance       (uint24) – bit 4
//   Resistance Level     (int16) – bit 5
//   Instantaneous Power  (int16, 1 W) – bit 6 = present
//   Average Power        (int16) – bit 7
//   Expended Energy      (uint16+uint16+uint8) – bit 8
//   Heart Rate           (uint8) – bit 9
//   Metabolic Equivalent (uint8) – bit 10
//   Elapsed Time         (uint16) – bit 11
//   Remaining Time       (uint16) – bit 12
void BtleHub::parseFtmsIndoorBikeData(const QByteArray &data)
{
    if (data.size() < 2)
        return;

    quint16 flags = static_cast<quint8>(data[0])
                  | (static_cast<quint8>(data[1]) << 8);

    int offset = 2;

    // A field the flags announce but the packet is too short to hold ends
    // parsing: emitting it as 0 would fake a stop and trip the auto-pause.
    auto hasU16 = [&]() { return offset + 2 <= data.size(); };
    auto readU16 = [&]() -> quint16 {
        quint16 v = static_cast<quint8>(data[offset])
                  | (static_cast<quint8>(data[offset+1]) << 8);
        offset += 2;
        return v;
    };

    // Bit 0: More Data (Instantaneous Speed NOT present when set)
    if (!(flags & 0x0001)) {
        if (!hasU16()) return;
        emit signal_speed(m_userID, readU16() * 0.01);
    }

    // Bit 1: Average Speed present
    if (flags & 0x0002) offset += 2;

    // Bit 2: Instantaneous Cadence present when set
    if (flags & 0x0004) {
        if (!hasU16()) return;
        const int cadence = static_cast<int>(readU16() * 0.5);
        if (!m_cscCadenceSeen)
            emit signal_cadence(m_userID, cadence);
    }

    // Bit 3: Average Cadence present
    if (flags & 0x0008) offset += 2;

    // Bit 4: Total Distance present (uint24)
    if (flags & 0x0010) offset += 3;

    // Bit 5: Resistance Level present
    if (flags & 0x0020) offset += 2;

    // Bit 6: Instantaneous Power present when set
    if (flags & 0x0040) {
        if (!hasU16()) return;
        qint16 power = static_cast<qint16>(readU16());
        if (!m_cpsPowerSeen)
            emit signal_power(m_userID, static_cast<int>(power));
    }
}

// FTMS Rower Data (0x2AD1) — the rowing counterpart of Indoor Bike Data.
// Emits stroke rate on the cadence channel (SPM), pace converted to km/h on
// the speed channel (the dashboard formats it back to /500m in rowing mode),
// and instantaneous power on the power channel.
void BtleHub::parseFtmsRowerData(const QByteArray &data)
{
    if (data.size() < 2)
        return;

    quint16 flags = static_cast<quint8>(data[0])
                  | (static_cast<quint8>(data[1]) << 8);

    int offset = 2;

    auto readU8 = [&]() -> quint8 {
        if (offset + 1 > data.size()) return 0;
        return static_cast<quint8>(data[offset++]);
    };
    auto readU16 = [&]() -> quint16 {
        if (offset + 2 > data.size()) return 0;
        quint16 v = static_cast<quint8>(data[offset])
                  | (static_cast<quint8>(data[offset+1]) << 8);
        offset += 2;
        return v;
    };

    // Bit 0: More Data — Stroke Rate (u8, 0.5 spm) + Stroke Count (u16)
    // present when CLEAR.
    if (!(flags & 0x0001)) {
        emit signal_cadence(m_userID, static_cast<int>(readU8() * 0.5));
        offset += 2; // stroke count
    }

    // Bit 1: Average Stroke Rate (u8)
    if (flags & 0x0002) offset += 1;

    // Bit 2: Total Distance (u24, metres)
    if (flags & 0x0004) offset += 3;

    // Bit 3: Instantaneous Pace (u16, seconds per 500 m; 0xFFFF = invalid)
    if (flags & 0x0008) {
        quint16 paceSec = readU16();
        if (paceSec > 0 && paceSec != 0xFFFF)
            emit signal_speed(m_userID, 1800.0 / paceSec); // km/h equivalent
        else
            emit signal_speed(m_userID, 0.0);
    }

    // Bit 4: Average Pace (u16)
    if (flags & 0x0010) offset += 2;

    // Bit 5: Instantaneous Power (s16, watts)
    if (flags & 0x0020) {
        qint16 power = static_cast<qint16>(readU16());
        emit signal_power(m_userID, static_cast<int>(power));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Zero-out cadence & speed when messages stop arriving
// ─────────────────────────────────────────────────────────────────────────────
void BtleHub::onCscStopTimer()
{
    emit signal_cadence(m_userID, 0);
    emit signal_speed(m_userID, 0.0);
}

// Moxy Muscle Oxygen Measurement (0xAAB2)
// Bytes 0-1: flags (uint16 LE) – bit0=SmO2 present, bit1=tHb present
// Bytes 2-3: SmO2 (uint16 LE, units 0.1 %)
// Bytes 4-5: tHb  (uint16 LE, units 0.01 g/dL)
void BtleHub::parseMoxyMeasurement(const QByteArray &data)
{
    if (data.size() < 6)
        return;

    quint16 flags = static_cast<quint8>(data[0])
                  | (static_cast<quint8>(data[1]) << 8);
    quint16 rawSmo2 = static_cast<quint8>(data[2])
                    | (static_cast<quint8>(data[3]) << 8);
    quint16 rawThb  = static_cast<quint8>(data[4])
                    | (static_cast<quint8>(data[5]) << 8);

    double smo2 = (flags & 0x01) ? rawSmo2 / 10.0  : 0.0;
    double thb  = (flags & 0x02) ? rawThb  / 100.0 : 0.0;

    emit signal_oxygen(m_userID, smo2, thb);
}

// Battery Level (0x2A19)
// Single byte: battery percentage 0–100
void BtleHub::parseBatteryLevel(const QByteArray &data)
{
    if (data.isEmpty())
        return;

    int percentage = static_cast<quint8>(data[0]);
    percentage = qBound(0, percentage, 100);

    emit signal_battery(determineSensorType(), percentage);
}

// Return a human-readable sensor type based on which services are connected.
QString BtleHub::determineSensorType() const
{
    if (m_hrService)      return QStringLiteral("Heart Rate");
    if (m_powerService)   return QStringLiteral("Power");
    if (m_ftmsService)    return QStringLiteral("Trainer");
    if (m_cscService)     return QStringLiteral("Speed/Cadence");
    if (m_moxyService)    return QStringLiteral("Oxygen");
    return QStringLiteral("Sensor");
}
