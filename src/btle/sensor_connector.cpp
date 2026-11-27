#include "sensor_connector.h"

#include "btle_hub.h"
#include "btle_sensor_store.h"

namespace {
/// Stable identity for a saved sensor: two roles on the same physical device
/// share this key and therefore one BtleHub.
QString deviceKey(const BtleSavedSensor &saved)
{
    return saved.deviceUuid.isEmpty() ? saved.address : saved.deviceUuid;
}
} // namespace

SensorConnector::SensorConnector(const QMap<BtleSensorRole, BtleSavedSensor> &savedSensors,
                                 bool rowingMode, QObject *parent)
    : QObject(parent)
    , m_slots(slotsFor(savedSensors, rowingMode))
{
    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(kScanTimeoutMs);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &SensorConnector::onDeviceDiscovered);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished, this, &SensorConnector::onScanFinished);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, [this](QBluetoothDeviceDiscoveryAgent::Error) { onScanFinished(); });
}

SensorConnector::~SensorConnector()
{
    if (m_agent && m_agent->isActive())
        m_agent->stop();
    if (m_hubsTaken)
        return;
    for (BtleHub *hub : std::as_const(m_hubByDevice)) {
        hub->disconnectFromDevice();
        delete hub;
    }
}

QVector<SensorConnector::Slot> SensorConnector::slotsFor(
        const QMap<BtleSensorRole, BtleSavedSensor> &savedSensors, bool rowingMode)
{
    QVector<Slot> result;
    for (BtleSensorRole role : BtleSensorStore::allRoles()) {
        if (role == BtleSensorRole::Trainer && rowingMode)
            continue;
        if (role == BtleSensorRole::Rower && !rowingMode)
            continue;
        const auto found = savedSensors.constFind(role);
        if (found == savedSensors.constEnd() || !found->isValid() || !found->enabled)
            continue;
        Slot slot;
        slot.role = role;
        slot.saved = *found;
        result.append(slot);
    }
    return result;
}

bool SensorConnector::anyConnected() const
{
    return std::any_of(m_slots.cbegin(), m_slots.cend(),
                       [](const Slot &slot) { return slot.status == Status::Connected; });
}

bool SensorConnector::isScanning() const
{
    return m_agent && m_agent->isActive();
}

QMap<BtleSensorRole, BtleHub*> SensorConnector::connectedHubs() const
{
    QMap<BtleSensorRole, BtleHub*> result;
    for (const Slot &slot : m_slots) {
        if (slot.status == Status::Connected && slot.hub)
            result.insert(slot.role, slot.hub);
    }
    return result;
}

void SensorConnector::takeHubs()
{
    m_hubsTaken = true;
    for (BtleHub *hub : std::as_const(m_hubByDevice))
        hub->setParent(nullptr);
}

void SensorConnector::start()
{
    if (m_slots.isEmpty() || isScanning())
        return;
    for (int index = 0; index < m_slots.size(); ++index) {
        if (m_slots[index].status == Status::NotFound)
            setStatus(index, Status::Searching);
    }
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void SensorConnector::stopScan()
{
    if (isScanning())
        m_agent->stop();
}

void SensorConnector::retry(BtleSensorRole role)
{
    for (int index = 0; index < m_slots.size(); ++index) {
        Slot &slot = m_slots[index];
        if (slot.role != role || slot.status == Status::Connected)
            continue;
        const QString key = deviceKey(slot.saved);
        if (slot.hub)
            m_releasedHubs.remove(slot.hub);
        if (slot.hub && m_infoByDevice.contains(key)) {
            setStatusForHub(slot.hub, Status::Connecting);
            slot.hub->connectToDevice(m_infoByDevice.value(key));
            return;
        }
        setStatus(index, Status::Searching);
        start();
        return;
    }
}

void SensorConnector::release(BtleSensorRole role)
{
    for (const Slot &slot : std::as_const(m_slots)) {
        if (slot.role != role || !slot.hub)
            continue;
        m_releasedHubs.insert(slot.hub);
        slot.hub->disconnectFromDevice();
        setStatusForHub(slot.hub, Status::Disconnected);
        return;
    }
}

BtleHub *SensorConnector::hubForDevice(const QString &key, const QBluetoothDeviceInfo &info)
{
    if (BtleHub *existing = m_hubByDevice.value(key, nullptr))
        return existing;

    auto *hub = new BtleHub(this);
    connect(hub, &BtleHub::deviceConnected, this, [this, hub]() {
        setStatusForHub(hub, Status::Connected);
        emit hubConnected(hub);
    });
    // The hub retries an unexpected drop on its own; show it as reconnecting
    // until it either comes back or gives up with connectionError.
    connect(hub, &BtleHub::deviceDisconnected, this, [this, hub]() {
        setStatusForHub(hub, m_releasedHubs.contains(hub) ? Status::Disconnected : Status::Connecting);
    });
    connect(hub, &BtleHub::connectionError, this, [this, hub](const QString &) {
        setStatusForHub(hub, Status::Lost);
    });
    m_hubByDevice.insert(key, hub);
    m_infoByDevice.insert(key, info);
    hub->connectToDevice(info);
    return hub;
}

void SensorConnector::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    if (!(info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration))
        return;

    for (int index = 0; index < m_slots.size(); ++index) {
        Slot &slot = m_slots[index];
        if (slot.status != Status::Searching || !BtleSensorStore::matchesDiscovered(slot.saved, info))
            continue;
        slot.hub = hubForDevice(deviceKey(slot.saved), info);
        setStatus(index, slot.hub->isConnected() ? Status::Connected : Status::Connecting);
    }
}

void SensorConnector::onScanFinished()
{
    for (int index = 0; index < m_slots.size(); ++index) {
        if (m_slots[index].status == Status::Searching)
            setStatus(index, Status::NotFound);
    }
    emit scanFinished();
}

void SensorConnector::setStatus(int slotIndex, Status status)
{
    Slot &slot = m_slots[slotIndex];
    if (slot.status == status)
        return;
    slot.status = status;
    emit statusChanged(slot.role, status);
}

void SensorConnector::setStatusForHub(BtleHub *hub, Status status)
{
    for (int index = 0; index < m_slots.size(); ++index) {
        if (m_slots[index].hub == hub)
            setStatus(index, status);
    }
}
