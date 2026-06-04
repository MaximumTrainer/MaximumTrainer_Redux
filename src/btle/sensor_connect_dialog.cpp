#include "sensor_connect_dialog.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

#include "btle_hub.h"
#include "btle_sensor_store.h"

namespace {
/// Stable identity for a saved sensor – the platform connectable identifier.
/// Two roles pointing at the same physical device share this key and thus
/// share one BtleHub.
QString deviceKey(const BtleSavedSensor &s)
{
    return s.deviceUuid.isEmpty() ? s.address : s.deviceUuid;
}
} // namespace

SensorConnectDialog::SensorConnectDialog(
        const QMap<BtleSensorRole, BtleSavedSensor> &savedSensors,
        int wheelCircMm,
        QWidget *parent)
    : QDialog(parent)
    , m_wheelCircMm(wheelCircMm)
{
    setWindowTitle(tr("Connecting Sensors"));

    // ── Build one row per valid, enabled saved sensor ────────────────────────
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QGridLayout *grid = new QGridLayout();
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(8);

    int row = 0;
    for (BtleSensorRole role : BtleSensorStore::allRoles()) {
        if (!savedSensors.contains(role))
            continue;
        const BtleSavedSensor saved = savedSensors.value(role);
        if (!saved.isValid() || !saved.enabled)
            continue;

        ConnectSlot slot;
        slot.role  = role;
        slot.saved = saved;

        QLabel *roleLabel   = new QLabel(BtleSensorStore::roleDisplayName(role), this);
        QLabel *deviceLabel = new QLabel(saved.name, this);
        deviceLabel->setStyleSheet(QStringLiteral("color: #777;"));
        slot.statusLabel    = new QLabel(this);

        grid->addWidget(roleLabel,        row, 0);
        grid->addWidget(deviceLabel,      row, 1);
        grid->addWidget(slot.statusLabel, row, 2);

        m_slots.append(slot);
        ++row;
    }

    mainLayout->addLayout(grid);
    mainLayout->addStretch();

    // ── Buttons ───────────────────────────────────────────────────────────────
    QHBoxLayout *btnRow = new QHBoxLayout();
    m_btnManage   = new QPushButton(tr("Manage Sensors…"), this);
    m_btnRescan   = new QPushButton(tr("Rescan"), this);
    m_btnSkip     = new QPushButton(tr("Skip"), this);
    m_btnContinue = new QPushButton(tr("Continue"), this);
    m_btnContinue->setDefault(true);

    btnRow->addWidget(m_btnManage);
    btnRow->addStretch();
    btnRow->addWidget(m_btnRescan);
    btnRow->addWidget(m_btnSkip);
    btnRow->addWidget(m_btnContinue);
    mainLayout->addLayout(btnRow);

    connect(m_btnContinue, &QPushButton::clicked, this, &SensorConnectDialog::onContinueClicked);
    connect(m_btnSkip,     &QPushButton::clicked, this, &SensorConnectDialog::onSkipClicked);
    connect(m_btnRescan,   &QPushButton::clicked, this, &SensorConnectDialog::onRescanClicked);
    connect(m_btnManage,   &QPushButton::clicked, this, &SensorConnectDialog::onManageClicked);

    // ── Discovery agent ─────────────────────────────────────────────────────
    m_agent = new QBluetoothDeviceDiscoveryAgent(this);
    m_agent->setLowEnergyDiscoveryTimeout(SCAN_TIMEOUT_MS);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &SensorConnectDialog::onDeviceDiscovered);
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &SensorConnectDialog::onScanFinished);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_agent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, &SensorConnectDialog::onScanError);
#else
    connect(m_agent,
            static_cast<void(QBluetoothDeviceDiscoveryAgent::*)
                (QBluetoothDeviceDiscoveryAgent::Error)>(&QBluetoothDeviceDiscoveryAgent::error),
            this, &SensorConnectDialog::onScanError);
#endif

    m_autoContinueTimer = new QTimer(this);
    m_autoContinueTimer->setSingleShot(true);
    connect(m_autoContinueTimer, &QTimer::timeout, this, [this]() {
        if (anyConnected())
            onContinueClicked();
    });

    for (int i = 0; i < m_slots.size(); ++i)
        updateSlotUi(i);
    refreshButtons();

    startDiscovery();
}

SensorConnectDialog::~SensorConnectDialog()
{
    if (m_agent && m_agent->isActive())
        m_agent->stop();
    if (!m_hubsDetached)
        teardownHubs();
}

void SensorConnectDialog::detachHubs()
{
    m_hubsDetached = true;
    // Reparent so this dialog's destruction does not take the hubs with it.
    // The caller (executeWorkout) becomes responsible for deleting them when
    // the workout ends.
    for (BtleHub *hub : m_hubByDevice) {
        if (hub)
            hub->setParent(nullptr);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void SensorConnectDialog::startDiscovery()
{
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].status == SlotStatus::Searching
            || m_slots[i].status == SlotStatus::NotFound
            || m_slots[i].status == SlotStatus::Error) {
            m_slots[i].status = SlotStatus::Searching;
            updateSlotUi(i);
        }
    }
    refreshButtons();
    m_agent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void SensorConnectDialog::resetForRescan()
{
    if (m_agent && m_agent->isActive())
        m_agent->stop();
    m_autoContinueTimer->stop();
    teardownHubs();
    for (int i = 0; i < m_slots.size(); ++i) {
        m_slots[i].status = SlotStatus::Searching;
        m_slots[i].hub    = nullptr;
        updateSlotUi(i);
    }
}

