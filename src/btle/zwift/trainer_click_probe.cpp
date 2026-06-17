#include "trainer_click_probe.h"

#include "zwift_click_protocol.h"
#include "zwift_click_relay.h"
#include "virtual_gear.h"
#include "logger.h"

#include <QBluetoothUuid>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyDescriptor>
#include <QTimer>

namespace {
const QBluetoothUuid FtmsService  (quint16(0x1826));
const QBluetoothUuid FtmsControlPt (quint16(0x2AD9));
QBluetoothUuid uuid(const char *s) { return QBluetoothUuid(QString::fromLatin1(s)); }
const QBluetoothUuid Cccd(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
} // namespace

TrainerClickProbe::TrainerClickProbe(QObject *parent) : QObject(parent) {}

void TrainerClickProbe::start(const QString &nameFilter, int runSeconds)
{
    m_nameFilter = nameFilter;
    LOG_WARN("ClickProbe", QStringLiteral("── trainer Click probe (proven relay, armed after FTMS grant) ── filter=%1  %2s")
                 .arg(m_nameFilter).arg(runSeconds));
    LOG_WARN("ClickProbe", QStringLiteral("watch: FTMS GRANTED + 0x04 ACKs (ERG) AND relay LIVE + button/shift (Click)"));

    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(0);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &TrainerClickProbe::onDeviceDiscovered);
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);

    m_runTimer = new QTimer(this);
    m_runTimer->setSingleShot(true);
    connect(m_runTimer, &QTimer::timeout, this, [this]() {
        LOG_WARN("ClickProbe", QStringLiteral("── done — FTMS granted=%1  relay=%2  presses=%3 ──")
                     .arg(m_ftmsGranted).arg(m_relayActive ? "LIVE" : "SILENT").arg(m_buttonPresses));
        emit finished();
    });
    m_runTimer->start(runSeconds * 1000);
}

void TrainerClickProbe::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    if (m_started || !info.name().contains(m_nameFilter, Qt::CaseInsensitive))
        return;
    m_started = true;
    m_agent->stop();
    LOG_WARN("ClickProbe", QStringLiteral("connecting to %1 [%2]")
                 .arg(info.name(), info.address().toString()));
    m_ctrl = QLowEnergyController::createCentral(info, this);
    connect(m_ctrl, &QLowEnergyController::connected, this,
            [this]() { m_ctrl->discoverServices(); });
    connect(m_ctrl, &QLowEnergyController::discoveryFinished,
            this, &TrainerClickProbe::onDiscoveryFinished);
    m_ctrl->connectToDevice();
}

void TrainerClickProbe::onDiscoveryFinished()
{
    m_ftms = m_ctrl->createServiceObject(FtmsService, this);
    m_fc82 = m_ctrl->createServiceObject(uuid(ZwiftClickProtocol::Uuid::Service), this);
    LOG_WARN("ClickProbe", QStringLiteral("services: FTMS=%1  FC82=%2")
                 .arg(m_ftms ? "yes" : "NO").arg(m_fc82 ? "yes" : "NO"));
    if (m_ftms) {
        connect(m_ftms, &QLowEnergyService::stateChanged, this,
                [this](QLowEnergyService::ServiceState st) {
                    if (st == QLowEnergyService::RemoteServiceDiscovered) setupFtms();
                });
        connect(m_ftms, &QLowEnergyService::characteristicChanged,
                this, &TrainerClickProbe::onFtmsChanged);
        connect(m_ftms, &QLowEnergyService::descriptorWritten, this,
                [this](const QLowEnergyDescriptor &, const QByteArray &) { requestFtmsControl(); });
        m_ftms->discoverDetails();
    }
    // FC82 is handed to ZwiftClickRelay only after the FTMS grant (armRelay()).
}

void TrainerClickProbe::setupFtms()
{
    const QLowEnergyCharacteristic cp = m_ftms->characteristic(FtmsControlPt);
    const QLowEnergyDescriptor cccd = cp.descriptor(Cccd);
    if (cccd.isValid())
        m_ftms->writeDescriptor(cccd, QByteArray::fromHex("0200"));  // → requestFtmsControl
    else
        requestFtmsControl();
}

void TrainerClickProbe::requestFtmsControl()
{
    static bool sent = false;
    if (sent) return;
    sent = true;
    const QLowEnergyCharacteristic cp = m_ftms->characteristic(FtmsControlPt);
    m_ftms->writeCharacteristic(cp, QByteArray(1, '\x00'), QLowEnergyService::WriteWithResponse);
    LOG_WARN("ClickProbe", QStringLiteral("FTMS Request Control sent"));

    m_ergTimer = new QTimer(this);
    connect(m_ergTimer, &QTimer::timeout, this, [this]() {
        if (m_ftmsGranted) sendResistance(VirtualGear::resistanceLevel(m_gear));
    });
    m_ergTimer->start(3000);

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, [this]() {
        LOG_WARN("ClickProbe", QStringLiteral("· status: relay=%1  gear=%2/24  presses=%3  ERG-acks=%4")
                     .arg(m_relayActive ? "LIVE" : "SILENT").arg(m_gear).arg(m_buttonPresses).arg(m_ergAcks));
    });
    m_statusTimer->start(30000);
}

