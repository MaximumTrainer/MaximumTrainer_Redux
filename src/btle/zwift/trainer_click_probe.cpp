#include "trainer_click_probe.h"

#include "zwift_click_protocol.h"
#include "logger.h"

#include <QBluetoothUuid>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyDescriptor>
#include <QTimer>

namespace {
const QBluetoothUuid FtmsService   (quint16(0x1826));
const QBluetoothUuid FtmsControlPt  (quint16(0x2AD9));
QBluetoothUuid uuid(const char *s) { return QBluetoothUuid(QString::fromLatin1(s)); }
const QBluetoothUuid Cccd(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);

// The trainer's ZCS relay setup (NO RideOn, NO 441002 connect). From the Zwift
// capture (handle 83 writes). 441002 is sent only AFTER the trainer announces
// the Click (a 0x47 frame), per the proven recipe in notes/zwift-cog-…md.
const char *const kSetupSteps[] = {
    "410805", "000800", "442001", "440801", "410805", "000810", "0008800a", "450801",
};
const char kRideOn[]        = "526964654f6e0203";
const char kConnectClick[]  = "441002";
const char kRelayedRideOn[] = "4e08021208526964654f6e0203";
const QByteArray kClickName = QByteArrayLiteral("Zwift Click");
} // namespace

TrainerClickProbe::TrainerClickProbe(QObject *parent) : QObject(parent) {}

void TrainerClickProbe::start(const QString &nameFilter, int runSeconds, Relay relay)
{
    m_nameFilter = nameFilter;
    m_relay = relay;
    const char *mode = relay == Relay::Passive ? "PASSIVE (subscribe only, no FC82 writes)"
                     : relay == Relay::Zcs     ? "ZCS (connect Click, NO RideOn)"
                                               : "RIDEON (full original relay arming)";
    LOG_WARN("ClickProbe", QStringLiteral("── trainer Click probe ── filter=%1  mode=%2  %3s")
                 .arg(m_nameFilter, QString::fromLatin1(mode)).arg(runSeconds));
    LOG_WARN("ClickProbe", QStringLiteral("watch for: 'FTMS control GRANTED' + '0x04 ACK' (ERG alive) AND 'CLICK button' (relay works)"));

    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(0);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &TrainerClickProbe::onDeviceDiscovered);
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);

    m_runTimer = new QTimer(this);
    m_runTimer->setSingleShot(true);
    connect(m_runTimer, &QTimer::timeout, this, [this]() {
        LOG_WARN("ClickProbe", QStringLiteral("── done — FTMS granted=%1 ──").arg(m_ftmsGranted));
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
    if (m_fc82) {
        connect(m_fc82, &QLowEnergyService::stateChanged, this,
                [this](QLowEnergyService::ServiceState st) {
                    if (st == QLowEnergyService::RemoteServiceDiscovered) setupFc82();
                });
        connect(m_fc82, &QLowEnergyService::characteristicChanged,
                this, &TrainerClickProbe::onFc82Changed);
        m_fc82->discoverDetails();
    }
}

void TrainerClickProbe::setupFtms()
{
    const QLowEnergyCharacteristic cp = m_ftms->characteristic(FtmsControlPt);
    const QLowEnergyDescriptor cccd = cp.descriptor(Cccd);
    if (cccd.isValid())
        m_ftms->writeDescriptor(cccd, QByteArray::fromHex("0200"));  // → triggers requestFtmsControl
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
    // Periodically push a resistance target so we can see ongoing ERG-channel acks.
    m_ergTimer = new QTimer(this);
    connect(m_ergTimer, &QTimer::timeout, this, [this]() {
        static int lvl = 20;
        lvl = (lvl == 20) ? 40 : 20;   // toggle so each command is a fresh value
        if (m_ftmsGranted) sendResistance(lvl);
    });
    m_ergTimer->start(3000);
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
        armRelay();   // only now arm the relay, so ERG is established first
    } else if (op == 0x04) {
        LOG_WARN("ClickProbe", QStringLiteral("FTMS 0x04 resistance %1 — ERG still actuating")
                     .arg(res == 0x01 ? "ACK" : "refused"));
    } else {
        LOG_WARN("ClickProbe", QStringLiteral("FTMS resp op=0x%1 res=0x%2")
                     .arg(op,2,16,QLatin1Char('0')).arg(res,2,16,QLatin1Char('0')));
    }
}

