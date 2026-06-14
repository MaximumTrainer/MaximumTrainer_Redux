#include "btle_scanner_dialog.h"
#include "ui_btle_scanner_dialog.h"

#include <QDebug>
#include <QMessageBox>

#include "btle_sensor_store.h"

// ─────────────────────────────────────────────────────────────────────────────
BtleScannerDialog::BtleScannerDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BtleScannerDialog)
{
    init();
}

BtleScannerDialog::BtleScannerDialog(BtleSensorRole role, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::BtleScannerDialog)
    , m_role(role)
    , m_hasRoleFilter(true)
{
    init();
}

void BtleScannerDialog::init()
{
    ui->setupUi(this);
    setWindowTitle(tr("Select Bluetooth Device"));

    if (m_hasRoleFilter) {
        ui->label_title->setText(
            tr("Nearby %1 sensors").arg(BtleSensorStore::roleDisplayName(m_role)));
    } else {
        // No role → nothing to filter by; hide the toggle entirely.
        ui->checkBox_showAll->hide();
    }

    m_model = new QStandardItemModel(this);
    m_model->setHorizontalHeaderLabels({ tr("Device Name"), tr("Address") });
    ui->tableView_devices->setModel(m_model);
    ui->tableView_devices->horizontalHeader()->setStretchLastSection(true);
    ui->tableView_devices->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView_devices->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView_devices->setEditTriggers(QAbstractItemView::NoEditTriggers);

    ui->pushButton_connect->setEnabled(false);

    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    m_discoveryAgent->setLowEnergyDiscoveryTimeout(SCAN_TIMEOUT_MS);

    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BtleScannerDialog::onDeviceDiscovered);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BtleScannerDialog::onScanFinished);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred,
            this, &BtleScannerDialog::onScanError);
#else
    connect(m_discoveryAgent,
            static_cast<void(QBluetoothDeviceDiscoveryAgent::*)
                (QBluetoothDeviceDiscoveryAgent::Error)>(&QBluetoothDeviceDiscoveryAgent::error),
            this, &BtleScannerDialog::onScanError);
#endif

    connect(ui->pushButton_scan,    &QPushButton::clicked,
            this, &BtleScannerDialog::startScan);
    connect(ui->pushButton_stop,    &QPushButton::clicked,
            this, &BtleScannerDialog::stopScan);
    connect(ui->pushButton_connect, &QPushButton::clicked,
            this, &BtleScannerDialog::onConnectClicked);
    // Double-clicking a device connects to it, like double-clicking a file.
    connect(ui->tableView_devices, &QAbstractItemView::doubleClicked,
            this, &BtleScannerDialog::onConnectClicked);
    connect(ui->tableView_devices->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this, &BtleScannerDialog::onSelectionChanged);
    connect(ui->checkBox_showAll, &QCheckBox::toggled,
            this, [this]() { rebuildVisibleList(); });

    startScan();
}

BtleScannerDialog::~BtleScannerDialog()
{
    if (m_discoveryAgent->isActive())
        m_discoveryAgent->stop();
    delete ui;
}

// ─────────────────────────────────────────────────────────────────────────────
void BtleScannerDialog::startScan()
{
    m_model->removeRows(0, m_model->rowCount());
    m_devices.clear();
    m_allDiscovered.clear();
    ui->label_status->setText(tr("Scanning..."));
    ui->pushButton_scan->setEnabled(false);
    ui->pushButton_stop->setEnabled(true);

    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
}

void BtleScannerDialog::stopScan()
{
    m_discoveryAgent->stop();
}

void BtleScannerDialog::onDeviceDiscovered(const QBluetoothDeviceInfo &device)
{
    // Only show BLE devices
    if (!(device.coreConfigurations() & QBluetoothDeviceInfo::LowEnergyCoreConfiguration))
        return;

    // Track every discovery so the list can be rebuilt when the user toggles
    // the "Show all devices" filter, then add it to the visible list if it
    // matches the current filter.
    for (int i = 0; i < m_allDiscovered.size(); ++i) {
        if (m_allDiscovered[i].address() == device.address()) {
            m_allDiscovered[i] = device;
            if (passesFilter(device))
                addOrUpdateDevice(device);
            return;
        }
    }
    m_allDiscovered.append(device);
    if (passesFilter(device))
        addOrUpdateDevice(device);
}

