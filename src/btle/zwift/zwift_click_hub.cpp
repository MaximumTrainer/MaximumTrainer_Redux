#include "zwift_click_hub.h"

#include "zwift_click_protocol.h"
#include "logger.h"

#include <QBluetoothUuid>
#include <QDateTime>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyDescriptor>
#include <QTimer>

namespace {
const QByteArray kNotifyOn = QByteArray::fromHex("0100");

QBluetoothUuid uuid(const char *s) { return QBluetoothUuid(QString::fromLatin1(s)); }
} // namespace

ZwiftClickHub::ZwiftClickHub(QObject *parent) : QObject(parent) {}

ZwiftClickHub::~ZwiftClickHub() { teardown(); }

QString ZwiftClickHub::deviceAddress() const
{
#ifdef Q_OS_MACOS
    return m_device.deviceUuid().toString(QUuid::WithoutBraces);
#else
    return m_device.address().toString();
#endif
}

bool ZwiftClickHub::isConnected() const
{
    return m_controller
        && m_controller->state() == QLowEnergyController::DiscoveredState;
}

void ZwiftClickHub::teardown()
{
    if (m_watchdog) m_watchdog->stop();
    if (m_service) { m_service->deleteLater(); m_service = nullptr; }
    if (m_controller) {
        m_controller->disconnectFromDevice();
        m_controller->deleteLater();
        m_controller = nullptr;
    }
}

void ZwiftClickHub::disconnectFromDevice() { teardown(); }

void ZwiftClickHub::connectToDevice(const QBluetoothDeviceInfo &device)
{
    teardown();
    m_device      = device;
    m_name        = device.name();
    m_lastBitmap  = 0xFFFFFFFFu;
    m_streamQuiet = false;
    m_frameCount  = 0;
    m_lastFrameMs = QDateTime::currentMSecsSinceEpoch();
    m_lastKickMs  = 0;
    m_retriesLeft = MAX_RETRIES;

    if (!m_watchdog) {
        m_watchdog = new QTimer(this);
        m_watchdog->setInterval(WATCHDOG_TICK_MS);
        connect(m_watchdog, &QTimer::timeout, this, [this]() { watchdogTick(); });
    }

    startController();
}

// Recover a stream that stopped while the BLE link stayed up: re-kick RideOn
// once it goes quiet, and force a full reconnect if it stays silent past STALL.
void ZwiftClickHub::watchdogTick()
{
    if (!isConnected())
        return;
    const qint64 now    = QDateTime::currentMSecsSinceEpoch();
    const qint64 silent = now - m_lastFrameMs;

    if (silent >= STALL_MS) {
        LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: stalled %3 ms — reconnecting")
                     .arg(m_name, deviceAddress()).arg(silent));
        m_streamQuiet  = true;
        m_lastFrameMs  = now;   // reset so we give the reconnect time before re-stalling
        startController();      // re-handshakes; does not emit our disconnected()
        return;
    }

    // EXPERIMENT (left Click v2 keepalive): a healthy controller never goes silent
    // this long (re-kicks bring it back within ~3 s); sustained silence = the
    // left unit locking up. Send RESET (0x18) to refresh it, as BikeControl does.
    if (silent >= RESET_AFTER_MS && now - m_lastResetMs >= RESET_AFTER_MS) {
        m_lastResetMs = now;
        LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: silent %3 ms — sending RESET (0x18) to refresh")
                     .arg(m_name, deviceAddress()).arg(silent));
        sendReset();
    }

    if (silent >= STREAM_QUIET_MS) {
        if (!m_streamQuiet) {
            m_streamQuiet = true;
            LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: stream QUIET (no frame for %3 ms after %4 frames) — re-kicking")
                         .arg(m_name, deviceAddress()).arg(silent).arg(m_frameCount));
        }
        if (now - m_lastKickMs >= KICK_EVERY_MS) {
            m_lastKickMs = now;
            sendRideOn();   // idempotent — restart a stalled (or sleeping) stream
        }
    }

    // Periodic unlock-ack keepalive (BikeControl sends ff0400 on an unlocked
    // device). Harmless if locked; may keep an unlocked Click's session alive.
    if (now - m_lastAckMs >= ACK_EVERY_MS) {
        m_lastAckMs = now;
        sendUnlockAck();
    }
}

