#include "studiowidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QApplication>
#include <QSignalBlocker>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

#include "account.h"
#include "xmlutil.h"
#include "toggleswitch.h"

#ifndef GC_WASM_BUILD
#include "btle_sensor_store.h"
#include "btle_scanner_dialog.h"
#endif

namespace {
// The sensor roles offered per rider in Studio mode (Oxygen is omitted).
const QVector<BtleSensorRole> kStudioSensorRoles = {
    BtleSensorRole::HeartRate,
    BtleSensorRole::Power,
    BtleSensorRole::CadenceSpeed,
    BtleSensorRole::Trainer
};
}

StudioWidget::StudioWidget(QWidget *parent)
    : QWidget(parent)
{
    m_account = qApp->property("Account").value<Account*>();

    // Paint the page with the palette's Window colour (see SensorsWidget).
    setAutoFillBackground(true);
    buildUi();
    reload();
}

void StudioWidget::buildUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    QLabel *title = new QLabel(tr("Studio"), this);
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() + 3);
    titleFont.setBold(true);
    title->setFont(titleFont);
    mainLayout->addWidget(title);

    // Switch row — always visible and pinned at the top, so the Studio Mode
    // toggle never moves when the rest of the page is shown/hidden.
    QHBoxLayout *switchRow = new QHBoxLayout();
    switchRow->setSpacing(10);

    QLabel *enableLabel = new QLabel(tr("Studio Mode"), this);
    QFont enableFont = enableLabel->font();
    enableFont.setPointSizeF(enableFont.pointSizeF() + 1);
    enableFont.setBold(true);
    enableLabel->setFont(enableFont);
    switchRow->addWidget(enableLabel);

    m_enableSwitch = new ToggleSwitch(this);
    m_enableSwitch->setFixedSize(60, 30);
    m_enableSwitch->setToolTip(tr("Turn Studio Mode on or off"));
    connect(m_enableSwitch, &QAbstractButton::toggled,
            this, &StudioWidget::onStudioModeToggled);
    switchRow->addWidget(m_enableSwitch);
    switchRow->addStretch();
    mainLayout->addLayout(switchRow);

    // Controls row — rider count + import/export. Hidden when studio mode is off
    // (wrapped in a container so it collapses without disturbing the switch row).
    m_controlsRow = new QWidget(this);
    QHBoxLayout *controlsLayout = new QHBoxLayout(m_controlsRow);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(10);

    m_riderCountLabel = new QLabel(tr("Number of riders:"), m_controlsRow);
    controlsLayout->addWidget(m_riderCountLabel);
    m_riderCountCombo = new QComboBox(m_controlsRow);
    for (int i = 1; i <= kMaxRiders; ++i)
        m_riderCountCombo->addItem(QString::number(i), i);
    connect(m_riderCountCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &StudioWidget::onRiderCountChanged);
    controlsLayout->addWidget(m_riderCountCombo);
    controlsLayout->addStretch();

    // Share the whole studio setup (riders + sensors + ERG) as a single file so
    // it can be moved between computers without re-pairing.
    m_importButton = new QPushButton(tr("Import…"), m_controlsRow);
    m_importButton->setToolTip(tr("Load a studio configuration from a file"));
    connect(m_importButton, &QPushButton::clicked, this, &StudioWidget::onImportClicked);
    controlsLayout->addWidget(m_importButton);

    m_exportButton = new QPushButton(tr("Export…"), m_controlsRow);
    m_exportButton->setToolTip(tr("Save the current studio configuration to a file"));
    connect(m_exportButton, &QPushButton::clicked, this, &StudioWidget::onExportClicked);
    controlsLayout->addWidget(m_exportButton);

    mainLayout->addWidget(m_controlsRow);

    m_noteLabel = new QLabel(
        tr("Each rider trains with their own sensors. Leave a name blank to use "
           "Rider1, Rider2, … in the workout view."),
        this);
    m_noteLabel->setWordWrap(true);
    m_noteLabel->setStyleSheet(QStringLiteral("color: #777; font-size: 11px;"));
    mainLayout->addWidget(m_noteLabel);

    // Scrollable grid of compact rider cards (2 per row so several fit).
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);

    QScrollArea *scroll = m_scrollArea;
    QWidget *cardsHost = new QWidget(scroll);
    m_cardsGrid = new QGridLayout(cardsHost);
    m_cardsGrid->setContentsMargins(0, 0, 0, 0);
    m_cardsGrid->setHorizontalSpacing(10);
    m_cardsGrid->setVerticalSpacing(10);

    m_cards.resize(kMaxRiders);
    for (int rider = 1; rider <= kMaxRiders; ++rider) {
        QGroupBox *box = buildRiderCard(rider);
        m_cardsGrid->addWidget(box, (rider - 1) / 2, (rider - 1) % 2);
    }
    m_cardsGrid->setColumnStretch(0, 1);
    m_cardsGrid->setColumnStretch(1, 1);
    m_cardsGrid->setRowStretch(kMaxRiders, 1);

    scroll->setWidget(cardsHost);
    mainLayout->addWidget(scroll, 1);

    // When studio mode is off the cards/scroll are hidden; this expanding spacer
    // takes over the freed space so the switch row stays pinned at the top
    // instead of the layout re-centring the remaining widgets.
    m_bottomSpacer = new QWidget(this);
    m_bottomSpacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_bottomSpacer->setVisible(false);
    mainLayout->addWidget(m_bottomSpacer);
}

