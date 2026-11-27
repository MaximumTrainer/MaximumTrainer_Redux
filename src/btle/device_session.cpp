#include "device_session.h"

#include <QTimer>

#include "account.h"
#include "btle_hub.h"
#include "btle_sensor_store.h"
#include "logger.h"
#include "zwift_click_manager.h"

DeviceSession::DeviceSession(Account *account, QObject *parent)
    : QObject(parent)
    , m_account(account)
{
    m_maintenance = new QTimer(this);
    m_maintenance->setInterval(kMaintenanceMs);
    connect(m_maintenance, &QTimer::timeout, this, &DeviceSession::maintain);
    m_clickBurstEnd = new QTimer(this);
    m_clickBurstEnd->setSingleShot(true);
    m_clickBurstEnd->setInterval(kClickBurstMs);
    connect(m_clickBurstEnd, &QTimer::timeout, this, [this]() {
        if (m_click && !m_rideActive)
            m_click->stopDiscovery();
    });
}

DeviceSession::~DeviceSession()
{
    teardown();
}

DeviceSession::Mode DeviceSession::wantedMode() const
{
    if (m_account->use_simulated_sensors)
        return Mode::Simulated;
    if (m_account->enable_studio_mode)
        return Mode::Studio;
    if (m_released || m_paused)
        return Mode::Released;
    if (SensorConnector::slotsFor(BtleSensorStore::loadAll(), m_account->rowing_mode).isEmpty()
            && !m_account->use_zwift_click)
        return Mode::NothingPaired;
    return Mode::Live;
}

QString DeviceSession::configSignature() const
{
    QStringList parts{QString::number(int(wantedMode())),
                      m_account->rowing_mode ? QStringLiteral("row") : QStringLiteral("bike"),
                      m_account->use_zwift_click ? QStringLiteral("click") : QString()};
    for (const SensorConnector::Slot &slot : SensorConnector::slotsFor(BtleSensorStore::loadAll(),
                                                                       m_account->rowing_mode))
        parts << QStringLiteral("%1=%2/%3").arg(int(slot.role)).arg(slot.saved.address, slot.saved.deviceUuid);
    return parts.join(QLatin1Char('|'));
}

void DeviceSession::reconfigure()
{
    const QString signature = configSignature();
    if (signature == m_signature)
        return;
    m_signature = signature;
    teardown();
    build();
}

void DeviceSession::release()
{
    LOG_INFO("Devices", QStringLiteral("released every device"));
    m_released = true;
    m_signature.clear();
    reconfigure();
}

void DeviceSession::reconnect()
{
    m_released = false;
    m_signature.clear();
    reconfigure();
}

void DeviceSession::setPaused(bool paused)
{
    if (paused == m_paused)
        return;
    m_paused = paused;
    m_signature.clear();
    reconfigure();
}

void DeviceSession::teardown()
{
    m_maintenance->stop();
    m_clickBurstEnd->stop();
    m_waitingSince.clear();
    if (m_click) {
        m_click->stop();
        delete m_click;
    }
    delete m_connector;   // disconnects and frees every hub
    m_connector = nullptr;
}

void DeviceSession::build()
{
    const Mode mode = wantedMode();
    if (mode == Mode::Live) {
        m_connector = new SensorConnector(BtleSensorStore::loadAll(), m_account->rowing_mode, this);
        for (const SensorConnector::Slot &slot : m_connector->sensorSlots())
            m_waitingSince[int(slot.role)].start();
        connect(m_connector, &SensorConnector::statusChanged, this,
                [this](BtleSensorRole role, SensorConnector::Status status) {
            if (status == SensorConnector::Status::Connected)
                m_waitingSince.remove(int(role));
            else if (!m_waitingSince.contains(int(role)))
                m_waitingSince[int(role)].start();
            emit statusChanged(int(role), status);
        });
        connect(m_connector, &SensorConnector::hubConnected, this, &DeviceSession::hubConnected);

        if (m_account->use_zwift_click) {
            m_click = new ZwiftClickManager(this);
            m_waitingSince[kZwiftClickKey].start();
            connect(m_click, &ZwiftClickManager::connectionChanged, this, [this]() {
                const SensorConnector::Status status = clickStatus();
                if (status == SensorConnector::Status::Connected)
                    m_waitingSince.remove(kZwiftClickKey);
                else if (!m_waitingSince.contains(kZwiftClickKey))
                    m_waitingSince[kZwiftClickKey].start();
                emit statusChanged(kZwiftClickKey, status);
            });
        }
        LOG_INFO("Devices", QStringLiteral("connecting %1 saved sensor(s)%2")
                     .arg(m_connector->sensorSlots().size())
                     .arg(m_click ? QStringLiteral(" and Zwift Click") : QString()));
        m_connector->start();
        if (m_click) {
            m_click->start();
            if (!m_rideActive)
                m_clickBurstEnd->start();
        }
        m_maintenance->start();
    }
    setMode(mode);
    emit itemsChanged();
}