bool BtleScannerDialog::passesFilter(const QBluetoothDeviceInfo &device) const
{
    if (!m_hasRoleFilter || ui->checkBox_showAll->isChecked())
        return true;

    const QList<QBluetoothUuid> wanted = BtleSensorStore::serviceUuidsForRole(m_role);
    if (wanted.isEmpty())
        return true;

    // Strict match: only devices that advertise the matching GATT service are
    // shown. Real sensors (HR straps, power meters, FTMS trainers) advertise
    // their primary service; unrelated gadgets (phones, TVs) do not, and most
    // advertise no service UUIDs at all. A sensor that doesn't advertise its
    // service is recovered via the "Show all devices" toggle.
    const QList<QBluetoothUuid> advertised = device.serviceUuids();
    for (const QBluetoothUuid &uuid : advertised) {
        if (wanted.contains(uuid))
            return true;
    }
    return false;
}

void BtleScannerDialog::rebuildVisibleList()
{
    const QString previouslySelected =
        m_selectedDevice.isValid() ? m_selectedDevice.address().toString() : QString();

    m_model->removeRows(0, m_model->rowCount());
    m_devices.clear();
    for (const QBluetoothDeviceInfo &device : m_allDiscovered) {
        if (passesFilter(device))
            addOrUpdateDevice(device);
    }

    // Preserve the user's selection across a filter toggle when still visible.
    if (!previouslySelected.isEmpty()) {
        for (int i = 0; i < m_devices.size(); ++i) {
            if (m_devices[i].address().toString() == previouslySelected) {
                ui->tableView_devices->selectRow(i);
                break;
            }
        }
    }

    if (!m_discoveryAgent->isActive())
        updateStatusComplete();
}

void BtleScannerDialog::addOrUpdateDevice(const QBluetoothDeviceInfo &device)
{
    for (int i = 0; i < m_devices.size(); ++i) {
        if (m_devices[i].address() == device.address()) {
            m_devices[i] = device;
            QString name = device.name().isEmpty() ? tr("(unknown)") : device.name();
            m_model->item(i, 0)->setText(name);
            return;
        }
    }

    m_devices.append(device);
    QString name = device.name().isEmpty() ? tr("(unknown)") : device.name();
    QList<QStandardItem*> row;
    row << new QStandardItem(name)
        << new QStandardItem(device.address().toString());
    m_model->appendRow(row);
}

void BtleScannerDialog::updateStatusComplete()
{
    if (m_hasRoleFilter && !ui->checkBox_showAll->isChecked()
        && m_allDiscovered.size() > m_devices.size()) {
        ui->label_status->setText(
            tr("Scan complete. %1 matching device(s) shown (%2 hidden).")
                .arg(m_devices.size())
                .arg(m_allDiscovered.size() - m_devices.size()));
    } else {
        ui->label_status->setText(
            tr("Scan complete. %1 device(s) found.").arg(m_devices.size()));
    }
}

void BtleScannerDialog::onScanFinished()
{
    updateStatusComplete();
    ui->pushButton_scan->setEnabled(true);
    ui->pushButton_stop->setEnabled(false);
}

void BtleScannerDialog::onScanError(QBluetoothDeviceDiscoveryAgent::Error error)
{
    Q_UNUSED(error)
    ui->label_status->setText(tr("Scan error: %1").arg(m_discoveryAgent->errorString()));
    ui->pushButton_scan->setEnabled(true);
    ui->pushButton_stop->setEnabled(false);
}

void BtleScannerDialog::onSelectionChanged()
{
    bool hasRow = ui->tableView_devices->selectionModel()->hasSelection();
    ui->pushButton_connect->setEnabled(hasRow);
}

void BtleScannerDialog::onConnectClicked()
{
    QModelIndexList selected = ui->tableView_devices->selectionModel()->selectedRows();
    if (selected.isEmpty())
        return;

    int row = selected.first().row();
    if (row < 0 || row >= m_devices.size())
        return;

    m_selectedDevice = m_devices.at(row);

    if (m_discoveryAgent->isActive())
        m_discoveryAgent->stop();

    accept();
}