void ZwiftClickHub::sendUnlockAck()
{
    if (!m_service)
        return;
    const QLowEnergyCharacteristic cp =
        m_service->characteristic(uuid(ZwiftClickProtocol::Uuid::ControlPoint));
    if (!cp.isValid())
        return;
    const auto mode = (cp.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                          ? QLowEnergyService::WriteWithoutResponse
                          : QLowEnergyService::WriteWithResponse;
    m_service->writeCharacteristic(cp, QByteArray::fromHex("ff0400"), mode);
}

void ZwiftClickHub::startController()
{
    if (m_controller) {
        // A watchdog reconnect tears the old controller down; detach its signals
        // first so its dying disconnected() does not propagate to the manager
        // (which would delete this hub mid-reconnect).
        disconnect(m_controller, nullptr, this, nullptr);
        m_controller->disconnectFromDevice();
        m_controller->deleteLater();
        m_controller = nullptr;
    }
    m_controller = QLowEnergyController::createCentral(m_device, this);

    connect(m_controller, &QLowEnergyController::connected, this,
            [this]() { m_controller->discoverServices(); });
    connect(m_controller, &QLowEnergyController::disconnected, this, [this]() {
        if (m_watchdog) m_watchdog->stop();
        LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: DISCONNECTED")
                     .arg(m_name, deviceAddress()));
        emit disconnected();
    });
    connect(m_controller,
            static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(
                &QLowEnergyController::errorOccurred),
            this, [this](QLowEnergyController::Error err) {
                const QString es = m_controller ? m_controller->errorString() : QStringLiteral("error");
                // A connect-phase error is often transient — retry a few times.
                if (m_controller && m_controller->state() != QLowEnergyController::DiscoveredState
                        && m_retriesLeft > 0) {
                    --m_retriesLeft;
                    LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: controller error %3 (%4) — retrying (%5 left)")
                                 .arg(m_name, deviceAddress()).arg(int(err)).arg(es).arg(m_retriesLeft));
                    QTimer::singleShot(1500, this, [this]() { startController(); });
                    return;
                }
                LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: controller error %3 (%4) — giving up")
                             .arg(m_name, deviceAddress()).arg(int(err)).arg(es));
                emit connectionError(es);
            });
    connect(m_controller, &QLowEnergyController::discoveryFinished, this, [this]() {
        m_service = m_controller->createServiceObject(
            uuid(ZwiftClickProtocol::Uuid::Service), this);
        if (!m_service) {
            emit connectionError(QStringLiteral("Zwift service (0xFC82) not found"));
            return;
        }
        connect(m_service, &QLowEnergyService::stateChanged, this,
                [this](QLowEnergyService::ServiceState st) {
                    if (st == QLowEnergyService::RemoteServiceDiscovered)
                        setupService();
                });
        m_service->discoverDetails();
    });

    m_controller->connectToDevice();
}