void DeviceSession::setMode(Mode mode)
{
    if (mode == m_mode)
        return;
    m_mode = mode;
    emit modeChanged(mode);
}

ZwiftClickManager *DeviceSession::clickManager() const
{
    return m_click.data();
}

SensorConnector::Status DeviceSession::clickStatus() const
{
    // Only the right controller's buttons are mapped, so it alone counts.
    return m_click && m_click->rightConnected() ? SensorConnector::Status::Connected
                                                : SensorConnector::Status::Searching;
}

QVector<DeviceSession::Item> DeviceSession::items() const
{
    QVector<Item> result;
    if (m_connector) {
        for (const SensorConnector::Slot &slot : m_connector->sensorSlots())
            result.append({int(slot.role), BtleSensorStore::roleDisplayName(slot.role), slot.saved.name, slot.status});
    }
    if (m_click)
        result.append({kZwiftClickKey, tr("Zwift Click"), QString(), clickStatus()});
    return result;
}

QMap<BtleSensorRole, BtleHub*> DeviceSession::connectedHubs() const
{
    return m_connector ? m_connector->connectedHubs() : QMap<BtleSensorRole, BtleHub*>();
}

BtleHub *DeviceSession::trainerHub() const
{
    const QMap<BtleSensorRole, BtleHub*> hubs = connectedHubs();
    const BtleSensorRole role = m_account->rowing_mode ? BtleSensorRole::Rower : BtleSensorRole::Trainer;
    return hubs.value(role, nullptr);
}

void DeviceSession::retry(int key)
{
    if (key == kZwiftClickKey) {
        if (m_click)
            m_click->resumeDiscovery();
        return;
    }
    if (m_connector)
        m_connector->retry(BtleSensorRole(key));
}

void DeviceSession::setRideActive(bool active)
{
    m_rideActive = active;
    if (!m_click)
        return;
    if (active) {
        m_clickBurstEnd->stop();
        m_click->resumeDiscovery();
    } else if (m_click->isDiscovering()) {
        m_clickBurstEnd->start();
    }
}

void DeviceSession::returnTrainerToFreeRide()
{
    if (BtleHub *trainer = trainerHub())
        trainer->setSlope(1, 0.0);
}

void DeviceSession::maintain()
{
    ++m_ticks;
    if (m_connector) {
        for (const SensorConnector::Slot &slot : m_connector->sensorSlots()) {
            const bool waiting = slot.status == SensorConnector::Status::NotFound
                || slot.status == SensorConnector::Status::Lost;
            const auto since = m_waitingSince.constFind(int(slot.role));
            const bool stuck = slot.status != SensorConnector::Status::Connected
                && since != m_waitingSince.constEnd() && since->elapsed() > kStuckMs;
            if (waiting || stuck) {
                LOG_INFO("Devices", QStringLiteral("retrying %1").arg(BtleSensorStore::roleDisplayName(slot.role)));
                m_waitingSince[int(slot.role)].start();
                m_connector->retry(slot.role);
            }
        }
    }
    if (m_click && !m_rideActive && !m_click->rightConnected() && m_ticks % kClickBurstEveryTicks == 0)
        clickBurst();
}

void DeviceSession::clickBurst()
{
    m_click->resumeDiscovery();
    m_clickBurstEnd->start();
}