QGroupBox *StudioWidget::buildRiderCard(int riderIndex)
{
    RiderCard card;
    QGroupBox *box = new QGroupBox(tr("Rider %1").arg(riderIndex), this);

    QVBoxLayout *cardLayout = new QVBoxLayout(box);
    cardLayout->setContentsMargins(8, 6, 8, 6);
    cardLayout->setSpacing(4);

    // Identity row: name | FTP | LTHR.
    QHBoxLayout *idRow = new QHBoxLayout();
    idRow->setSpacing(6);
    card.nameEdit = new QLineEdit(box);
    card.nameEdit->setPlaceholderText(tr("Rider%1").arg(riderIndex));
    idRow->addWidget(card.nameEdit, 1);
    idRow->addWidget(new QLabel(tr("FTP"), box));
    card.ftpSpin = new QSpinBox(box);
    card.ftpSpin->setRange(0, 600);
    card.ftpSpin->setSuffix(tr(" W"));
    idRow->addWidget(card.ftpSpin);
    idRow->addWidget(new QLabel(tr("LTHR"), box));
    card.lthrSpin = new QSpinBox(box);
    card.lthrSpin->setRange(0, 230);
    idRow->addWidget(card.lthrSpin);
    cardLayout->addLayout(idRow);

    connect(card.nameEdit, &QLineEdit::editingFinished,
            this, [this, riderIndex]() { onNameChanged(riderIndex); });
    connect(card.ftpSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this, riderIndex]() { onFtpChanged(riderIndex); });
    connect(card.lthrSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this, riderIndex]() { onLthrChanged(riderIndex); });

#ifndef GC_WASM_BUILD
    // Sensor slots — same pattern as SensorsWidget, smaller buttons.
    QGridLayout *grid = new QGridLayout();
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(3);
    int row = 0;
    for (BtleSensorRole role : kStudioSensorRoles) {
        SensorSlot slot;
        slot.role = role;

        QLabel *roleLabel = new QLabel(BtleSensorStore::roleDisplayName(role), box);
        slot.deviceLabel = new QLabel(box);
        slot.deviceLabel->setStyleSheet(QStringLiteral("color: #777;"));

        QPushButton *scanButton = new QPushButton(tr("Scan…"), box);
        scanButton->setFixedWidth(70);
        slot.clearButton = new QPushButton(tr("Clear"), box);
        slot.clearButton->setFixedWidth(55);

        const int slotIdx = card.sensorSlots.size();
        connect(scanButton, &QPushButton::clicked,
                this, [this, riderIndex, slotIdx]() { onScanClicked(riderIndex, slotIdx); });
        connect(slot.clearButton, &QPushButton::clicked,
                this, [this, riderIndex, slotIdx]() { onClearClicked(riderIndex, slotIdx); });

        grid->addWidget(roleLabel,        row, 0);
        grid->addWidget(slot.deviceLabel, row, 1);
        grid->addWidget(scanButton,       row, 2);
        grid->addWidget(slot.clearButton, row, 3);

        card.sensorSlots.append(slot);
        ++row;
    }
    cardLayout->addLayout(grid);
