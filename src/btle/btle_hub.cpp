#include "btle_hub.h"
#include "btle_uuids.h"
#include "zwift/zwift_click_relay.h"
#include "zwift/zwift_protocol.h"

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
// Zwift proprietary trainer service (FC82) – relays a linked Zwift Click v2.
static const QBluetoothUuid ZwiftService          (QString::fromLatin1(ZwiftProtocol::Uuid::Service));

// Characteristics
static const QBluetoothUuid HeartRateMeasurement    (QBluetoothUuid::CharacteristicType::HeartRateMeasurement);
static const QBluetoothUuid CSCMeasurement          (QBluetoothUuid::CharacteristicType::CSCMeasurement);
static const QBluetoothUuid CyclingPowerMeasurement (QBluetoothUuid::CharacteristicType::CyclingPowerMeasurement);
static const QBluetoothUuid FtmsIndoorBikeData      (quint16(0x2AD2));
static const QBluetoothUuid FtmsControlPoint        (quint16(0x2AD9));
static const QBluetoothUuid FtmsFeature             (quint16(0x2ACC));
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

    // Frees the FTMS control point if a response indication never arrives
    // (marginal signal, trainer quirk) so commands don't queue forever.
    m_ftmsOpTimeout = new QTimer(this);
    m_ftmsOpTimeout->setSingleShot(true);
    connect(m_ftmsOpTimeout, &QTimer::timeout, this, [this]() {
        if (!m_ftmsOpInFlight)
            return;
        LOG_WARN("BtleHub", QStringLiteral("FTMS response indication timed out — releasing control point"));
        m_ftmsOpInFlight = false;
        if (m_ftmsControlGranted && !m_ftmsPendingCmd.isEmpty()) {
            const QByteArray next = m_ftmsPendingCmd;
            m_ftmsPendingCmd.clear();
            writeFtmsCommandNow(next);
        }
    });
}

