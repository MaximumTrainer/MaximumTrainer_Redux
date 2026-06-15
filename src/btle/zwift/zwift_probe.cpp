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

// Standard FTMS (the channel that actually actuates resistance on this trainer).
QBluetoothUuid ftmsServiceUuid()      { return QBluetoothUuid(quint16(0x1826)); }
QBluetoothUuid ftmsControlPointUuid() { return QBluetoothUuid(quint16(0x2AD9)); }

// FTMS Set Target Power (opcode 0x05), value = int16 watts, little-endian.
QByteArray ftmsSetTargetPower(int watts)
{
    const qint16 w = static_cast<qint16>(watts);
    QByteArray cmd(3, '\0');
    cmd[0] = 0x05;
    cmd[1] = static_cast<char>(w & 0xFF);
    cmd[2] = static_cast<char>((w >> 8) & 0xFF);
    return cmd;
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
    m_retriesLeft      = 5;   // weak links (distant trainer) fail/drop transiently
    line(QString());
    line(QStringLiteral("== Connecting to %1 [%2] ==")
             .arg(m_current.name(), m_current.address().toString()));

    m_controller = QLowEnergyController::createCentral(m_current, this);
    connect(m_controller, &QLowEnergyController::connected, this, [this]() {
        line(QStringLiteral("  connected — discovering services"));
        m_controller->discoverServices();
    });
    connect(m_controller, &QLowEnergyController::disconnected, this, [this]() {
        if (!m_zwiftServiceSeen) { retryOrAdvance(QStringLiteral("link dropped")); return; }
        line(QStringLiteral("  disconnected"));
    });
    connect(m_controller,
            static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(
                &QLowEnergyController::errorOccurred),
            this, [this](QLowEnergyController::Error) {
                const QString es = m_controller ? m_controller->errorString()
                                                : QStringLiteral("error");
                retryOrAdvance(QStringLiteral("connect error: %1").arg(es));
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

// A weak/transient BLE failure (connect "Unknown Error" or a mid-discovery
// drop) before we see the Zwift service: wait a moment for the stack to settle,
// then reconnect the same device. Bounded by m_retriesLeft. The success path
// (m_zwiftServiceSeen) owns its own teardown, so do nothing there.
void ZwiftProbe::retryOrAdvance(const QString &reason)
{
    if (m_zwiftServiceSeen || m_reconnectPending)
        return;
    if (m_retriesLeft <= 0 || !m_controller) {
        line(QStringLiteral("  %1 — giving up on this device").arg(reason));
        finishCurrentDevice();
        return;
    }
    --m_retriesLeft;
    m_reconnectPending = true;
    if (m_listenTimer) m_listenTimer->stop();
    qDeleteAll(m_services);
    m_services.clear();
    line(QStringLiteral("  %1 — retrying in 1.5 s (%2 left)").arg(reason).arg(m_retriesLeft));
    QTimer::singleShot(1500, this, [this]() {
        m_reconnectPending = false;
        if (m_controller && !m_zwiftServiceSeen)
            m_controller->connectToDevice();
    });
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
            [this, service](const QLowEnergyCharacteristic &c, const QByteArray &value) {
                const QString hex = QString::fromLatin1(value.toHex());
                ZwiftProtocol::RidingData rd;
                // FTMS control-point response: [0x80, opcode, result(0x01=ok)].
                if (c.uuid() == ftmsControlPointUuid() && value.size() >= 3
                        && quint8(value[0]) == 0x80) {
                    const quint8 op = quint8(value[1]);
                    const quint8 res = quint8(value[2]);
                    line(QStringLiteral("    ◀ FTMS resp op=0x%1 result=0x%2")
                             .arg(op, 2, 16, QLatin1Char('0')).arg(res, 2, 16, QLatin1Char('0')));
                    if (op == 0x00 && res == 0x01 && !m_controlStarted) {
                        line(QStringLiteral("    ✓ FTMS control GRANTED — starting script"));
                        beginControlScript(service, ftmsControlPointUuid());
                    }
                    return;
                }
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

            // Zwift-protocol scripts drive the Zwift control point directly. The
            // FTMS script keeps this handshake only for reading power/cadence and
            // is started later, once FTMS control is granted.
            const bool zwiftScript = (m_script == Script::ErgDemo
                                   || m_script == Script::GearSweep
                                   || m_script == Script::ErgHold);
            if (zwiftScript && !m_controlStarted)
                beginControlScript(service, zwiftControlPointUuid());
        }
    }

    // FTMS mode: set up the standard request-control handshake on this trainer's
    // FTMS service — that is the channel that actually actuates resistance.
    if (m_script == Script::FtmsErg && service->serviceUuid() == ftmsServiceUuid())
        setupFtmsControl(service);
}

// Enable indications on the FTMS control point and, once confirmed, send
// Request Control (0x00). Mirrors BtleHub's working sequence.
void ZwiftProbe::setupFtmsControl(QLowEnergyService *service)
{
    line(QStringLiteral("  FTMS service found — enabling control"));
    connect(service, &QLowEnergyService::descriptorWritten, this,
            [this, service](const QLowEnergyDescriptor &, const QByteArray &v) {
                // The 0x0200 (indications) write is the FTMS control point's CCCD.
                if (v != kIndicateOn || m_ftmsRequested)
                    return;
                const QLowEnergyCharacteristic cp =
                    service->characteristic(ftmsControlPointUuid());
                if (!cp.isValid())
                    return;
                m_ftmsRequested = true;
                service->writeCharacteristic(cp, QByteArray::fromHex("00"),
                                             QLowEnergyService::WriteWithResponse);
                line(QStringLiteral("    ▶ FTMS Request Control (0x00) sent"));
            });
}

// Phase 2: after the handshake, send a scripted sequence of 0x04 control
// commands and watch the trainer react (ResCtrl debug log + FTMS data). Steps
// are spaced so each reaction is visible; the final step releases resistance.
void ZwiftProbe::beginControlScript(QLowEnergyService *service,
                                    const QBluetoothUuid &controlPoint)
{
    using namespace ZwiftProtocol;
    m_controlStarted = true;
    m_ctrlService    = service;
    m_ctrlPointUuid  = controlPoint;
    m_ctrlIndex      = 0;

    auto erg = [](quint32 watts) {
        HubCommand c; c.powerTargetW = watts; return encodeControlCommand(c);
    };
    auto simGear = [](qint32 inclineX100, int gear) {
        HubCommand c;
        c.sim.windX100             = 0;
        c.sim.inclineX100          = inclineX100;
        c.sim.cwaX10000            = ZwiftSim::CWaX10000;   // 0.51 — required for gears to bite
        c.sim.crrX100000           = ZwiftSim::CrrX100000;  // 0.004
        c.physical.gearRatioX10000 = ZwiftGears::ratioX10000ForGear(gear);
        c.physical.riderWeightX100 = 7500;   // 75.0 kg
        c.physical.bikeWeightX100  = 800;    //  8.0 kg
        return encodeControlCommand(c);
    };

    if (m_script == Script::FtmsErg) {
        // Decisive test on the PROVEN channel: drive standard FTMS Set Target
        // Power (0x05). If the rider's power tracks these, FTMS is our actuation
        // path for virtual shifting. Warmup holds 100 W so it shows immediately.
        m_ctrlIntervalMs = 15000;
        m_ctrlPrerollMs  = 30000;
        m_ctrlSteps = {
            { QStringLiteral("CONNECTED — get on & pedal; FTMS ERG 100W (30s warmup)"), ftmsSetTargetPower(100) },
            { QStringLiteral("FTMS ERG 170W → power should rise to ~170"), ftmsSetTargetPower(170) },
            { QStringLiteral("FTMS ERG 240W → power should rise to ~240"), ftmsSetTargetPower(240) },
            { QStringLiteral("FTMS ERG 120W → power should drop to ~120"), ftmsSetTargetPower(120) },
            { QStringLiteral("RESET — FTMS ERG 0 W (release)"),            ftmsSetTargetPower(0)   },
        };
    } else if (m_script == Script::GearSweep) {
        // Rider pedals steadily; we step the gear so they FEEL each change with
        // no input. Hold +3% so SIM resistance is non-trivial at speed. The
        // baseline holds for a warmup so the rider can mount + start pedaling
        // (and the connection settles) before gears begin changing.
        m_ctrlIntervalMs = 8000;   // hold each long enough to feel
        m_ctrlPrerollMs  = 30000;  // warmup after baseline
        m_ctrlSteps = {
            { QStringLiteral("CONNECTED — get on & pedal steady ~80rpm (30s warmup)"), simGear(0, 8) },
            { QStringLiteral("Gear 2/24  @ 0% → should feel EASIEST"), simGear(0, 2)  },
            { QStringLiteral("Gear 22/24 @ 0% → should feel HARDEST"), simGear(0, 22) },
            { QStringLiteral("Gear 2/24  @ 0% → EASIEST again"),       simGear(0, 2)  },
            { QStringLiteral("Gear 22/24 @ 0% → HARDEST again"),       simGear(0, 22) },
            { QStringLiteral("Gear 11 @ 0%  → mid gear (flat)"),       simGear(0,   11) },
            { QStringLiteral("Gear 11 @ +5% → grade sanity: HARDER"),  simGear(500, 11) },
            { QStringLiteral("RESET — release resistance"),            erg(0)           },
        };
    } else if (m_script == Script::ErgHold) {
        // Decisive test: does ERG actuate resistance while pedaling? Hold each
        // target long enough for the loop to settle; the rider's power should
        // track the target at whatever cadence they pedal. The warmup itself
        // holds 100 W, so a working ERG shows up immediately.
        m_ctrlIntervalMs = 15000;
        m_ctrlPrerollMs  = 30000;
        m_ctrlSteps = {
            { QStringLiteral("CONNECTED — get on & pedal; holding ERG 100W (30s warmup)"), erg(100) },
            { QStringLiteral("ERG 160W → your power should rise to ~160"), erg(160) },
            { QStringLiteral("ERG 230W → your power should rise to ~230"), erg(230) },
            { QStringLiteral("ERG 120W → your power should drop to ~120"), erg(120) },
            { QStringLiteral("RESET — ERG 0 W (release)"),                 erg(0)   },
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
    line(QStringLiteral("  ▶▶ control script: %1 steps, %2 s each%3 — watch the dbg ResCtrl lines")
             .arg(m_ctrlSteps.size()).arg(m_ctrlIntervalMs / 1000)
             .arg(m_ctrlPrerollMs > 0
                      ? QStringLiteral(" (after a %1s warmup)").arg(m_ctrlPrerollMs / 1000)
                      : QString()));

    if (!m_ctrlTimer) {
        m_ctrlTimer = new QTimer(this);
        m_ctrlTimer->setSingleShot(true);   // each step chains the next explicitly
        connect(m_ctrlTimer, &QTimer::timeout, this, &ZwiftProbe::onControlStep);
    }
    onControlStep();   // sends baseline now, schedules the rest
}

void ZwiftProbe::onControlStep()
{
    if (!m_ctrlService || m_ctrlIndex >= m_ctrlSteps.size()) {
        if (m_ctrlTimer) m_ctrlTimer->stop();
        line(QStringLiteral("  ▶▶ control script complete (resistance released)"));
        finishCurrentDevice();
        return;
    }

    const int sent = m_ctrlIndex;
    const auto &step = m_ctrlSteps.at(m_ctrlIndex++);
    const QLowEnergyCharacteristic cp =
        m_ctrlService->characteristic(m_ctrlPointUuid);
    if (cp.isValid()) {
        const auto mode = (cp.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                              ? QLowEnergyService::WriteWithoutResponse
                              : QLowEnergyService::WriteWithResponse;
        m_ctrlService->writeCharacteristic(cp, step.second, mode);
        line(QStringLiteral("  ▶ step %1/%2: %3   (%4)")
                 .arg(m_ctrlIndex).arg(m_ctrlSteps.size())
                 .arg(step.first, QString::fromLatin1(step.second.toHex())));
    }

    // Chain the next step: the baseline (step 0) holds for the warmup, the rest
    // for the normal interval.
    const int delay = (sent == 0 && m_ctrlPrerollMs > 0) ? m_ctrlPrerollMs
                                                         : m_ctrlIntervalMs;
    if (m_ctrlTimer)
        m_ctrlTimer->start(delay);
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