#endif

    // Per-rider Smart Trainer ERG control.
    QHBoxLayout *ergRow = new QHBoxLayout();
    card.ergCheck = new QCheckBox(tr("Control trainer resistance (ERG / FTMS)"), box);
    ergRow->addWidget(card.ergCheck);
    ergRow->addStretch();
    ergRow->addWidget(new QLabel(tr("Ramp"), box));
    card.ergRampSpin = new QSpinBox(box);
    card.ergRampSpin->setRange(0, 30);
    card.ergRampSpin->setSuffix(tr(" s"));
    ergRow->addWidget(card.ergRampSpin);
    cardLayout->addLayout(ergRow);

    connect(card.ergCheck, &QCheckBox::toggled,
            this, [this, riderIndex]() { onErgChanged(riderIndex); });
    connect(card.ergRampSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, [this, riderIndex]() { onErgChanged(riderIndex); });

    card.box = box;
    m_cards[riderIndex - 1] = card;
    return box;
}

void StudioWidget::reload()
{
    if (!m_account)
        return;

    XmlUtil xmlUtil;
    m_riders = xmlUtil.parseUserStudioFile(QString());
    // Guarantee enough entries to back every card.
    while (m_riders.size() < kMaxRiders)
        m_riders.append(UserStudio("", -1, -1, -1, -1, -1, -1, -1, 2100, false, 0, 0));

    if (m_enableSwitch) {
        QSignalBlocker b(m_enableSwitch);
        m_enableSwitch->setChecked(m_account->enable_studio_mode);
        m_enableSwitch->setKnobPosition(m_account->enable_studio_mode ? 1.0 : 0.0);
    }

    const int count = qBound(1, m_account->nb_user_studio, kMaxRiders);
    if (m_riderCountCombo) {
        QSignalBlocker b(m_riderCountCombo);
        m_riderCountCombo->setCurrentIndex(count - 1);
    }

    for (int rider = 1; rider <= kMaxRiders; ++rider)
        loadRiderIntoCard(rider);
    updateVisibleCards(count);

    // Hide everything but the switch when studio mode is off.
    setStudioControlsVisible(m_account->enable_studio_mode);
}

void StudioWidget::loadRiderIntoCard(int riderIndex)
{
    RiderCard &card = m_cards[riderIndex - 1];
    const UserStudio &u = m_riders.at(riderIndex - 1);

    {
        QSignalBlocker b(card.nameEdit);
        card.nameEdit->setText(u.getDisplayName());
    }
    {
        QSignalBlocker b(card.ftpSpin);
        card.ftpSpin->setValue(u.getFTP() > 0 ? u.getFTP()
                                              : (m_account ? m_account->FTP : 0));
    }
    {
        QSignalBlocker b(card.lthrSpin);
        card.lthrSpin->setValue(u.getLTHR() > 0 ? u.getLTHR()
                                                : (m_account ? m_account->LTHR : 0));
    }

#ifndef GC_WASM_BUILD
    for (int s = 0; s < card.sensorSlots.size(); ++s)
        refreshSensorSlot(riderIndex, s);
#endif

    {
        QSignalBlocker bc(card.ergCheck);
        QSignalBlocker br(card.ergRampSpin);
        card.ergCheck->setChecked(
            studioErgControl(riderIndex, m_account && m_account->control_trainer_resistance));
        card.ergRampSpin->setValue(
            studioErgRamp(riderIndex, m_account ? m_account->erg_smoothing_duration_s : 0));
    }
}

void StudioWidget::refreshSensorSlot(int riderIndex, int slotIdx)
{
#ifndef GC_WASM_BUILD
    RiderCard &card = m_cards[riderIndex - 1];
    SensorSlot &slot = card.sensorSlots[slotIdx];

    const QMap<BtleSensorRole, BtleSavedSensor> saved = BtleSensorStore::loadAll(riderIndex);
    const BtleSavedSensor s = saved.value(slot.role, BtleSavedSensor{});

    if (s.isValid()) {
        slot.deviceLabel->setText(s.name);
        slot.deviceLabel->setStyleSheet(QString());
        slot.clearButton->setEnabled(true);
    } else {
        slot.deviceLabel->setText(tr("(none saved)"));
        slot.deviceLabel->setStyleSheet(QStringLiteral("color: #777;"));
        slot.clearButton->setEnabled(false);
    }
#else
    Q_UNUSED(riderIndex);
    Q_UNUSED(slotIdx);
#endif
}

void StudioWidget::updateVisibleCards(int count)
{
    for (int rider = 1; rider <= kMaxRiders; ++rider)
        if (m_cards[rider - 1].box)
            m_cards[rider - 1].box->setVisible(rider <= count);
}