BtleHub::~BtleHub()
{
    disconnectFromDevice();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void BtleHub::setWheelCircumferenceMm(int mm)
{
    if (mm > 0)
        m_wheelCircMm = mm;
}

void BtleHub::connectToDevice(const QBluetoothDeviceInfo &device)
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
    // Keep the Click relay across reconnects so the workout's button wiring stays
    // valid; just detach it from the service we're about to delete (it re-attaches
    // when FC82 is re-discovered). The relay (parented to this) is freed with us.
    if (m_clickRelay)
        m_clickRelay->detach();
    delete m_zwiftService;    m_zwiftService    = nullptr;

    m_firstCscMeasurement   = true;
    m_ftmsControlRequested  = false;
    m_ftmsControlGranted    = false;
    m_ftmsOpInFlight        = false;
    m_ftmsPendingCmd.clear();
    m_ftmsLastSentCmd.clear();
    m_ftmsLastAckedCmd.clear();
    m_reconnectDevice   = device;
    m_reconnectAttempts = 0;

    m_controller = QLowEnergyController::createCentral(device, this);

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
void BtleHub::setLoad(int antID, double watts)
{
    // In Studio mode each rider's trainer only honours its own target.
    if (m_filterControlByUserID && antID != m_userID)
        return;

    if (!m_ftmsService)
        return;

    // FTMS: Set Target Power (opcode 0x05), value = int16 watts
    qint16 w = static_cast<qint16>(qBound(-32768.0, watts, 32767.0));
    QByteArray cmd(3, '\0');
    cmd[0] = 0x05;                      // Set Target Power opcode
    cmd[1] = static_cast<char>(w & 0xFF);
    cmd[2] = static_cast<char>((w >> 8) & 0xFF);
    sendFtmsCommand(cmd);
}

void BtleHub::setSlope(int antID, double grade)
{
    if (m_filterControlByUserID && antID != m_userID)
        return;

    if (!m_ftmsService)
        return;

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
    sendFtmsCommand(cmd);
}

void BtleHub::setResistanceLevel(int antID, int levelTenths)
{
    if (m_filterControlByUserID && antID != m_userID)
        return;

    if (!m_ftmsService)
        return;

    // FTMS: Set Target Resistance Level (opcode 0x04), value = int16 in 0.1 units
    // (same representation as the Supported Resistance Level Range, 0x2AD6).
    qint16 lvl = static_cast<qint16>(qBound(-32768, levelTenths, 32767));
    QByteArray cmd(3, '\0');
    cmd[0] = 0x04;                          // Set Target Resistance Level opcode
    cmd[1] = static_cast<char>(lvl & 0xFF);
    cmd[2] = static_cast<char>((lvl >> 8) & 0xFF);
    sendFtmsCommand(cmd);
}

// One control-point op in flight at a time: real trainers reject overlapping
// writes with ATT error 0x80, and every op completes only when its response
// indication ([0x80, opcode, result]) arrives.  Commands issued while waiting
// (or before control is granted) are deferred; a newer one replaces an older.
void BtleHub::sendFtmsCommand(const QByteArray &cmd)
{
    // Identical to what the trainer already confirmed, with nothing newer on
    // the wire or queued — re-sending would be pure BLE noise (the ERG
    // smoothing layer re-emits unchanged targets every second during ramps).
    if (cmd == m_ftmsLastAckedCmd && !m_ftmsOpInFlight && m_ftmsPendingCmd.isEmpty()) {
        LOG_DEBUG("BtleHub", QStringLiteral("FTMS duplicate of confirmed op 0x%1 skipped")
                                 .arg(quint8(cmd[0]), 2, 16, QLatin1Char('0')));
        return;
    }
    if (!m_ftmsControlGranted || m_ftmsOpInFlight) {
        m_ftmsPendingCmd = cmd;
        return;
    }
    writeFtmsCommandNow(cmd);
}

void BtleHub::writeFtmsCommandNow(const QByteArray &cmd)
{
    if (!m_ftmsService)
        return;
    QLowEnergyCharacteristic cp =
        m_ftmsService->characteristic(BtleUuid::FtmsControlPoint);
    if (!cp.isValid())
        return;

    m_ftmsOpInFlight  = true;
    m_ftmsLastSentCmd = cmd;
    m_ftmsService->writeCharacteristic(cp, cmd,
                                       QLowEnergyService::WriteWithResponse);
    LOG_DEBUG("BtleHub", QStringLiteral("FTMS op 0x%1 sent (%2 bytes)")
                             .arg(quint8(cmd[0]), 2, 16, QLatin1Char('0'))
                             .arg(cmd.size()));
    if (m_ftmsOpTimeout)
        m_ftmsOpTimeout->start(FTMS_OP_TIMEOUT_MS);
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
    m_controller->discoverServices();
}

void BtleHub::onControllerDisconnected()
{
    LOG_INFO("BtleHub", QStringLiteral("Device disconnected"));
    m_cscStopTimer->stop();
    m_hrSeen = false;
    emit deviceDisconnected();

    if (m_reconnectAttempts < MAX_RECONNECT_ATTEMPTS)
        m_reconnectTimer->start(RECONNECT_INTERVAL_MS);
}

void BtleHub::onReconnectTimer()
{
    LOG_INFO("BtleHub", QStringLiteral("Reconnect attempt ") + QString::number(m_reconnectAttempts + 1));
    ++m_reconnectAttempts;
    connectToDevice(m_reconnectDevice);
}

void BtleHub::onControllerError(QLowEnergyController::Error error)
{
    const QString errStr = m_controller->errorString();
    LOG_WARN("BtleHub",
             QStringLiteral("BLE controller error ") + QString::number(static_cast<int>(error))
             + QStringLiteral(": ") + errStr);
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
    else if (serviceUuid == BtleUuid::ZwiftService) {
        // Zwift Click v2 read through the trainer's FC82 relay. The relay drives
        // its own setup/notifications on this service (BtleHub doesn't parse it).
        m_zwiftService = m_controller->createServiceObject(serviceUuid, this);
        if (m_zwiftService) {
            if (!m_clickRelay)
                m_clickRelay = new ZwiftClickRelay(this);
            m_clickRelay->attachService(m_zwiftService);
            LOG_INFO("BtleHub", QStringLiteral("Zwift FC82 found — Click relay enabled"));
        }
    }
}

void BtleHub::onDiscoveryFinished()
{
    LOG_INFO("BtleHub", QStringLiteral("Service discovery finished"));
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
                // A failed control-point write never gets a response
                // indication — free the slot now (instead of waiting for the
                // timeout) and let the freshest queued command retry.  The
                // failed payload is NOT marked confirmed, so an identical
                // re-send passes the duplicate filter.
                if (service == m_ftmsService &&
                    error == QLowEnergyService::CharacteristicWriteError &&
                    m_ftmsOpInFlight) {
                    m_ftmsOpInFlight = false;
                    if (m_ftmsOpTimeout) m_ftmsOpTimeout->stop();
                    if (m_ftmsPendingCmd.isEmpty())
                        m_ftmsPendingCmd = m_ftmsLastSentCmd;   // retry the failed op
                    if (m_ftmsControlGranted && !m_ftmsPendingCmd.isEmpty()) {
                        const QByteArray next = m_ftmsPendingCmd;
                        m_ftmsPendingCmd.clear();
                        writeFtmsCommandNow(next);
                    }
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

void BtleHub::requestFtmsControl()
{
    if (!m_ftmsService || m_ftmsControlRequested)
        return;

    QLowEnergyCharacteristic cp =
        m_ftmsService->characteristic(BtleUuid::FtmsControlPoint);
    if (!cp.isValid())
        return;

    // Opcode 0x00 = Request Control
    m_ftmsOpInFlight = true;
    m_ftmsService->writeCharacteristic(cp, QByteArray(1, '\x00'),
                                       QLowEnergyService::WriteWithResponse);
    m_ftmsControlRequested = true;
    if (m_ftmsOpTimeout)
        m_ftmsOpTimeout->start(FTMS_OP_TIMEOUT_MS);
    LOG_INFO("BtleHub", QStringLiteral("FTMS Request Control sent"));
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
        requestFtmsControl();
    }
}

void BtleHub::handleFtmsControlPointResponse(const QByteArray &value)
{
    // Indication format: [0x80, request opcode, result code]
    // result 0x01 = success; anything else is a refusal (0x02 not supported,
    // 0x03 invalid parameter, 0x04 operation failed, 0x05 control not permitted).
    if (value.size() < 3 || quint8(value[0]) != 0x80)
        return;

    const quint8 requestOp = quint8(value[1]);
    const quint8 result    = quint8(value[2]);

    // Whatever the verdict, the op is complete — the control point is free.
    m_ftmsOpInFlight = false;
    if (m_ftmsOpTimeout)
        m_ftmsOpTimeout->stop();

    if (result != 0x01) {
        LOG_WARN("BtleHub", QStringLiteral("FTMS control point refused opcode 0x%1 (result 0x%2)")
                                .arg(requestOp, 2, 16, QLatin1Char('0'))
                                .arg(result, 2, 16, QLatin1Char('0')));
        if (requestOp == 0x00) {
            m_ftmsControlGranted = false;
            return;
        }
        // A refused target falls through: the pending (newer) command still
        // deserves its attempt.
    } else if (requestOp == 0x00) {   // Request Control granted
        m_ftmsControlGranted = true;
        LOG_INFO("BtleHub", QStringLiteral("FTMS control granted by trainer"));
    } else {
        m_ftmsLastAckedCmd = m_ftmsLastSentCmd;
        LOG_DEBUG("BtleHub", QStringLiteral("FTMS op 0x%1 acknowledged by trainer")
                                 .arg(requestOp, 2, 16, QLatin1Char('0')));
    }

    // Send the command that queued up while this op was in flight (a target
    // issued before the grant, or one that superseded an older write).
    if (m_ftmsControlGranted && !m_ftmsPendingCmd.isEmpty()) {
        const QByteArray next = m_ftmsPendingCmd;
        m_ftmsPendingCmd.clear();
        writeFtmsCommandNow(next);
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
        // Control Point needs INDICATIONS, and Request Control must wait for
        // the descriptor write to be confirmed (see onDescriptorWritten) —
        // trainers ignore/refuse control commands on an unconfigured control
        // point, which left ERG mode stuck at the trainer's default load.
        enableIndication(service,
            service->characteristic(BtleUuid::FtmsControlPoint));
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
    else if (uuid == BtleUuid::FtmsControlPoint)
        handleFtmsControlPointResponse(value);
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
        crankRevs = static_cast<quint8>(data[offset])
                  | (static_cast<quint8>(data[offset+1]) << 8);
        crankTime = static_cast<quint8>(data[offset+2])
                  | (static_cast<quint8>(data[offset+3]) << 8);
    }

    if (m_firstCscMeasurement) {
        m_lastWheelRevolutions = wheelRevs;
        m_lastWheelEventTime   = wheelTime;
        m_lastCrankRevolutions = crankRevs;
        m_lastCrankEventTime   = crankTime;
        m_firstCscMeasurement  = false;
        return;
    }

    // Cadence (RPM) – same rollover-safe math as speed/cadence controller
    if (crankPresent) {
        quint16 deltaCrankTime = crankTime - m_lastCrankEventTime; // rollover safe (uint16)
        quint16 deltaCrankRevs = crankRevs - m_lastCrankRevolutions;

        if (deltaCrankTime > 0) {
            // cadence = revs * 60 * 1024 / deltaTime
            // Use quint64 to avoid overflow when deltaCrankRevs is large
            const quint64 numerator = static_cast<quint64>(deltaCrankRevs) * 60ULL * 1024ULL;
            const quint64 raw = numerator / static_cast<quint64>(deltaCrankTime);
            const int cadence = static_cast<int>(qMin(raw, static_cast<quint64>(std::numeric_limits<int>::max())));
            emit signal_cadence(m_userID, cadence);
        } else if (deltaCrankRevs == 0) {
            emit signal_cadence(m_userID, 0);
        }

        m_lastCrankRevolutions = crankRevs;
        m_lastCrankEventTime   = crankTime;
    }

    // Speed (km/h)
    if (wheelPresent) {
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

        m_lastWheelRevolutions = wheelRevs;
        m_lastWheelEventTime   = wheelTime;
    }

    // Restart the stop-timer so we zero-out values if messages cease
    m_cscStopTimer->start(CSC_STOP_TIMEOUT_MS);
}

// Cycling Power Measurement (0x2A63)
// Bytes 0-1: flags (16-bit)
// Bytes 2-3: Instantaneous Power (int16, watts)
// Byte 4   : Pedal Power Balance (uint8, 1/2 %) — present when flag bit 0 is set
void BtleHub::parsePowerMeasurement(const QByteArray &data)
{
    if (data.size() < 4)
        return;

    quint16 flags = static_cast<quint8>(data[0])
                  | (static_cast<quint8>(data[1]) << 8);

    qint16 power = static_cast<qint16>(
        static_cast<quint8>(data[2]) | (static_cast<quint8>(data[3]) << 8));

    emit signal_power(m_userID, static_cast<int>(power));

    // Flag bit 0: Pedal Power Balance present. Value is uint8 at byte 4 with
    // 1/2 % resolution. Bit 1 is the reference (1 = Left); meters that set it
    // report the left contribution, so the right pedal share is 100 - value.
    // We treat the unknown-reference case as left too (the common convention).
    if ((flags & 0x0001) && data.size() >= 5) {
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
//   Instantaneous Cadence (uint16, 0.5 rpm) – bit 2 = NOT present
//   Average Cadence      (uint16) – bit 3
//   Total Distance       (uint24) – bit 4
//   Resistance Level     (int16) – bit 5
//   Instantaneous Power  (int16, 1 W) – bit 6 = NOT present
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

    // Helper: read little-endian uint16 and advance offset
    auto readU16 = [&]() -> quint16 {
        if (offset + 2 > data.size()) return 0;
        quint16 v = static_cast<quint8>(data[offset])
                  | (static_cast<quint8>(data[offset+1]) << 8);
        offset += 2;
        return v;
    };

    // Bit 0: More Data (Instantaneous Speed NOT present when set)
    if (!(flags & 0x0001)) {
        emit signal_speed(m_userID, readU16() * 0.01);
    }

    // Bit 1: Average Speed present
    if (flags & 0x0002) offset += 2;

    // Bit 2: Instantaneous Cadence NOT present when set
    if (!(flags & 0x0004)) {
        emit signal_cadence(m_userID, static_cast<int>(readU16() * 0.5));
    }

    // Bit 3: Average Cadence present
    if (flags & 0x0008) offset += 2;

    // Bit 4: Total Distance present (uint24)
    if (flags & 0x0010) offset += 3;

    // Bit 5: Resistance Level present
    if (flags & 0x0020) offset += 2;

    // Bit 6: Instantaneous Power NOT present when set
    if (!(flags & 0x0040)) {
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
