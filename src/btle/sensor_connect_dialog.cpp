#include "sensor_connect_dialog.h"

#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "account.h"
#include "btle_sensor_store.h"

SensorConnectDialog::SensorConnectDialog(
        const QMap<BtleSensorRole, BtleSavedSensor> &savedSensors,
        QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Connecting Sensors"));

    const Account *account = qApp->property("Account").value<Account*>();
    m_connector = new SensorConnector(savedSensors, account && account->rowing_mode, this);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto *grid = new QGridLayout();
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(8);
    int row = 0;
    for (const SensorConnector::Slot &slot : m_connector->sensorSlots()) {
        auto *deviceLabel = new QLabel(slot.saved.name, this);
        deviceLabel->setStyleSheet(QStringLiteral("color: #777;"));
        auto *statusLabel = new QLabel(this);
        grid->addWidget(new QLabel(BtleSensorStore::roleDisplayName(slot.role), this), row, 0);
        grid->addWidget(deviceLabel, row, 1);
        grid->addWidget(statusLabel, row, 2);
        m_statusLabels.insert(slot.role, statusLabel);
        updateStatusLabel(slot.role, slot.status);
        ++row;
    }
    mainLayout->addLayout(grid);
    mainLayout->addStretch();

    auto *buttonRow = new QHBoxLayout();
    m_btnContinue = new QPushButton(tr("Continue"), this);
    m_btnContinue->setDefault(true);
    m_btnContinue->setEnabled(false);
    buttonRow->addStretch();
    buttonRow->addWidget(m_btnContinue);
    mainLayout->addLayout(buttonRow);
    connect(m_btnContinue, &QPushButton::clicked, this, &SensorConnectDialog::onContinueClicked);

    m_autoContinueTimer = new QTimer(this);
    m_autoContinueTimer->setSingleShot(true);
    connect(m_autoContinueTimer, &QTimer::timeout, this, [this]() {
        if (m_connector->anyConnected())
            onContinueClicked();
    });

    connect(m_connector, &SensorConnector::statusChanged, this, [this](BtleSensorRole role, SensorConnector::Status status) {
        updateStatusLabel(role, status);
        m_btnContinue->setEnabled(m_connector->anyConnected());
    });
    // Give in-flight connections a moment, then continue on its own so an
    // unattended start is not blocked on the button.
    connect(m_connector, &SensorConnector::scanFinished, this, [this]() {
        if (m_connector->anyConnected())
            m_autoContinueTimer->start(AUTO_CONTINUE_MS);
    });
    m_connector->start();
}

QMap<BtleSensorRole, BtleHub*> SensorConnectDialog::connectedHubs() const
{
    return m_connector->connectedHubs();
}

void SensorConnectDialog::detachHubs()
{
    m_connector->takeHubs();
}

void SensorConnectDialog::onContinueClicked()
{
    m_autoContinueTimer->stop();
    m_connector->stopScan();
    accept();
}

void SensorConnectDialog::updateStatusLabel(BtleSensorRole role, SensorConnector::Status status)
{
    QLabel *label = m_statusLabels.value(role, nullptr);
    if (!label)
        return;
    QString text;
    QString color = QStringLiteral("#777");
    switch (status) {
    case SensorConnector::Status::Searching:  text = tr("Searching…");  break;
    case SensorConnector::Status::Connecting: text = tr("Connecting…"); break;
    case SensorConnector::Status::Connected:  text = tr("✓ Connected"); color = QStringLiteral("green"); break;
    case SensorConnector::Status::NotFound:   text = tr("Not found");   color = QStringLiteral("red"); break;
    case SensorConnector::Status::Lost:       text = tr("Error");       color = QStringLiteral("red"); break;
    }
    label->setText(text);
    label->setStyleSheet(QStringLiteral("color: %1;").arg(color));
}
