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
    m_retriesLeft = MAX_RETRIES;

    if (!m_quietTimer) {
        m_quietTimer = new QTimer(this);
        m_quietTimer->setSingleShot(true);
        m_quietTimer->setInterval(STREAM_QUIET_MS);
        connect(m_quietTimer, &QTimer::timeout, this, [this]() {
            m_streamQuiet = true;
            LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: stream QUIET (no frame for %3 ms after %4 frames) — link still up")
                         .arg(m_name, deviceAddress()).arg(STREAM_QUIET_MS).arg(m_frameCount));
        });
    }

    startController();
}

void ZwiftClickHub::startController()
{
    if (m_controller) { m_controller->deleteLater(); m_controller = nullptr; }
    m_controller = QLowEnergyController::createCentral(m_device, this);

    connect(m_controller, &QLowEnergyController::connected, this,
            [this]() { m_controller->discoverServices(); });
    connect(m_controller, &QLowEnergyController::disconnected, this, [this]() {
        if (m_quietTimer) m_quietTimer->stop();
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

    // Subscribe to the button stream.
    const QLowEnergyCharacteristic meas =
        m_service->characteristic(uuid(ZwiftClickProtocol::Uuid::Measurement));
    if (meas.isValid()) {
        const QLowEnergyDescriptor cccd = meas.descriptor(
            QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
        if (cccd.isValid())
            m_service->writeDescriptor(cccd, kNotifyOn);
    }

    // RideOn handshake — makes the (unencrypted) shifter start streaming. It's a
    // WriteWithoutResponse (fire-and-forget) that can be silently lost when both
    // controllers connect at once, so send it a few times to reliably start the
    // stream (idempotent — extra RideOns are harmless once it's streaming).
    sendRideOn();
    QTimer::singleShot(500,  this, [this]() { sendRideOn(); });
    QTimer::singleShot(1200, this, [this]() { sendRideOn(); });

    LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: CONNECTED").arg(m_name, deviceAddress()));
    if (m_quietTimer) m_quietTimer->start();   // expect the stream to begin
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
        return;  // not a 0x23 button frame (handshake echo, capability, etc.)

    ++m_frameCount;
    if (m_streamQuiet) {
        m_streamQuiet = false;
        LOG_WARN("ZwiftClick", QStringLiteral("%1 [%2]: stream RESUMED").arg(m_name, deviceAddress()));
    }
    if (m_quietTimer) m_quietTimer->start();   // reset quiet detector

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
