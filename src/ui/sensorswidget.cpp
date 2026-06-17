#include "sensorswidget.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QApplication>

#include "account.h"

#ifndef GC_WASM_BUILD
#include "btle_sensor_store.h"
#include "btle_scanner_dialog.h"
#endif

SensorsWidget::SensorsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_account = qApp->property("Account").value<Account*>();

    // Paint the page with the palette's Window colour. The light stylesheet only
    // backgrounds a few named widgets and trusts the palette elsewhere, so without
    // this the MainWindow chrome gradient shows through this page in light mode.
    setAutoFillBackground(true);
    buildUi();
    reload();
}

void SensorsWidget::buildUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    QLabel *title = new QLabel(tr("Devices"), this);
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

    // Trainer control toggle. Lives here, next to the Trainer slot, rather than
    // in a separate Preferences page. Drives account->control_trainer_resistance,
    // which gates ERG setpoints over BLE FTMS (and legacy ANT FE-C) at runtime.
    QGroupBox *trainerGroup = new QGroupBox(tr("Smart Trainer"), this);
    QVBoxLayout *trainerLayout = new QVBoxLayout(trainerGroup);
    trainerLayout->setSpacing(4);

    m_controlResistanceCheck =
        new QCheckBox(tr("Control trainer resistance (ERG / FTMS)"), trainerGroup);
    connect(m_controlResistanceCheck, &QCheckBox::toggled,
            this, &SensorsWidget::onControlResistanceToggled);
    trainerLayout->addWidget(m_controlResistanceCheck);

    QLabel *trainerHint = new QLabel(
        tr("When on, MaximumTrainer sets your smart trainer's target power "
           "during a workout (ERG mode). Turn off to ride a structured workout "
           "while controlling resistance yourself."), trainerGroup);
    trainerHint->setWordWrap(true);
    trainerHint->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    trainerLayout->addWidget(trainerHint);

    m_virtualShiftingCheck =
        new QCheckBox(tr("Virtual shifting (Zwift Cog / single-gear setups)"), trainerGroup);
    connect(m_virtualShiftingCheck, &QCheckBox::toggled,
            this, &SensorsWidget::onVirtualShiftingToggled);
    trainerLayout->addWidget(m_virtualShiftingCheck);

    QLabel *vsHint = new QLabel(
        tr("Adds ▲/▼ gear shifting — by keyboard or the on-screen arrows — so a "
           "single-cog trainer has usable resistance on free rides and tests. "
           "Leave off if you shift with a real cassette."), trainerGroup);
    vsHint->setWordWrap(true);
    vsHint->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    trainerLayout->addWidget(vsHint);

    m_useZwiftClickCheck =
        new QCheckBox(tr("Use Zwift Click v2 controller (right side only) - Beta"), trainerGroup);
    connect(m_useZwiftClickCheck, &QCheckBox::toggled,
            this, &SensorsWidget::onUseZwiftClickToggled);
    trainerLayout->addWidget(m_useZwiftClickCheck);

    QLabel *clickHint = new QLabel(
        tr("Wake the controller before the workout. Right side: Y = gear up, "
           "B = gear down, A/Z = radio next/prev, + paddle = start/pause. The left "
           "side isn't supported yet."), trainerGroup);
    clickHint->setWordWrap(true);
    clickHint->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    trainerLayout->addWidget(clickHint);

    QHBoxLayout *ergRampRow = new QHBoxLayout();
    ergRampRow->addWidget(new QLabel(tr("ERG transition ramp duration:"), trainerGroup));
    m_ergRampSpin = new QSpinBox(trainerGroup);
    m_ergRampSpin->setRange(0, 30);
    m_ergRampSpin->setSuffix(tr(" s"));
    // Native Windows spin buttons crowd a short value/suffix; reserve room.
    m_ergRampSpin->setMinimumWidth(80);
    m_ergRampSpin->setToolTip(tr("Seconds to linearly ramp ERG resistance between intervals.\n"
                                 "Set to 0 to disable smoothing (instant resistance changes)."));
    connect(m_ergRampSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &SensorsWidget::onErgRampChanged);
    ergRampRow->addWidget(m_ergRampSpin);
    ergRampRow->addStretch();
    trainerLayout->addLayout(ergRampRow);

    QLabel *ergRampHint = new QLabel(
        tr("Ramps resistance gradually when transitioning between interval "
           "targets, reducing mechanical jolt on the trainer."), trainerGroup);
    ergRampHint->setWordWrap(true);
    ergRampHint->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    trainerLayout->addWidget(ergRampHint);

    mainLayout->addWidget(trainerGroup);

    // Sensor dropout auto-pause.
    QGroupBox *dropoutGroup = new QGroupBox(tr("Sensor Dropout"), this);
    QVBoxLayout *dropoutLayout = new QVBoxLayout(dropoutGroup);
    dropoutLayout->setSpacing(4);

    m_dropoutEnabledCheck =
        new QCheckBox(tr("Auto-pause workout when sensor signal is lost"), dropoutGroup);
    connect(m_dropoutEnabledCheck, &QCheckBox::toggled,
            this, &SensorsWidget::onSensorDropoutChanged);
    dropoutLayout->addWidget(m_dropoutEnabledCheck);

    QHBoxLayout *dropoutRow = new QHBoxLayout();
    dropoutRow->addWidget(new QLabel(tr("Dropout timeout:"), dropoutGroup));
    m_dropoutTimeoutSpin = new QSpinBox(dropoutGroup);
    m_dropoutTimeoutSpin->setRange(2, 30);
    m_dropoutTimeoutSpin->setSuffix(tr(" s"));
    m_dropoutTimeoutSpin->setMinimumWidth(80);
    connect(m_dropoutTimeoutSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &SensorsWidget::onSensorDropoutChanged);
    dropoutRow->addWidget(m_dropoutTimeoutSpin);
    dropoutRow->addStretch();
    dropoutLayout->addLayout(dropoutRow);

    QLabel *dropoutHint = new QLabel(
        tr("Workout resumes automatically 3 seconds after the signal is restored."),
        dropoutGroup);
    dropoutHint->setWordWrap(true);
    dropoutHint->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    dropoutLayout->addWidget(dropoutHint);

    mainLayout->addWidget(dropoutGroup);

    // Low-battery warning threshold.
    QGroupBox *batteryGroup = new QGroupBox(tr("Battery Warning"), this);
    QVBoxLayout *batteryLayout = new QVBoxLayout(batteryGroup);
    batteryLayout->setSpacing(4);

    QHBoxLayout *batteryRow = new QHBoxLayout();
    batteryRow->addWidget(new QLabel(tr("Warn when battery drops below:"), batteryGroup));
    m_batteryThresholdSpin = new QSpinBox(batteryGroup);
    m_batteryThresholdSpin->setRange(5, 50);
    m_batteryThresholdSpin->setSuffix(tr(" %"));
    m_batteryThresholdSpin->setMinimumWidth(80);
    connect(m_batteryThresholdSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &SensorsWidget::onBatteryThresholdChanged);
    batteryRow->addWidget(m_batteryThresholdSpin);
    batteryRow->addStretch();
    batteryLayout->addLayout(batteryRow);

    QLabel *batteryHint = new QLabel(
        tr("A notification is shown when a sensor reports a battery level at or "
           "below this threshold."), batteryGroup);
    batteryHint->setWordWrap(true);
    batteryHint->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    batteryLayout->addWidget(batteryHint);

    mainLayout->addWidget(batteryGroup);

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

    if (m_account) {
        if (m_controlResistanceCheck) {
            QSignalBlocker b(m_controlResistanceCheck);
            m_controlResistanceCheck->setChecked(m_account->control_trainer_resistance);
        }
        if (m_virtualShiftingCheck) {
            QSignalBlocker b(m_virtualShiftingCheck);
            m_virtualShiftingCheck->setChecked(m_account->virtual_shifting);
        }
        if (m_useZwiftClickCheck) {
            QSignalBlocker b(m_useZwiftClickCheck);
            m_useZwiftClickCheck->setChecked(m_account->use_zwift_click);
        }
        if (m_ergRampSpin) {
            QSignalBlocker b(m_ergRampSpin);
            m_ergRampSpin->setValue(m_account->erg_smoothing_duration_s);
        }
        if (m_dropoutEnabledCheck) {
            QSignalBlocker b(m_dropoutEnabledCheck);
            m_dropoutEnabledCheck->setChecked(m_account->sensor_dropout_enabled);
        }
        if (m_dropoutTimeoutSpin) {
            QSignalBlocker b(m_dropoutTimeoutSpin);
            m_dropoutTimeoutSpin->setValue(m_account->sensor_dropout_timeout_s);
        }
        if (m_batteryThresholdSpin) {
            QSignalBlocker b(m_batteryThresholdSpin);
            m_batteryThresholdSpin->setValue(m_account->battery_warning_threshold);
        }
    }
