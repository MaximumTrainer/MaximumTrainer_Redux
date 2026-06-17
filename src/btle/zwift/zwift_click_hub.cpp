#include "zwift_click_hub.h"

#include "zwift_click_protocol.h"
#include "logger.h"

#include <QBluetoothUuid>
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
    m_tearingDown = true;   // a disconnect from here is deliberate, not a drop
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
    m_retriesLeft = MAX_RETRIES;
    m_tearingDown = false;

    startController();
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
        if (m_tearingDown) {
            LOG_INFO("ZwiftClick", QStringLiteral("%1 [%2]: disconnected (teardown)").arg(m_name, deviceAddress()));
            emit disconnected();
            return;
        }
        // Unexpected drop (common when sharing the BLE adapter with the trainer).
        // Reconnect directly to the known device — much faster than tearing down
        // and waiting for a fresh scan to re-discover it. The controller will
        // complete the connection as soon as the Click is back in range/awake.
        LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: dropped — reconnecting directly")
                     .arg(m_name, deviceAddress()));
        m_retriesLeft = MAX_RETRIES;
        QTimer::singleShot(300, this, [this]() { if (!m_tearingDown) startController(); });
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
                // Out of direct-reconnect attempts: hand back to the manager so it
                // removes us and lets discovery re-find + reconnect the device when
                // it next advertises (avoids a dead-end where we never retry again).
                LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: controller error %3 (%4) — handing back to discovery")
                             .arg(m_name, deviceAddress()).arg(int(err)).arg(es));
                emit disconnected();
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

    // RideOn handshake, ONCE (like BikeControl). A WriteWithoutResponse can be
    // silently lost when both controllers connect at once, so send it a few times
    // at startup to reliably begin button notifications. We do NOT re-kick
    // periodically afterwards: the device stays connected at the link layer and
    // sends a frame on each button press, and a periodic re-kick write only adds
    // radio contention (and drops) when sharing the adapter with the trainer.
    sendRideOn();
    QTimer::singleShot(500,  this, [this]() { sendRideOn(); });
    QTimer::singleShot(1200, this, [this]() { sendRideOn(); });

    LOG_INFO("ZwiftClick", QStringLiteral("%1 [%2]: CONNECTED").arg(m_name, deviceAddress()));
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

void ZwiftClickHub::onCharacteristicChanged(const QLowEnergyCharacteristic &,
                                            const QByteArray &value)
{
    quint32 bitmap;
    if (!ZwiftClickProtocol::decodeClickButtons(value, bitmap))
        return;  // idle/heartbeat frame (0x19/0x15) — not a button frame

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