void StudioWidget::setStudioControlsVisible(bool visible)
{
    // The Studio Mode switch row stays put; only the rest of the page collapses.
    if (m_controlsRow)  m_controlsRow->setVisible(visible);
    if (m_noteLabel)    m_noteLabel->setVisible(visible);
    if (m_scrollArea)   m_scrollArea->setVisible(visible);
    // The spacer fills the freed space (off) so nothing re-centres; off when the
    // cards are visible so the scroll area keeps the stretch instead.
    if (m_bottomSpacer) m_bottomSpacer->setVisible(!visible);
}

void StudioWidget::onStudioModeToggled(bool enabled)
{
    setStudioControlsVisible(enabled);
    emit studioModeChanged(enabled);
}

void StudioWidget::onRiderCountChanged(int index)
{
    const int count = index + 1;
    updateVisibleCards(count);
    emit riderCountChanged(count);
}

void StudioWidget::onNameChanged(int riderIndex)
{
    UserStudio u = m_riders.at(riderIndex - 1);
    u.setDisplayName(m_cards[riderIndex - 1].nameEdit->text());
    m_riders.replace(riderIndex - 1, u);
    persistRiders();
}

void StudioWidget::onFtpChanged(int riderIndex)
{
    UserStudio u = m_riders.at(riderIndex - 1);
    u.setFTP(m_cards[riderIndex - 1].ftpSpin->value());
    m_riders.replace(riderIndex - 1, u);
    persistRiders();
}

void StudioWidget::onLthrChanged(int riderIndex)
{
    UserStudio u = m_riders.at(riderIndex - 1);
    u.setLTHR(m_cards[riderIndex - 1].lthrSpin->value());
    m_riders.replace(riderIndex - 1, u);
    persistRiders();
}

void StudioWidget::onScanClicked(int riderIndex, int slotIdx)
{
#ifndef GC_WASM_BUILD
    SensorSlot &slot = m_cards[riderIndex - 1].sensorSlots[slotIdx];

    BtleScannerDialog scanner(slot.role, this);
    if (scanner.exec() != QDialog::Accepted || !scanner.hasSelection())
        return;

    BtleSavedSensor saved = BtleSensorStore::fromDeviceInfo(slot.role, scanner.selectedDevice());
    BtleSensorStore::saveSensor(saved, riderIndex);     // persist to the rider's package
    refreshSensorSlot(riderIndex, slotIdx);
#else
    Q_UNUSED(riderIndex);
    Q_UNUSED(slotIdx);
#endif
}

void StudioWidget::onClearClicked(int riderIndex, int slotIdx)
{
#ifndef GC_WASM_BUILD
    SensorSlot &slot = m_cards[riderIndex - 1].sensorSlots[slotIdx];
    BtleSensorStore::clearSensor(slot.role, riderIndex);
    refreshSensorSlot(riderIndex, slotIdx);
#else
    Q_UNUSED(riderIndex);
    Q_UNUSED(slotIdx);
#endif
}

void StudioWidget::onErgChanged(int riderIndex)
{
    RiderCard &card = m_cards[riderIndex - 1];
    saveErg(riderIndex, card.ergCheck->isChecked(), card.ergRampSpin->value());
}

void StudioWidget::persistRiders()
{
    XmlUtil::saveUserStudioFile(m_riders, QString());
    emit ridersChanged(m_riders);
}

QString StudioWidget::ergGroup(int riderIndex)
{
    return QStringLiteral("studioErg/rider%1").arg(riderIndex);
}

bool StudioWidget::studioErgControl(int riderIndex, bool defaultValue)
{
    QSettings settings;
    settings.beginGroup(ergGroup(riderIndex));
    const bool value = settings.value(QStringLiteral("control_resistance"), defaultValue).toBool();
    settings.endGroup();
    return value;
}

int StudioWidget::studioErgRamp(int riderIndex, int defaultValue)
{
    QSettings settings;
    settings.beginGroup(ergGroup(riderIndex));
    const int value = settings.value(QStringLiteral("ramp_s"), defaultValue).toInt();
    settings.endGroup();
    return value;
}

void StudioWidget::saveErg(int riderIndex, bool control, int ramp)
{
    QSettings settings;
    settings.beginGroup(ergGroup(riderIndex));
    settings.setValue(QStringLiteral("control_resistance"), control);
    settings.setValue(QStringLiteral("ramp_s"), ramp);
    settings.endGroup();
}

