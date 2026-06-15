#include "zwift_probe.h"
#include "zwift_protocol.h"

#include <QLowEnergyDescriptor>
#include <QMetaEnum>

#include <cstdio>

namespace {
const QByteArray kNotifyOn   = QByteArray::fromHex("0100");
const QByteArray kIndicateOn = QByteArray::fromHex("0200");

QBluetoothUuid zwiftServiceUuid()
{
    return QBluetoothUuid(QString::fromLatin1(ZwiftProtocol::Uuid::Service));
}
QBluetoothUuid zwiftControlPointUuid()
{
    return QBluetoothUuid(QString::fromLatin1(ZwiftProtocol::Uuid::ControlPoint));
}

// JetBlack's diagnostic log characteristic — streams ASCII text (incl. the
// "ResCtrl: Resistance control timer …" lines that reveal resistance changes).
QBluetoothUuid jetBlackDebugLogUuid()
{
    return QBluetoothUuid(QStringLiteral("c4632b08-003f-4cec-8994-e489b04d857f"));
}

// Console logging — write straight to stdout so output survives the app's
// installed Qt message handler (which filters qInfo unless --debug).
void line(const QString &s)
{
    const QByteArray utf8 = s.toUtf8();
    fprintf(stdout, "%s\n", utf8.constData());
    fflush(stdout);
}
} // namespace

ZwiftProbe::ZwiftProbe(QObject *parent) : QObject(parent) {}

void ZwiftProbe::start(const QString &nameFilter, int scanSeconds,
                       int listenSeconds, Script script)
{
    m_nameFilter    = nameFilter.trimmed();
    m_listenSeconds = listenSeconds;
    m_script        = script;

    line(script != Script::None
             ? QStringLiteral("── Zwift CONTROL TEST (writes change resistance!) ───")
             : QStringLiteral("── Zwift probe (read-only) ──────────────────────────"));
    line(QStringLiteral("  name filter : %1")
             .arg(m_nameFilter.isEmpty() ? QStringLiteral("(auto)") : m_nameFilter));
    line(QStringLiteral("  scan        : %1 s   listen/device : %2 s")
             .arg(scanSeconds).arg(listenSeconds));
    line(QStringLiteral("  zwift svc   : %1").arg(QString::fromLatin1(ZwiftProtocol::Uuid::Service)));
    line(QString());

    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(scanSeconds * 1000);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &ZwiftProbe::onDeviceDiscovered);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &ZwiftProbe::onScanFinished);
    connect(m_agent,
            static_cast<void (QBluetoothDeviceDiscoveryAgent::*)(QBluetoothDeviceDiscoveryAgent::Error)>(
                &QBluetoothDeviceDiscoveryAgent::errorOccurred),
            this, [this](QBluetoothDeviceDiscoveryAgent::Error) {
                line(QStringLiteral("  [scan error] %1").arg(m_agent->errorString()));
                // No adapter / failed scan won't emit finished() — proceed anyway
                // so the probe still quits instead of hanging.
                onScanFinished();
            });

    m_listenTimer = new QTimer(this);
    m_listenTimer->setSingleShot(true);
    connect(m_listenTimer, &QTimer::timeout, this, &ZwiftProbe::finishCurrentDevice);

    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

bool ZwiftProbe::looksInteresting(const QBluetoothDeviceInfo &info, const QString &nameFilter)
{
    if (!nameFilter.isEmpty())
        return info.name().contains(nameFilter, Qt::CaseInsensitive);

    if (info.serviceUuids().contains(zwiftServiceUuid()))
        return true;

    // No filter and no advertised Zwift service: fall back to name hints so we
    // still catch the Click and trainers that don't advertise the full service.
    static const QStringList hints = {
        QStringLiteral("zwift"), QStringLiteral("click"), QStringLiteral("play"),
        QStringLiteral("victory"), QStringLiteral("jetblack"), QStringLiteral("jet black"),
    };
    const QString name = info.name();
    for (const QString &h : hints)
        if (name.contains(h, Qt::CaseInsensitive))
            return true;
    return false;
}