void SensorConnectDialog::teardownHubs()
{
    for (BtleHub *hub : m_hubByDevice) {
        if (hub) {
            hub->disconnectFromDevice();
            delete hub;
        }
    }
    m_hubByDevice.clear();
    for (ConnectSlot &slot : m_slots)
        slot.hub = nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
void SensorConnectDialog::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    if (!(info.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration))
        return;

    for (int i = 0; i < m_slots.size(); ++i) {
        ConnectSlot &slot = m_slots[i];
        if (slot.status != SlotStatus::Searching)
            continue;
        if (!BtleSensorStore::matchesDiscovered(slot.saved, info))
            continue;

        // Reuse an existing hub if this physical device is already connecting
        // for another role; otherwise create one and connect it.
        const QString key = deviceKey(slot.saved);
        BtleHub *hub = m_hubByDevice.value(key, nullptr);
        if (!hub) {
            hub = new BtleHub(this);
            if (m_wheelCircMm > 0)
                hub->setWheelCircumferenceMm(m_wheelCircMm);
            connect(hub, &BtleHub::deviceConnected, this, [this, hub]() {
                markSlotConnected(hub);
            });
            connect(hub, &BtleHub::connectionError, this, [this, hub](const QString &) {
                markSlotError(hub);
            });
            m_hubByDevice.insert(key, hub);
            hub->connectToDevice(info);
        }
        slot.hub    = hub;
        slot.status = SlotStatus::Connecting;
        updateSlotUi(i);
    }
}

void SensorConnectDialog::markSlotConnected(BtleHub *hub)
{
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].hub == hub && m_slots[i].status != SlotStatus::Connected) {
            m_slots[i].status = SlotStatus::Connected;
            updateSlotUi(i);
        }
    }
    refreshButtons();
}

void SensorConnectDialog::markSlotError(BtleHub *hub)
{
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].hub == hub && m_slots[i].status != SlotStatus::Connected) {
            m_slots[i].status = SlotStatus::Error;
            updateSlotUi(i);
        }
    }
    refreshButtons();
}

void SensorConnectDialog::onScanFinished()
{
    for (int i = 0; i < m_slots.size(); ++i) {
        if (m_slots[i].status == SlotStatus::Searching) {
            m_slots[i].status = SlotStatus::NotFound;
            updateSlotUi(i);
        }
    }
    refreshButtons();

    // Give in-flight GATT connections a moment, then auto-continue if anything
    // connected so an unattended start isn't blocked waiting for the user.
    if (anyConnected())
        m_autoContinueTimer->start(AUTO_CONTINUE_MS);
}

void SensorConnectDialog::onScanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    Q_UNUSED(error)
    onScanFinished();
}

// ─────────────────────────────────────────────────────────────────────────────
void SensorConnectDialog::onContinueClicked()
{
    m_autoContinueTimer->stop();
    if (m_agent && m_agent->isActive())
        m_agent->stop();
    accept();
}

void SensorConnectDialog::onSkipClicked()
{
    m_autoContinueTimer->stop();
    if (m_agent && m_agent->isActive())
        m_agent->stop();
    teardownHubs();   // discard any connections – caller falls back to manual scan
    accept();
}

void SensorConnectDialog::onRescanClicked()
{
    resetForRescan();
    startDiscovery();
    refreshButtons();
}

void SensorConnectDialog::onManageClicked()
{
    emit openSensorPreferences();
}

// ─────────────────────────────────────────────────────────────────────────────
QMap<BtleSensorRole, BtleHub*> SensorConnectDialog::connectedHubs() const
{
    QMap<BtleSensorRole, BtleHub*> result;
    for (const ConnectSlot &slot : m_slots) {
        if (slot.status == SlotStatus::Connected && slot.hub)
            result.insert(slot.role, slot.hub);
    }
    return result;
}

bool SensorConnectDialog::anyConnected() const
{
    for (const ConnectSlot &slot : m_slots)
        if (slot.status == SlotStatus::Connected)
            return true;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
void SensorConnectDialog::updateSlotUi(int slotIndex)
{
    ConnectSlot &slot = m_slots[slotIndex];
    if (!slot.statusLabel)
        return;

    QString text;
    QString color;
    switch (slot.status) {
    case SlotStatus::Searching:  text = tr("Searching…");  color = "#777";    break;
    case SlotStatus::Connecting: text = tr("Connecting…"); color = "#777";    break;
    case SlotStatus::Connected:  text = tr("✓ Connected"); color = "green";   break;
    case SlotStatus::NotFound:   text = tr("Not found");   color = "red";     break;
    case SlotStatus::Error:      text = tr("Error");       color = "red";     break;
    }
    slot.statusLabel->setText(text);
    slot.statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(color));
}

void SensorConnectDialog::refreshButtons()
{
    m_btnContinue->setEnabled(anyConnected());
}