#endif
}

void SensorsWidget::onControlResistanceToggled(bool checked)
{
    if (!m_account)
        return;
    m_account->control_trainer_resistance = checked;
    m_account->saveDisplayPrefs();
}

void SensorsWidget::onVirtualShiftingToggled(bool checked)
{
    if (!m_account)
        return;
    m_account->virtual_shifting = checked;
    m_account->saveDisplayPrefs();
}

void SensorsWidget::onUseZwiftClickToggled(bool checked)
{
    if (!m_account)
        return;
    m_account->use_zwift_click = checked;
    m_account->saveDisplayPrefs();
}

void SensorsWidget::onErgRampChanged(int seconds)
{
    if (!m_account)
        return;
    m_account->saveErgSmoothingDuration(seconds);
}

void SensorsWidget::onSensorDropoutChanged()
{
    if (!m_account)
        return;
    if (m_dropoutEnabledCheck)
        m_account->sensor_dropout_enabled = m_dropoutEnabledCheck->isChecked();
    if (m_dropoutTimeoutSpin)
        m_account->sensor_dropout_timeout_s = m_dropoutTimeoutSpin->value();
    m_account->saveSensorDropoutSettings();
}

void SensorsWidget::onBatteryThresholdChanged(int percent)
{
    if (!m_account)
        return;
    m_account->battery_warning_threshold = percent;
    m_account->saveBatteryWarningThreshold();
}