void ZwiftProbe::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    QStringList svc;
    for (const QBluetoothUuid &u : info.serviceUuids())
        svc << u.toString(QUuid::WithoutBraces);

    const bool interesting = looksInteresting(info, m_nameFilter);

    // Cut the scan-log flood: skip uninteresting devices that expose no
    // services (TVs, phones, beacons). Count them for the summary instead.
    if (!interesting && info.serviceUuids().isEmpty()) {
        ++m_suppressedCount;
        return;
    }

    line(QStringLiteral("  %1 %2  [%3]  rssi=%4  svcs={%5}")
             .arg(interesting ? QStringLiteral("►") : QStringLiteral(" "))
             .arg(info.name().isEmpty() ? QStringLiteral("(unnamed)") : info.name(), -22)
             .arg(info.address().toString())
             .arg(info.rssi())
             .arg(svc.join(QStringLiteral(", "))));

    if (interesting && !m_candidates.contains(info))
        m_candidates.enqueue(info);
}

void ZwiftProbe::onScanFinished()
{
    if (m_scanHandled)   // finished() and errorOccurred() can both fire
        return;
    m_scanHandled = true;
    line(QString());
    line(QStringLiteral("Scan finished — %1 candidate(s); %2 service-less device(s) hidden.")
             .arg(m_candidates.size()).arg(m_suppressedCount));
    connectNextCandidate();
}

void ZwiftProbe::connectNextCandidate()
{
    teardownController();
    if (m_candidates.isEmpty()) {
        line(QStringLiteral("── probe complete ──"));
        emit finished();
        return;
    }

    m_current          = m_candidates.dequeue();
    m_zwiftServiceSeen = false;
    m_retriesLeft      = 3;   // weak links (distant trainer) drop mid-discovery
    line(QString());
    line(QStringLiteral("== Connecting to %1 [%2] ==")
             .arg(m_current.name(), m_current.address().toString()));

    m_controller = QLowEnergyController::createCentral(m_current, this);
    connect(m_controller, &QLowEnergyController::connected, this, [this]() {
        line(QStringLiteral("  connected — discovering services"));
        m_controller->discoverServices();
    });
    connect(m_controller, &QLowEnergyController::disconnected, this, [this]() {
        // A weak link can drop before per-service detail discovery completes,
        // so we never see the Zwift service. Retry the same device a few times.
        if (!m_zwiftServiceSeen && m_retriesLeft > 0 && m_controller) {
            --m_retriesLeft;
            if (m_listenTimer) m_listenTimer->stop();
            qDeleteAll(m_services);
            m_services.clear();
            line(QStringLiteral("  link dropped before discovery — retrying (%1 left)")
                     .arg(m_retriesLeft));
            m_controller->connectToDevice();
            return;
        }
        line(QStringLiteral("  disconnected"));
    });
    connect(m_controller,
            static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(
                &QLowEnergyController::errorOccurred),
            this, [this](QLowEnergyController::Error) {
                line(QStringLiteral("  [controller error] %1").arg(m_controller->errorString()));
                finishCurrentDevice();
            });
    connect(m_controller, &QLowEnergyController::discoveryFinished, this, [this]() {
        const auto uuids = m_controller->services();
        line(QStringLiteral("  %1 service(s) discovered").arg(uuids.size()));
        for (const QBluetoothUuid &u : uuids) {
            QLowEnergyService *svc = m_controller->createServiceObject(u, this);
            if (!svc)
                continue;
            m_services << svc;
            connect(svc, &QLowEnergyService::stateChanged, this,
                    [this, svc](QLowEnergyService::ServiceState st) {
                        if (st == QLowEnergyService::RemoteServiceDiscovered)
                            dumpService(svc);
                    });
            svc->discoverDetails();
        }
        // Hold the connection open to capture streaming frames, then move on.
        m_listenTimer->start(m_listenSeconds * 1000);
    });

    m_controller->connectToDevice();
}

void ZwiftProbe::dumpService(QLowEnergyService *service)
{
    const bool isZwift = service->serviceUuid() == zwiftServiceUuid();
    if (isZwift)
        m_zwiftServiceSeen = true;

    line(QStringLiteral("  service %1%2")
             .arg(service->serviceUuid().toString(QUuid::WithoutBraces))
             .arg(isZwift ? QStringLiteral("   ◄ ZWIFT TRAINER SERVICE") : QString()));

    for (const QLowEnergyCharacteristic &c : service->characteristics()) {
        line(QStringLiteral("      char %1  props=[%2]  val=%3")
                 .arg(c.uuid().toString(QUuid::WithoutBraces))
                 .arg(propsToString(c.properties()))
                 .arg(QString::fromLatin1(c.value().toHex())));
    }
    subscribeAndProbe(service);
}