void TrainerClickProbe::setupFc82()
{
    // Subscribe notify (0002) AND indicate (0004) — both are CCCD writes, NOT
    // control commands. The proven recipe subscribes both before arming.
    const QLowEnergyCharacteristic meas =
        m_fc82->characteristic(uuid(ZwiftClickProtocol::Uuid::Measurement));
    const QLowEnergyDescriptor mcccd = meas.descriptor(Cccd);
    if (mcccd.isValid())
        m_fc82->writeDescriptor(mcccd, QByteArray::fromHex("0100"));
    const QLowEnergyCharacteristic resp =
        m_fc82->characteristic(uuid(ZwiftClickProtocol::Uuid::Response));
    const QLowEnergyDescriptor rcccd = resp.descriptor(Cccd);
    if (rcccd.isValid())
        m_fc82->writeDescriptor(rcccd, QByteArray::fromHex("0200"));
    LOG_WARN("ClickProbe", QStringLiteral("FC82 notify+indicate subscribed (no control write yet)"));
    // If FTMS isn't present, arm immediately; otherwise armRelay() runs after grant.
    if (!m_ftms) armRelay();
}

void TrainerClickProbe::armRelay()
{
    static bool armed = false;
    if (armed || !m_fc82) return;
    armed = true;
    if (m_relay == Relay::Passive) {
        LOG_WARN("ClickProbe", QStringLiteral("PASSIVE — writing nothing to FC82; press Click buttons…"));
        return;
    }
    if (m_relay == Relay::RideOn)
        writeFc82(QByteArray::fromHex(kRideOn));
    int delay = 0;
    for (const char *hex : kSetupSteps) {
        const QByteArray b = QByteArray::fromHex(hex);
        delay += 150;
        QTimer::singleShot(delay, this, [this, b]() { writeFc82(b); });
    }
    LOG_WARN("ClickProbe", QStringLiteral("relay setup sent (mode=%1) — WAKE the Click so the trainer announces it…")
                 .arg(m_relay == Relay::RideOn ? "RideOn+ZCS" : "ZCS-only"));
}

void TrainerClickProbe::writeFc82(const QByteArray &bytes)
{
    if (!m_fc82) return;
    const QLowEnergyCharacteristic cp = m_fc82->characteristic(uuid(ZwiftClickProtocol::Uuid::ControlPoint));
    if (!cp.isValid()) return;
    const auto mode = (cp.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                          ? QLowEnergyService::WriteWithoutResponse
                          : QLowEnergyService::WriteWithResponse;
    m_fc82->writeCharacteristic(cp, bytes, mode);
}

void TrainerClickProbe::onFc82Changed(const QLowEnergyCharacteristic &, const QByteArray &v)
{
    // Diagnostics: show what the trainer is actually streaming on FC82 so we can
    // tell whether the Click is being relayed at all. 0x47 = device announce
    // (carries "Zwift Click"), 0x4e = relayed Click frame, 0x03 = riding data.
    const quint8 cmd = v.isEmpty() ? 0 : quint8(v.at(0));
    if (++m_fc82Frames <= 15 || cmd == 0x47 || cmd == 0x4e)
        LOG_WARN("ClickProbe", QStringLiteral("FC82 rx [%1]: %2")
                     .arg(m_fc82Frames).arg(QString::fromLatin1(v.toHex())));

    // Trainer announced the linked Click (0x47 device frame carrying its name) →
    // NOW connect it (441002), then relay RideOn ~4.5 s later (per the recipe).
    if (!m_connectSent && m_relay != Relay::Passive
        && cmd == 0x47 && v.contains(kClickName)) {
        m_connectSent = true;
        LOG_WARN("ClickProbe", QStringLiteral("Click announced by trainer → connecting (441002)"));
        writeFc82(QByteArray::fromHex(kConnectClick));
        QTimer::singleShot(4500, this, [this]() {
            LOG_WARN("ClickProbe", QStringLiteral("relaying RideOn to the Click…"));
            writeFc82(QByteArray::fromHex(kRelayedRideOn));
        });
    }

    quint32 bitmap;
    if (!ZwiftClickProtocol::decodeRelayedClickButtons(v, bitmap))
        return;   // riding data / device frame / not a relayed button frame
    const quint32 changed = bitmap ^ m_lastBitmap;
    m_lastBitmap = bitmap;
    for (int bit = 0; bit < 32; ++bit) {
        if ((changed & (1u << bit)) && ZwiftClickProtocol::clickButtonPressed(bitmap, bit))
            LOG_WARN("ClickProbe", QStringLiteral("CLICK button bit %1 pressed (relayed via trainer)").arg(bit));
    }
}