bool SensorsWidget::trainerPaired() const
{
    for (const SlotRow &row : m_rows)
        if (row.role == BtleSensorRole::Trainer && row.current.isValid())
            return true;
    return false;
}

void SensorsWidget::refreshRow(int rowIndex)
{
    SlotRow &slot = m_rows[rowIndex];
    if (!slot.deviceLabel)
        return;

    // A paired FTMS trainer reports power, cadence and speed over one
    // connection, so its dedicated slots are redundant — disable scanning and
    // show why. A previously-saved device stays clearable so it can be removed.
    // (BtleSensorStore is desktop-only; the WASM build never reaches here.)
#ifndef GC_WASM_BUILD
    const bool trainerCoversRole = BtleSensorStore::roleCoveredByTrainer(slot.role);
    // HR is only trainer-covered when the trainer was seen bridging a strap and
    // no dedicated strap is saved (a saved strap takes precedence over it).
    const bool trainerCoversHr   = slot.role == BtleSensorRole::HeartRate
                                   && !slot.current.isValid()
                                   && BtleSensorStore::trainerProvidesHr();
    if (trainerPaired() && (trainerCoversRole || trainerCoversHr)) {
        // Power/Cadence are always carried by FTMS, so their dedicated slots are
        // redundant — disable scanning. HR can stop (strap removed from the
        // trainer), so leave its slot scannable and just annotate it.
        slot.scanButton->setEnabled(trainerCoversHr);
        slot.clearButton->setEnabled(slot.current.isValid());
        slot.deviceLabel->setText(slot.current.isValid()
                                  ? tr("%1 (provided by trainer)").arg(slot.current.name)
                                  : tr("Provided by trainer"));
        slot.deviceLabel->setStyleSheet(QStringLiteral("color: #777;"));
        return;
    }
#endif

    slot.scanButton->setEnabled(true);
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
    BtleScannerDialog scanner(m_rows[rowIndex].role, this);
    if (scanner.exec() != QDialog::Accepted || !scanner.hasSelection())
        return;

    m_rows[rowIndex].current =
        BtleSensorStore::fromDeviceInfo(m_rows[rowIndex].role, scanner.selectedDevice());
    BtleSensorStore::saveSensor(m_rows[rowIndex].current);   // persist immediately
    // Refresh every row: pairing the Trainer collapses the Power/Cadence slots.
    for (int i = 0; i < m_rows.size(); ++i)
        refreshRow(i);
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
    for (int i = 0; i < m_rows.size(); ++i)
        refreshRow(i);
#else
    Q_UNUSED(rowIndex);
#endif
}