void ZwiftProbe::subscribeAndProbe(QLowEnergyService *service)
{
    connect(service, &QLowEnergyService::characteristicChanged, this,
            [this](const QLowEnergyCharacteristic &c, const QByteArray &value) {
                const QString hex = QString::fromLatin1(value.toHex());
                ZwiftProtocol::RidingData rd;
                if (c.uuid() == jetBlackDebugLogUuid()) {
                    // ASCII diagnostic log — show the text (ResCtrl lines etc.).
                    line(QStringLiteral("    ◀ dbg  %1")
                             .arg(QString::fromLatin1(value).trimmed()));
                } else if (ZwiftProtocol::decodeRidingData(value, rd)) {
                    line(QStringLiteral("    ◀ %1  RIDING pwr=%2 cad=%3 spd=%4 hr=%5")
                             .arg(c.uuid().toString(QUuid::WithoutBraces))
                             .arg(rd.power).arg(rd.cadence).arg(rd.speedX100).arg(rd.hr));
                } else {
                    line(QStringLiteral("    ◀ %1  %2")
                             .arg(c.uuid().toString(QUuid::WithoutBraces), hex));
                }
            });   // one connection per service object (dumpService runs once each)

    // Subscribe to every notify/indicate characteristic so we also capture the
    // Zwift Click's button frames, not just trainer riding data.
    for (const QLowEnergyCharacteristic &c : service->characteristics()) {
        const auto props = c.properties();
        QLowEnergyDescriptor cccd = c.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
        if (!cccd.isValid())
            continue;
        if (props & QLowEnergyCharacteristic::Notify)
            service->writeDescriptor(cccd, kNotifyOn);
        else if (props & QLowEnergyCharacteristic::Indicate)
            service->writeDescriptor(cccd, kIndicateOn);
    }

    // The only write we ever make: the documented handshake that makes the
    // trainer start streaming. No 0x04 control command → resistance untouched.
    if (service->serviceUuid() == zwiftServiceUuid()) {
        const QLowEnergyCharacteristic cp = service->characteristic(zwiftControlPointUuid());
        if (cp.isValid()) {
            const auto mode = (cp.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                                  ? QLowEnergyService::WriteWithoutResponse
                                  : QLowEnergyService::WriteWithResponse;
            service->writeCharacteristic(cp, ZwiftProtocol::rideOnHandshake(), mode);
            line(QStringLiteral("    ▶ wrote RideOn handshake to control point"));

            if (m_script != Script::None && !m_controlStarted)
                beginControlScript(service);
        }
    }
}

// Phase 2: after the handshake, send a scripted sequence of 0x04 control
// commands and watch the trainer react (ResCtrl debug log + FTMS data). Steps
// are spaced so each reaction is visible; the final step releases resistance.
void ZwiftProbe::beginControlScript(QLowEnergyService *zwiftService)
{
    using namespace ZwiftProtocol;
    m_controlStarted = true;
    m_ctrlService    = zwiftService;
    m_ctrlIndex      = 0;

    auto erg = [](quint32 watts) {
        HubCommand c; c.powerTargetW = watts; return encodeControlCommand(c);
    };
    auto simGear = [](qint32 inclineX100, int gear) {
        HubCommand c;
        c.sim.inclineX100          = inclineX100;
        c.physical.gearRatioX10000 = ZwiftGears::ratioX10000ForGear(gear);
        c.physical.riderWeightX100 = 7500;   // 75.0 kg
        c.physical.bikeWeightX100  = 800;    //  8.0 kg
        return encodeControlCommand(c);
    };

    if (m_script == Script::GearSweep) {
        // Rider pedals steadily; we step the gear so they FEEL each change with
        // no input. Hold +3% so SIM resistance is non-trivial at speed.
        m_ctrlIntervalMs = 8000;   // hold each long enough to feel
        m_ctrlSteps = {
            { QStringLiteral("PEDAL STEADY ~80rpm. Gear 12/24 @ +3% (baseline)"), simGear(300, 12) },
            { QStringLiteral("Gear 1/24  → should feel EASIEST"), simGear(300, 1)  },
            { QStringLiteral("Gear 24/24 → should feel HARDEST"), simGear(300, 24) },
            { QStringLiteral("Gear 1/24  → EASIEST again"),       simGear(300, 1)  },
            { QStringLiteral("Gear 24/24 → HARDEST again"),       simGear(300, 24) },
            { QStringLiteral("Gear 12 @ 0% → sanity: flat/easy"),simGear(0,   12) },
            { QStringLiteral("Gear 12 @ +6% → sanity: HARDER"),  simGear(600, 12) },
            { QStringLiteral("RESET — release resistance"),       erg(0)           },
        };
    } else { // Script::ErgDemo
        m_ctrlIntervalMs = 4500;
        m_ctrlSteps = {
            { QStringLiteral("ERG 100 W"),                 erg(100) },
            { QStringLiteral("ERG 250 W"),                 erg(250) },
            { QStringLiteral("ERG 150 W"),                 erg(150) },
            { QStringLiteral("SIM +6% gear 12 (mid)"),    simGear(600, 12) },
            { QStringLiteral("SIM +6% gear 2 (low/easy)"),simGear(600, 2)  },
            { QStringLiteral("SIM +6% gear 22 (high)"),   simGear(600, 22) },
            { QStringLiteral("RESET — ERG 0 W (release)"), erg(0)  },
        };
    }

    line(QString());
    line(QStringLiteral("  ▶▶ control script: %1 steps, %2 s each — watch the dbg ResCtrl lines")
             .arg(m_ctrlSteps.size()).arg(m_ctrlIntervalMs / 1000));

    if (!m_ctrlTimer) {
        m_ctrlTimer = new QTimer(this);
        connect(m_ctrlTimer, &QTimer::timeout, this, &ZwiftProbe::onControlStep);
    }
    onControlStep();                       // first step immediately
    m_ctrlTimer->start(m_ctrlIntervalMs);  // then one per interval
}

void ZwiftProbe::onControlStep()
{
    if (!m_ctrlService || m_ctrlIndex >= m_ctrlSteps.size()) {
        if (m_ctrlTimer) m_ctrlTimer->stop();
        line(QStringLiteral("  ▶▶ control script complete (resistance released)"));
        finishCurrentDevice();
        return;
    }

    const auto &step = m_ctrlSteps.at(m_ctrlIndex++);
    const QLowEnergyCharacteristic cp =
        m_ctrlService->characteristic(zwiftControlPointUuid());
    if (cp.isValid()) {
        const auto mode = (cp.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                              ? QLowEnergyService::WriteWithoutResponse
                              : QLowEnergyService::WriteWithResponse;
        m_ctrlService->writeCharacteristic(cp, step.second, mode);
        line(QStringLiteral("  ▶ step %1/%2: %3   (%4)")
                 .arg(m_ctrlIndex).arg(m_ctrlSteps.size())
                 .arg(step.first, QString::fromLatin1(step.second.toHex())));
    }
}

void ZwiftProbe::finishCurrentDevice()
{
    if (m_listenTimer)
        m_listenTimer->stop();
    line(QStringLiteral("  -- done with %1 (zwift service %2) --")
             .arg(m_current.name(),
                  m_zwiftServiceSeen ? QStringLiteral("FOUND") : QStringLiteral("not found")));
    connectNextCandidate();
}

void ZwiftProbe::teardownController()
{
    qDeleteAll(m_services);
    m_services.clear();
    if (!m_controller)
        return;

    // Disconnect is async — delete only once it has settled, else Qt warns
    // "Controller is not Unconnected when deleted".
    QLowEnergyController *c = m_controller;
    m_controller = nullptr;
    connect(c, &QLowEnergyController::disconnected, c, &QObject::deleteLater);
    if (c->state() != QLowEnergyController::UnconnectedState)
        c->disconnectFromDevice();
    else
        c->deleteLater();
}

QString ZwiftProbe::propsToString(QLowEnergyCharacteristic::PropertyTypes p)
{
    QStringList out;
    if (p & QLowEnergyCharacteristic::Read)            out << QStringLiteral("Rd");
    if (p & QLowEnergyCharacteristic::Write)           out << QStringLiteral("Wr");
    if (p & QLowEnergyCharacteristic::WriteNoResponse) out << QStringLiteral("WrNR");
    if (p & QLowEnergyCharacteristic::Notify)          out << QStringLiteral("Ntf");
    if (p & QLowEnergyCharacteristic::Indicate)        out << QStringLiteral("Ind");
    return out.join(QLatin1Char('|'));
}