void ZwiftClickHub::setupService()
{
    connect(m_service, &QLowEnergyService::characteristicChanged, this,
            &ZwiftClickHub::onCharacteristicChanged);

    // Subscribe to the button stream (ASYNC notify) AND the SYNC_TX indicate —
    // BikeControl subscribes both before the handshake.
    const QLowEnergyCharacteristic meas =
        m_service->characteristic(uuid(ZwiftClickProtocol::Uuid::Measurement));
    if (meas.isValid()) {
        const QLowEnergyDescriptor cccd = meas.descriptor(
            QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
        if (cccd.isValid())
            m_service->writeDescriptor(cccd, kNotifyOn);
    }
    const QLowEnergyCharacteristic resp =
        m_service->characteristic(uuid(ZwiftClickProtocol::Uuid::Response));
    if (resp.isValid()) {
        const QLowEnergyDescriptor cccd = resp.descriptor(
            QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
        if (cccd.isValid())
            m_service->writeDescriptor(cccd, QByteArray::fromHex("0200"));   // indications
    }

    // RideOn handshake — makes the (unencrypted) shifter start streaming. It's a
    // WriteWithoutResponse (fire-and-forget) that can be silently lost when both
    // controllers connect at once, so send it a few times to reliably start the
    // stream (idempotent — extra RideOns are harmless once it's streaming).
    sendRideOn();
    QTimer::singleShot(500,  this, [this]() { sendRideOn(); });
    QTimer::singleShot(1200, this, [this]() { sendRideOn(); });
    // BikeControl's Click-v2 "unlock ack" — sent on an UNLOCKED device to keep the
    // session alive (no effect if the device is still locked). Send it shortly
    // after RideOn, then periodically as a keepalive.
    QTimer::singleShot(1600, this, [this]() { sendUnlockAck(); });

    LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: CONNECTED").arg(m_name, deviceAddress()));
    m_lastFrameMs = QDateTime::currentMSecsSinceEpoch();   // give the stream time before the watchdog acts
    m_lastKickMs  = 0;
    if (m_watchdog) m_watchdog->start();
    emit connected();
}

void ZwiftClickHub::sendRideOn()
{
    if (!m_service)
        return;
    const QLowEnergyCharacteristic cp =
        m_service->characteristic(uuid(ZwiftClickProtocol::Uuid::ControlPoint));
    if (!cp.isValid())
        return;
    const auto mode = (cp.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                          ? QLowEnergyService::WriteWithoutResponse
                          : QLowEnergyService::WriteWithResponse;
    m_service->writeCharacteristic(cp, ZwiftClickProtocol::rideOnHandshake(), mode);
}

void ZwiftClickHub::sendReset()
{
    // Zwift Opcode.RESET = 24 (0x18), single byte to the control point. BikeControl
    // uses this to "restart" the left Click v2 (~every minute) to keep it from
    // locking up. Experiment: send it when a controller goes silent to refresh it.
    if (!m_service)
        return;
    const QLowEnergyCharacteristic cp =
        m_service->characteristic(uuid(ZwiftClickProtocol::Uuid::ControlPoint));
    if (!cp.isValid())
        return;
    const auto mode = (cp.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                          ? QLowEnergyService::WriteWithoutResponse
                          : QLowEnergyService::WriteWithResponse;
    m_service->writeCharacteristic(cp, QByteArray::fromHex("18"), mode);
}

void ZwiftClickHub::onCharacteristicChanged(const QLowEnergyCharacteristic &,
                                            const QByteArray &value)
{
    // ANY notification keeps the stream alive — when idle the controller sends
    // periodic 0x19/0x15 heartbeat frames (Makinolo, "Zwift Ride protocol"), not
    // the 0x23 button frame. Feed the watchdog on every frame so normal idle is
    // never mistaken for a stall (which would trigger a needless reconnect).
    ++m_frameCount;
    m_lastFrameMs = QDateTime::currentMSecsSinceEpoch();
    if (m_streamQuiet) {
        m_streamQuiet = false;
        LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: stream RESUMED").arg(m_name, deviceAddress()));
    }

    quint32 bitmap;
    if (!ZwiftClickProtocol::decodeClickButtons(value, bitmap))
        return;  // idle/heartbeat frame (0x19/0x15) — liveness only, no buttons

    const quint32 changed = bitmap ^ m_lastBitmap;
    if (!changed)
        return;

    emit frameChanged(bitmap);   // diagnostic: every DOWN/UP edge, pre-debounce

    for (int bit = 0; bit < 32; ++bit) {
        if (!(changed & (1u << bit)))
            continue;
        if (ZwiftClickProtocol::clickButtonPressed(bitmap, bit)) {
            emit buttonPressed(bit);
            if (bit == ZwiftClick::UpShiftBit)
                emit shiftRequested(+1);
            else if (bit == ZwiftClick::DownShiftBit)
                emit shiftRequested(-1);
        } else {
            emit buttonReleased(bit);
        }
    }
    m_lastBitmap = bitmap;
}