void StudioWidget::onExportClicked()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Studio Configuration"),
        QStringLiteral("studio_config.json"), tr("Studio configuration (*.json)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".json"), Qt::CaseInsensitive))
        path += QLatin1String(".json");

    QJsonObject root;
    root[QStringLiteral("version")]  = 1;
    root[QStringLiteral("nbRiders")] = m_riderCountCombo
                                           ? m_riderCountCombo->currentData().toInt()
                                           : 1;

    QJsonArray ridersArr;
    for (int rider = 1; rider <= kMaxRiders; ++rider) {
        const UserStudio &u = m_riders.at(rider - 1);

        QJsonObject ro;
        ro[QStringLiteral("index")] = rider;
        ro[QStringLiteral("name")]  = u.getDisplayName();
        ro[QStringLiteral("ftp")]   = u.getFTP();
        ro[QStringLiteral("lthr")]  = u.getLTHR();

        QJsonObject erg;
        erg[QStringLiteral("control")] =
            studioErgControl(rider, m_account && m_account->control_trainer_resistance);
        erg[QStringLiteral("ramp")] =
            studioErgRamp(rider, m_account ? m_account->erg_smoothing_duration_s : 0);
        ro[QStringLiteral("erg")] = erg;

#ifndef GC_WASM_BUILD
        QJsonObject sensors;
        const QMap<BtleSensorRole, BtleSavedSensor> saved = BtleSensorStore::loadAll(rider);
        for (BtleSensorRole role : kStudioSensorRoles) {
            if (!saved.contains(role))
                continue;
            const BtleSavedSensor &s = saved.value(role);
            QJsonObject so;
            so[QStringLiteral("name")]       = s.name;
            so[QStringLiteral("address")]    = s.address;
            so[QStringLiteral("deviceUuid")] = s.deviceUuid;
            sensors[BtleSensorStore::roleKey(role)] = so;
        }
        ro[QStringLiteral("sensors")] = sensors;
#endif
        ridersArr.append(ro);
    }
    root[QStringLiteral("riders")] = ridersArr;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Export failed"),
                             tr("Could not write to %1").arg(path));
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
}

void StudioWidget::onImportClicked()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import Studio Configuration"),
        QString(), tr("Studio configuration (*.json)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Import failed"),
                             tr("Could not read %1").arg(path));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QMessageBox::warning(this, tr("Import failed"),
                             tr("This is not a valid studio configuration file."));
        return;
    }

    const QJsonObject root = doc.object();
    const QJsonArray ridersArr = root.value(QStringLiteral("riders")).toArray();

    for (const QJsonValue &riderValue : ridersArr) {
        const QJsonObject ro = riderValue.toObject();
        const int rider = ro.value(QStringLiteral("index")).toInt();
        if (rider < 1 || rider > kMaxRiders)
            continue;

        UserStudio u = m_riders.at(rider - 1);
        u.setDisplayName(ro.value(QStringLiteral("name")).toString());
        u.setFTP(ro.value(QStringLiteral("ftp")).toInt());
        u.setLTHR(ro.value(QStringLiteral("lthr")).toInt());
        m_riders.replace(rider - 1, u);

        const QJsonObject erg = ro.value(QStringLiteral("erg")).toObject();
        saveErg(rider, erg.value(QStringLiteral("control")).toBool(),
                erg.value(QStringLiteral("ramp")).toInt());

#ifndef GC_WASM_BUILD
        const QJsonObject sensors = ro.value(QStringLiteral("sensors")).toObject();
        for (BtleSensorRole role : kStudioSensorRoles) {
            const QString key = BtleSensorStore::roleKey(role);
            if (sensors.contains(key)) {
                const QJsonObject so = sensors.value(key).toObject();
                BtleSavedSensor s;
                s.role       = role;
                s.name       = so.value(QStringLiteral("name")).toString();
                s.address    = so.value(QStringLiteral("address")).toString();
                s.deviceUuid = so.value(QStringLiteral("deviceUuid")).toString();
                s.enabled    = true;
                BtleSensorStore::saveSensor(s, rider);
            } else {
                BtleSensorStore::clearSensor(role, rider);
            }
        }
#endif
    }

    if (m_account) {
        m_account->nb_user_studio =
            qBound(1, root.value(QStringLiteral("nbRiders")).toInt(m_account->nb_user_studio),
                   kMaxRiders);
    }

    // Persist the identity changes, refresh every card, and tell MainWindow.
    XmlUtil::saveUserStudioFile(m_riders, QString());
    reload();
    emit ridersChanged(m_riders);
    if (m_account)
        emit riderCountChanged(m_account->nb_user_studio);
}