void TrainerClickProbe::sendResistance(int levelTenths)
{
    const QLowEnergyCharacteristic cp = m_ftms->characteristic(FtmsControlPt);
    QByteArray cmd(3, '\0');
    cmd[0] = 0x04;                                   // Set Target Resistance Level
    cmd[1] = char(levelTenths & 0xFF);
    cmd[2] = char((levelTenths >> 8) & 0xFF);
    m_ftms->writeCharacteristic(cp, cmd, QLowEnergyService::WriteWithResponse);
}

void TrainerClickProbe::onFtmsChanged(const QLowEnergyCharacteristic &, const QByteArray &v)
{
    if (v.size() < 3 || quint8(v[0]) != 0x80)
        return;
    const quint8 op = quint8(v[1]), res = quint8(v[2]);
    if (op == 0x00 && res == 0x01) {
        m_ftmsGranted = true;
        LOG_WARN("ClickProbe", QStringLiteral("FTMS control GRANTED — ERG channel is alive"));
        armRelay();   // arm the relay only now, so ERG is established first
    } else if (op == 0x04) {
        if (res == 0x01) {
            ++m_ergAcks;
            if (m_logNextAcks > 0) {
                --m_logNextAcks;
                LOG_WARN("ClickProbe", QStringLiteral("  ↳ FTMS 0x04 ACK — gear %1 resistance ACCEPTED (relay %2)")
                             .arg(m_gear).arg(m_relayActive ? "live" : "silent"));
            }
        } else {
            LOG_WARN("ClickProbe", QStringLiteral("FTMS 0x04 REFUSED (res=0x%1)").arg(res, 2, 16, QLatin1Char('0')));
        }
    }
}

void TrainerClickProbe::armRelay()
{
    if (m_clickRelay || !m_fc82) return;
    m_clickRelay = new ZwiftClickRelay(this);

    connect(m_clickRelay, &ZwiftClickRelay::relayActiveChanged, this, [this](bool active) {
        m_relayActive = active;
        LOG_WARN("ClickProbe", active ? QStringLiteral("✅ relay LIVE — ▶▶▶ press the Click ◀◀◀")
                                      : QStringLiteral("relay SILENT — watchdog will re-link"));
    });
    connect(m_clickRelay, &ZwiftClickRelay::paddleUpPressed,   this, [this]() { shift(+1); });
    connect(m_clickRelay, &ZwiftClickRelay::paddleDownPressed, this, [this]() { shift(-1); });
    auto logBtn = [this](const QString &b) {
        ++m_buttonPresses;
        LOG_WARN("ClickProbe", QStringLiteral("CLICK %1 (relayed via trainer)").arg(b));
    };
    connect(m_clickRelay, &ZwiftClickRelay::buttonAPressed, this, [logBtn]() { logBtn("A"); });
    connect(m_clickRelay, &ZwiftClickRelay::buttonBPressed, this, [logBtn]() { logBtn("B"); });
    connect(m_clickRelay, &ZwiftClickRelay::buttonYPressed, this, [logBtn]() { logBtn("Y"); });
    connect(m_clickRelay, &ZwiftClickRelay::buttonZPressed, this, [logBtn]() { logBtn("Z"); });
    connect(m_clickRelay, &ZwiftClickRelay::dpadLeftPressed,  this, [logBtn]() { logBtn("d-pad ◀"); });
    connect(m_clickRelay, &ZwiftClickRelay::dpadUpPressed,    this, [logBtn]() { logBtn("d-pad ▲"); });
    connect(m_clickRelay, &ZwiftClickRelay::dpadRightPressed, this, [logBtn]() { logBtn("d-pad ▶"); });
    connect(m_clickRelay, &ZwiftClickRelay::dpadDownPressed,  this, [logBtn]() { logBtn("d-pad ▼"); });

    LOG_WARN("ClickProbe", QStringLiteral("arming ZwiftClickRelay on FC82 — WAKE the Click so the trainer announces it…"));
    m_clickRelay->attachService(m_fc82);
}

void TrainerClickProbe::shift(int delta)
{
    m_gear = qBound(1, m_gear + delta, VirtualGear::Count);
    ++m_buttonPresses;
    const int level = VirtualGear::resistanceLevel(m_gear);
    if (m_ftmsGranted) { sendResistance(level); m_logNextAcks = 1; }
    LOG_WARN("ClickProbe", QStringLiteral("SHIFT %1 → gear %2/24 → FTMS resistance level %3 sent")
                 .arg(delta > 0 ? "UP" : "DOWN").arg(m_gear).arg(level));
}
