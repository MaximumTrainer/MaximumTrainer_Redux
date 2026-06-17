#include "zwift_click_manager.h"

#include "zwift_click_hub.h"
#include "zwift_click_protocol.h"
#include "logger.h"

namespace {
// Stable per-device key (deviceUuid on macOS, address otherwise) for dedup —
// the scan surfaces the same controller several times (adv vs scan response)
// with non-equal QBluetoothDeviceInfo, so dedup on identity, not the whole info.
QString deviceKey(const QBluetoothDeviceInfo &info)
{
#ifdef Q_OS_MACOS
    return info.deviceUuid().toString(QUuid::WithoutBraces);
#else
    return info.address().toString();
#endif
}
} // namespace

ZwiftClickManager::ZwiftClickManager(QObject *parent) : QObject(parent)
{
    for (qint64 &t : m_lastButtonMs)
        t = -BUTTON_DEBOUNCE_MS;
    m_clock.start();   // debounce clock runs even when fed devices externally
}

int ZwiftClickManager::connectedCount() const
{
    int n = 0;
    for (ZwiftClickHub *hub : m_hubs)
        if (hub->isConnected())
            ++n;
    return n;
}

// A Click v2 shifter: name hints at Click/Play and is NOT the trainer (the
// JetBlack Victory also advertises 0xFC82 but is not an input device).
bool ZwiftClickManager::looksLikeShifter(const QBluetoothDeviceInfo &info,
                                         const QString &nameFilter)
{
    const QString name = info.name();
    if (!nameFilter.isEmpty())
        return name.contains(nameFilter, Qt::CaseInsensitive);
    const QString n = name.toLower();
    if (n.contains("victory") || n.contains("jetblack") || n.contains("jet black"))
        return false;
    return n.contains("click") || n.contains("play") || n.contains("zwift");
}

int ZwiftClickManager::controllerSide(const QBluetoothDeviceInfo &info)
{
    const QByteArray data = info.manufacturerData(0x094A);
    if (data.isEmpty())
        return -1;
    return static_cast<quint8>(data.at(0));   // 8 = left/main, 7 = right
}

void ZwiftClickManager::start(const QString &nameFilter, bool singleDevice)
{
    if (m_agent)
        return;   // already scanning
    m_singleDevice = singleDevice;
    m_nameFilter = nameFilter.trimmed();
    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    // A controller only advertises for a few seconds after a button press, so
    // scan continuously to catch late-woken ones.
    m_agent->setLowEnergyDiscoveryTimeout(0);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &ZwiftClickManager::onDeviceDiscovered);
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void ZwiftClickManager::stopDiscovery()
{
    if (m_agent) {
        m_agent->stop();
        m_agent->deleteLater();
        m_agent = nullptr;
    }
}

void ZwiftClickManager::stop()
{
    if (m_agent) {
        m_agent->stop();
        m_agent->deleteLater();
        m_agent = nullptr;
    }
    qDeleteAll(m_hubs);
    m_hubs.clear();
    m_knownIds.clear();
}

void ZwiftClickManager::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    if (!looksLikeShifter(info, m_nameFilter))
        return;
    // In single-device mode, connect only the first shifter we accept, then stop
    // discovering so no second device connects.
    if (m_singleDevice && !m_hubs.isEmpty())
        return;
    connectDevice(info);
    if (m_singleDevice && !m_hubs.isEmpty())
        stopDiscovery();
}

void ZwiftClickManager::connectDevice(const QBluetoothDeviceInfo &info)
{
    if (m_knownIds.contains(deviceKey(info)))
        return;   // already connecting this physical device
    m_knownIds.insert(deviceKey(info));

    const int side = controllerSide(info);
    const QString sideStr = side == 8 ? QStringLiteral("LEFT/main")
                          : side == 7 ? QStringLiteral("RIGHT")
                                      : QStringLiteral("unknown");
    LOG_INFO("ZwiftClick", QStringLiteral("discovered %1 [%2] side=%3 (mfr-id byte %4)")
                 .arg(info.name(), deviceKey(info), sideStr).arg(side));

    auto *hub = new ZwiftClickHub(this);
    m_hubs.append(hub);

    connect(hub, &ZwiftClickHub::connected, this, [this, hub]() {
        emit deviceConnected(hub->deviceName(), hub->deviceAddress());
    });
    connect(hub, &ZwiftClickHub::disconnected, this, [this, hub]() {
        emit deviceDisconnected(hub->deviceAddress());
        m_knownIds.remove(hub->deviceAddress());
        m_hubs.removeOne(hub);
        hub->deleteLater();
    });
    connect(hub, &ZwiftClickHub::buttonPressed, this,
            &ZwiftClickManager::onButtonPressed);

    hub->connectToDevice(info);
}

bool ZwiftClickManager::acceptButton(int bit)
{
    if (bit < 0 || bit >= 32)
        return false;
    const qint64 now = m_clock.isValid() ? m_clock.elapsed() : 0;
    if (now - m_lastButtonMs[bit] < BUTTON_DEBOUNCE_MS)
        return false;   // echo from the paired device / repeated edge
    m_lastButtonMs[bit] = now;
    return true;
}

void ZwiftClickManager::onButtonPressed(int bit)
{
    if (!acceptButton(bit))
        return;

    // RIGHT-controller-only mapping (the left Click v2 needs the proprietary
    // unlock we can't do, so only the right's buttons are wired):
    //   Y → gear up   B → gear down   A → radio next   Z → radio prev
    //   right paddle (+) → start/pause workout
    // Left-side inputs (d-pad, navigation, left paddle) are intentionally ignored.
    using namespace ZwiftClick;
    switch (bit) {
    case ButtonYBit:   emit shiftUp();            break;  // Y → gear up
    case ButtonBBit:   emit shiftDown();          break;  // B → gear down
    case ButtonABit:   emit radioNext();          break;  // A → radio right/next
    case ButtonZBit:   emit radioPrev();          break;  // Z → radio left/prev
    case UpShiftBit:   emit startPauseWorkout();  break;  // right paddle (+) → start/pause
    default:           emit unmappedButton(bit);   break;  // left side / unmapped
    }
}
