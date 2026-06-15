#include "trainer_gear_test.h"
#include "btle_hub.h"
#include "virtual_gear.h"

#include <cstdio>

namespace {
void line(const QString &s)
{
    const QByteArray u = s.toUtf8();
    fprintf(stdout, "%s\n", u.constData());
    fflush(stdout);
}
} // namespace

TrainerGearTest::TrainerGearTest(QObject *parent) : QObject(parent) {}

void TrainerGearTest::start(const QString &nameFilter, int scanSeconds)
{
    line(QStringLiteral("── Trainer gear test (via BtleHub = real FTMS actuation) ──"));
    line(QStringLiteral("  looking for: %1   (auto-shifts; just pedal — no input needed)")
             .arg(nameFilter));

    m_hub = new BtleHub(this);
    connect(m_hub, &BtleHub::signal_cadence, this,
            [this](int, int c) { m_cadence = c; });
    connect(m_hub, &BtleHub::connectionError, this,
            [](const QString &e) { line(QStringLiteral("  [error] %1").arg(e)); });
    connect(m_hub, &BtleHub::serviceDiscoveryFinished, this, [this]() {
        if (m_started)
            return;
        m_started = true;
        line(QStringLiteral("  trainer connected — FTMS control will engage; auto gears starting"));
        m_tick = new QTimer(this);
        connect(m_tick, &QTimer::timeout, this, &TrainerGearTest::onGearTick);
        m_tick->start(1000);
    });

    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(scanSeconds * 1000);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this,
            [this, nameFilter](const QBluetoothDeviceInfo &info) {
                if (m_connecting || !info.name().contains(nameFilter, Qt::CaseInsensitive))
                    return;
                m_connecting = true;
                line(QStringLiteral("  found %1 [%2] — connecting")
                         .arg(info.name(), info.address().toString()));
                m_agent->stop();
                m_hub->connectToDevice(info);
            });
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished, this, [this]() {
        if (!m_connecting) {
            line(QStringLiteral("  trainer not found — awake & in range?"));
            emit finished();
        }
    });
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

// Cadence-aware gear → watts via the shared model (same feel as the in-app
// feature). FTP unknown in this standalone test → a sensible default is used.
int TrainerGearTest::gearWatts(int gear, double cadence) const
{
    return VirtualGear::targetWatts(gear, cadence, /*ftp=*/220.0);
}

void TrainerGearTest::onGearTick()
{
    ++m_elapsed;
    const int t = m_elapsed;

    int gear;
    QString label;
    if      (t < 25) { gear = 5;  label = QStringLiteral("WARMUP — get on & pedal steady (easy)"); }
    else if (t < 37) { gear = 3;  label = QStringLiteral("EASY  (gear 3)  → should feel light"); }
    else if (t < 49) { gear = 13; label = QStringLiteral("HARD  (gear 13) → should feel heavy"); }
    else if (t < 61) { gear = 3;  label = QStringLiteral("EASY  (gear 3)  → light again"); }
    else if (t < 73) { gear = 13; label = QStringLiteral("HARD  (gear 13) → heavy again"); }
    else if (t < 85) { gear = 8;  label = QStringLiteral("MID   (gear 8)"); }
    else {
        line(QStringLiteral("  releasing resistance — done"));
        if (m_hub) m_hub->setLoad(1, 0);
        if (m_tick) m_tick->stop();
        QTimer::singleShot(800, this, [this]() { emit finished(); });
        return;
    }

    if (gear != m_lastGear) {
        m_lastGear = gear;
        line(QStringLiteral("  ▶ %1   (cad=%2, target=%3W)")
                 .arg(label).arg(int(m_cadence)).arg(gearWatts(gear, m_cadence)));
    }
    if (m_hub)
        m_hub->setLoad(1, gearWatts(gear, m_cadence));
}
