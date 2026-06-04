#include "sensorswidget.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

#ifndef GC_WASM_BUILD
#include "btle_sensor_store.h"
#include "btle_scanner_dialog.h"
#endif

SensorsWidget::SensorsWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    reload();
}

void SensorsWidget::buildUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QLabel *title = new QLabel(tr("Bluetooth Sensors"), this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 3);
    titleFont.setBold(true);
    title->setFont(titleFont);
    mainLayout->addWidget(title);

#ifdef GC_WASM_BUILD
    QLabel *unavailable = new QLabel(
        tr("Bluetooth sensor pairing is not available in the web version."), this);
    unavailable->setWordWrap(true);
    unavailable->setStyleSheet(QStringLiteral("color: #777;"));
    mainLayout->addWidget(unavailable);
    mainLayout->addStretch();
    return;
#else
    QGroupBox *group = new QGroupBox(tr("Saved Sensors"), this);
    QGridLayout *grid = new QGridLayout(group);
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(8);

    int row = 0;
    for (BtleSensorRole role : BtleSensorStore::allRoles()) {
        SlotRow slot;
        slot.role = role;

        QLabel *roleLabel = new QLabel(BtleSensorStore::roleDisplayName(role), group);

        slot.deviceLabel = new QLabel(group);
        slot.deviceLabel->setStyleSheet(QStringLiteral("color: #777;"));

        slot.scanButton  = new QPushButton(tr("Scan…"), group);
        slot.scanButton->setFixedWidth(90);
        slot.clearButton = new QPushButton(tr("Clear"), group);
        slot.clearButton->setFixedWidth(70);

        const int rowIndex = m_rows.size();
        connect(slot.scanButton,  &QPushButton::clicked, this,
                [this, rowIndex]() { onScanClicked(rowIndex); });
        connect(slot.clearButton, &QPushButton::clicked, this,
                [this, rowIndex]() { onClearClicked(rowIndex); });

        grid->addWidget(roleLabel,        row, 0);
        grid->addWidget(slot.deviceLabel, row, 1);
        grid->addWidget(slot.scanButton,  row, 2);
        grid->addWidget(slot.clearButton, row, 3);

        m_rows.append(slot);
        ++row;
    }

    mainLayout->addWidget(group);

    QLabel *hint = new QLabel(
        tr("Saved sensors are connected automatically when you start a workout."), this);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    mainLayout->addWidget(hint);

    mainLayout->addStretch();
#endif
}

void SensorsWidget::reload()
{
#ifndef GC_WASM_BUILD
    const QMap<BtleSensorRole, BtleSavedSensor> saved = BtleSensorStore::loadAll();
    for (int i = 0; i < m_rows.size(); ++i) {
        m_rows[i].current = saved.value(m_rows[i].role, BtleSavedSensor{});
        m_rows[i].current.role = m_rows[i].role;
        refreshRow(i);
    }
#endif
}

void SensorsWidget::refreshRow(int rowIndex)
{
    SlotRow &slot = m_rows[rowIndex];
    if (!slot.deviceLabel)
        return;

    if (slot.current.isValid()) {
        slot.deviceLabel->setText(slot.current.name);
        slot.deviceLabel->setStyleSheet(QString());
        slot.clearButton->setEnabled(true);
    } else {
        slot.deviceLabel->setText(tr("(none saved)"));
        slot.deviceLabel->setStyleSheet(QStringLiteral("color: #777;"));
        slot.clearButton->setEnabled(false);
    }
}

void SensorsWidget::onScanClicked(int rowIndex)
{
#ifndef GC_WASM_BUILD
    BtleScannerDialog scanner(this);
    if (scanner.exec() != QDialog::Accepted || !scanner.hasSelection())
        return;

    m_rows[rowIndex].current =
        BtleSensorStore::fromDeviceInfo(m_rows[rowIndex].role, scanner.selectedDevice());
    BtleSensorStore::saveSensor(m_rows[rowIndex].current);   // persist immediately
    refreshRow(rowIndex);
#else
    Q_UNUSED(rowIndex);
#endif
}

void SensorsWidget::onClearClicked(int rowIndex)
{
#ifndef GC_WASM_BUILD
    BtleSensorStore::clearSensor(m_rows[rowIndex].role);
    m_rows[rowIndex].current = BtleSavedSensor{};
    m_rows[rowIndex].current.role = m_rows[rowIndex].role;
    refreshRow(rowIndex);
#else
    Q_UNUSED(rowIndex);
#endif
}
