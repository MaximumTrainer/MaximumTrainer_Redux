#include "dialogconfig.h"
#include "setting_keys.h"
#include "ui_dialogconfig.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QPushButton>
#include <QFileDialog>
#include <QDesktopServices>
#include <QSettings>
#include <QMessageBox>
#include <QApplication>

#include "workoutdialog.h"
#include "account.h"
#include "util.h"


DialogConfig::~DialogConfig() {


    if (parentDialog)
        AppSettings::write(SettingKeys::Media::RadioVolume, ui->slider_volumeRadio->value());


    delete ui;
}


DialogConfig::DialogConfig(QList<Radio> lstRadio, QWidget *parent,  WorkoutDialog *ptrParent) : QDialog(parent), ui(new Ui::DialogConfig) {


    ui->setupUi(this);


    currentRadioName = "";
    isPlayingRadio = false;

    /// List widgets
    ui->listWidget_settings->setIconSize(QSize(24, 24));

    // What riders change while pedalling comes first; the rest is also in the
    // Settings tab. Each row carries its page.
    const auto addRow = [this](const char *iconPath, const QString &label, Page page) {
        auto *item = new QListWidgetItem(QIcon(QString::fromLatin1(iconPath)), label, ui->listWidget_settings);
        item->setSizeHint(QSize(35, 35));
        item->setData(Qt::UserRole, int(page));
    };
    addRow(":/image/icon/radio",   tr("Radio"),       RadioPage);
    addRow(":/image/icon/sound",   tr("Sounds"),      SoundsPage);
    addRow(":/image/icon/display", tr("Panels"),      WidgetsPage);
    addRow(":/image/icon/chart",   tr("Graph"),       GraphPage);
    addRow(":/image/icon/movie",   tr("Video"),       VideoPage);
    addRow(":/image/icon/general", tr("Start && alerts").replace(QStringLiteral("&&"), QStringLiteral("&")), GeneralPage);

    // The interval-summary overlay setting lives at the bottom of the General
    // tab now (the standalone "Workout"/Studio tabs were removed). Studio mode
    // is configured on the main-window Studio page; logging in Preferences.
    buildIntervalSummaryGroup();


    connect(ui->listWidget_settings, &QListWidget::currentRowChanged, this, &DialogConfig::currentListViewSelectionChanged);

    ui->listWidget_settings->setCurrentRow(0);
    ///-----


    isHoldingSliderSound = true;
    playOnNextSliderRelease = false;

    this->parentDialog = ptrParent;
    /// Set up initial settings
    //    this->settings = qApp->property("User_Settings").value<Settings*>();
    this->account = qApp->property("Account").value<Account*>();
    this->soundPlayer =  qApp->property("SoundPlayer").value<SoundPlayer*>();


    connect(ui->horizontalSlider_volume, &QSlider::valueChanged, this, &DialogConfig::sliderValueMoved);
    connect(ui->horizontalSlider_volume, &QSlider::sliderPressed, this, &DialogConfig::sliderSoundPressed);
    connect(ui->horizontalSlider_volume, &QSlider::sliderReleased, this, &DialogConfig::sliderSoundReleased);


    initUi();
    isHoldingSliderSound = false;


    /// ------------------------------ Internet Radio --------------------------------------
    tableModel = new RadioTableModel(this);
    tableModel->addListRadio(lstRadio);
    ui->tableView_radio->setModel(tableModel);

    ui->tableView_radio->verticalHeader()->setDefaultSectionSize(30);
    ui->tableView_radio->verticalHeader()->setVisible(false);

    selectLastStation();

    // Name, genre, bitrate, language: the two texts share the room.
    QHeaderView *radioHeader = ui->tableView_radio->horizontalHeader();
    radioHeader->setStretchLastSection(false);
    radioHeader->setSectionResizeMode(QHeaderView::ResizeToContents);
    radioHeader->setSectionResizeMode(0, QHeaderView::Stretch);
    radioHeader->setSectionResizeMode(1, QHeaderView::Stretch);


    ui->slider_volumeRadio->setValue(AppSettings::read(SettingKeys::Media::RadioVolume));


    connect(ui->tableView_radio, &TableViewHover::doubleClicked, this, &DialogConfig::RadioDoubleClicked);
    // Play starts the station you picked, not the last one played.
    connect(ui->tableView_radio, &TableViewHover::clicked, this, [this](const QModelIndex &index) { currentRadioIndex = index; });

    ui->pushButton_prevRadio->setVisible(false);
    ui->pushButton_nextRadio->setVisible(false);


    connect(ui->slider_volumeRadio, &QSlider::valueChanged, this, &DialogConfig::signal_volumeRadioChanged);
    connect(ui->pushButton_playPause, &QPushButton::clicked, this, &DialogConfig::playPauseRadio);


}


// Preselect the station from the last session (falls back to the first row),
// so play resumes on the same radio.
void DialogConfig::selectLastStation()
{
    const QString lastStation = AppSettings::read(SettingKeys::Media::LastRadioStation);
    const QList<Radio> radios = tableModel->getAllRadios();
    int lastRow = 0;
    for (int row = 0; row < radios.size(); ++row) {
        if (!lastStation.isEmpty() && radios.at(row).getName() == lastStation) {
            lastRow = row;
            break;
        }
    }
    currentRadioIndex = tableModel->index(lastRow, 0);
    ui->tableView_radio->selectionModel()->select(
        currentRadioIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    ui->tableView_radio->scrollTo(currentRadioIndex);
}


void DialogConfig::on_pushButton_prevRadio_clicked()
{
    playNextOrPreviousRadio(false);
}

void DialogConfig::on_pushButton_nextRadio_clicked()
{

    playNextOrPreviousRadio(true);
}

void DialogConfig::radioVolumeUp()
{
    ui->slider_volumeRadio->setValue(ui->slider_volumeRadio->value() + 5);
}

void DialogConfig::radioVolumeDown()
{
    ui->slider_volumeRadio->setValue(ui->slider_volumeRadio->value() - 5);
}

int DialogConfig::radioVolume() const
{
    return ui->slider_volumeRadio->value();
}


void DialogConfig::playNextOrPreviousRadio(bool next) {


    QModelIndex indexToSelect;


    if (next) {
        if (currentRadioIndex.row()+1 < ui->tableView_radio->model()->rowCount()) {
            indexToSelect = ui->tableView_radio->model()->index(currentRadioIndex.row()+1, 0);
        }
        else {
            indexToSelect = ui->tableView_radio->model()->index(0, 0);
        }
    }
    else {
        if (currentRadioIndex.row()-1 > -1) {
            indexToSelect = ui->tableView_radio->model()->index(currentRadioIndex.row()-1, 0);
        }
        else {
            indexToSelect = ui->tableView_radio->model()->index(ui->tableView_radio->model()->rowCount()-1, 0);
        }
    }

    ui->tableView_radio->selectionModel()->select(indexToSelect, QItemSelectionModel::Select);
    ui->tableView_radio->scrollTo(indexToSelect);
    RadioDoubleClicked(indexToSelect);


}


void DialogConfig::RadioDoubleClicked(QModelIndex index) {


    currentRadioIndex = index;


    Radio radio = tableModel->getRadioAtRow(index);
    currentRadioName = radio.getName();

    // Remember the station so the next workout session starts on the same one.
    AppSettings::write(SettingKeys::Media::LastRadioStation, currentRadioName);


    isConnecting = true;
    QString status = tr("Connecting to ") + currentRadioName + "...";
    ui->label_statusRadio->setText(status);
    ui->label_statusRadio->fadeIn(400);
    emit radioStatus(status);


    tableModel->setActiveIndex(index);

    ui->tableView_radio->repaint();

    emit signal_connectToRadioUrl(radio.getUrl());
    emit signal_volumeRadioChanged(ui->slider_volumeRadio->value());


}


void DialogConfig::radioStartedPlaying() {

    isConnecting = false;
    QString status =  "<b>" + currentRadioName + "</b>";
    ui->label_statusRadio->setText(status);
    ui->label_statusRadio->fadeIn(400);
    emit radioStatus(status);

    ui->pushButton_playPause->setText(tr("Pause"));
    isPlayingRadio = true;

}

void DialogConfig::radioStoppedPlaying() {
    // When switching stations the player briefly stops to change source.
    // Re-emit the "Connecting to..." status so the top bar doesn't lose it.
    if (isConnecting) {
        QString status = tr("Connecting to ") + currentRadioName + "...";
        emit radioStatus(status);
        isPlayingRadio = false;
        return;
    }
    ui->label_statusRadio->setText("");
    emit radioStatus("");

    ui->pushButton_playPause->setText(tr("Play"));
    isPlayingRadio = false;
}


void DialogConfig::playPauseRadio() {


    if (isPlayingRadio) {
        emit signal_stopPlayingRadio();
    }
    else {
        RadioDoubleClicked(currentRadioIndex);
    }


}


void DialogConfig::currentListViewSelectionChanged(int section) {

    if (const QListWidgetItem *item = ui->listWidget_settings->item(section))
        ui->stackedWidget->setCurrentIndex(item->data(Qt::UserRole).toInt());
}

QList<QWidget *> DialogConfig::takePagesForSettings()
{
    QList<QWidget *> pages;
    while (ui->stackedWidget->count() > 0) {
        QWidget *page = ui->stackedWidget->widget(0);
        ui->stackedWidget->removeWidget(page);
        page->setParent(nullptr);
        pages.append(page);
    }
#ifndef GC_HAVE_QTMULTIMEDIA
    for (QWidget *control : std::initializer_list<QWidget *>{ui->label, ui->slider_volumeRadio, ui->pushButton_playPause,
                                                             ui->label_statusRadio})
        control->hide();
    disconnect(ui->tableView_radio, &TableViewHover::doubleClicked, this, &DialogConfig::RadioDoubleClicked);
#endif
    connect(ui->slider_volumeRadio, &QSlider::valueChanged, this, [](int value) {
        AppSettings::write(SettingKeys::Media::RadioVolume, value);
    });
    ui->tableView_radio->setMinimumHeight(260);

    for (QWidget *page : std::as_const(pages)) {
        const QList<QCheckBox *> boxes = page->findChildren<QCheckBox *>();
        for (QCheckBox *box : boxes)
            connect(box, &QCheckBox::clicked, this, &DialogConfig::saveSettings);
        const QList<QComboBox *> combos = page->findChildren<QComboBox *>();
        for (QComboBox *combo : combos)
            connect(combo, &QComboBox::activated, this, &DialogConfig::saveSettings);
        const QList<QSpinBox *> spins = page->findChildren<QSpinBox *>();
        for (QSpinBox *spin : spins)
            connect(spin, &QSpinBox::editingFinished, this, &DialogConfig::saveSettings);
    }
    connect(ui->lineEdit_homePage, &QLineEdit::editingFinished, this, &DialogConfig::saveSettings);
    connect(ui->horizontalSlider_volume, &QSlider::sliderReleased, this, &DialogConfig::saveSettings);
    return pages;
}

void DialogConfig::reloadFromAccount()
{
    isHoldingSliderSound = true;   // no test sound while loading
    initUi();
    isHoldingSliderSound = false;
    tableModel->setRadios(Util::loadLocalRadioList());
    selectLastStation();
    const QSignalBlocker quiet(ui->slider_volumeRadio);
    ui->slider_volumeRadio->setValue(AppSettings::read(SettingKeys::Media::RadioVolume));
}


void DialogConfig::sliderSoundPressed() {
    isHoldingSliderSound = true;
    playOnNextSliderRelease = true;
}
void DialogConfig::sliderSoundReleased() {
    isHoldingSliderSound = false;

    if (playOnNextSliderRelease) {
        playSoundTestEffect();
        playOnNextSliderRelease = false;
    }
}
void DialogConfig::sliderValueMoved(int value) {

    ui->label_valueVolume->setText(QString::number(value)+ "%");

    if (!isHoldingSliderSound) {
        playSoundTestEffect();
    }
}
void DialogConfig::playSoundTestEffect() {
    soundPlayer->setVolume(ui->horizontalSlider_volume->value());
    soundPlayer->playSoundEffectTest();


}

void DialogConfig::initUi() {


    /// Show last openned tab
    for (int row = 0; row < ui->listWidget_settings->count(); ++row)
        if (ui->listWidget_settings->item(row)->data(Qt::UserRole).toInt() == account->last_index_selected_config_workout)
            ui->listWidget_settings->setCurrentRow(row);

    // Init the programmatic tabs
    initIntervalSummary();


    ///Timers
    ui->checkBox_showIntervalRemainingTime->setChecked(account->show_interval_remaining);
    ui->checkBox_showWorkoutRemainingTime->setChecked(account->show_workout_remaining);
    ui->checkBox_showElapsedTime->setChecked(account->show_elapsed);
    ui->checkBox_showCurrentTarget->setChecked(account->show_current_target);

    int startTrigger = account->start_trigger;
    /// 0 - Cadence
    if (startTrigger == 0) {
        ui->comboBox_startTrigger->setCurrentIndex(0);
        ui->label_isOver->setVisible(true);
        ui->label_value->setVisible(true);
        ui->spinBox_value->setVisible(true);
        ui->label_value->setText(tr("rpm"));
        ui->spinBox_value->setValue(account->value_cadence_start);
    }
    /// 1- Power
    else if (startTrigger == 1) {
        ui->comboBox_startTrigger->setCurrentIndex(1);
        ui->label_isOver->setVisible(true);
        ui->label_value->setVisible(true);
        ui->spinBox_value->setVisible(true);
        ui->label_value->setText(tr("watts"));
        ui->spinBox_value->setValue(account->value_power_start);
    }
    /// 2 - Speed
    else if (startTrigger == 2){
        ui->comboBox_startTrigger->setCurrentIndex(2);
        ui->label_isOver->setVisible(true);
        ui->label_value->setVisible(true);
        ui->spinBox_value->setVisible(true);
        if (account->distance_in_km)
            ui->label_value->setText(tr("km/h"));
        else ///MPH
            ui->label_value->setText(tr("mph"));
        ui->spinBox_value->setValue(account->value_speed_start);
    }
    /// 2 - Button
    else {
        ui->comboBox_startTrigger->setCurrentIndex(3);
        ui->label_isOver->setVisible(false);
        ui->label_value->setVisible(false);
        ui->spinBox_value->setVisible(false);
    }


    ///Hr widget
    ui->checkBox_enableHR->setChecked(account->show_hr_widget);
    ui->comboBox_displayHR->setEnabled(account->show_hr_widget);

    ///Power widget
    ui->checkBox_enablePower->setChecked(account->show_power_widget);
    ui->comboBox_displayPower->setEnabled(account->show_power_widget);

    ///Power balance widget
    ui->checkBox_enablePowerBalance->setChecked(account->show_power_balance_widget);
    ui->comboBox_displayPowerBalance->setEnabled(account->show_power_balance_widget);

    ///Cadence widget
    ui->checkBox_enableCadence->setChecked(account->show_cadence_widget);
    ui->comboBox_displayCadence->setEnabled(account->show_cadence_widget);

    ///Speed widget
    ui->checkBox_enableSpeed->setChecked(account->show_speed_widget);

    ///Speed widget
    ui->checkBox_showTrainerSpeed->setChecked(account->show_trainer_speed);


    ///Calories widget
    ui->checkBox_enableCalories->setChecked(account->show_calories_widget);

    ///Oxygen widget
    ui->checkBox_enableOxygen->setChecked(account->show_oxygen_widget);


    /// Display
    // Display value 1=Detailed, 2=Graph, 3=Graph&Detailed (Minimalist/0 removed),
    // so the combo index is value-1.
    ui->comboBox_displayHR->setCurrentIndex(account->display_hr - 1);
    ui->comboBox_displayPower->setCurrentIndex(account->display_power - 1);
    ui->comboBox_displayCadence->setCurrentIndex(account->display_cadence - 1);
    ui->comboBox_displayPowerBalance->setCurrentIndex(account->display_power_balance);

    const bool webBrowser = account->display_video == 1;
    ui->comboBox_displayVideo->setCurrentIndex(webBrowser ? 1 : 0);
    ui->label_homePage->setVisible(webBrowser);
    ui->lineEdit_homePage->setVisible(webBrowser);

    ui->lineEdit_homePage->setText(AppSettings::readWorkoutBrowserHomePage());

    ui->checkBox_seperator->setChecked(account->show_seperator_interval);
    ui->checkBox_showGrid->setChecked(account->show_grid);
    ui->checkBox_targetHr->setChecked(account->show_hr_target);
    ui->checkBox_targetCadence->setChecked(account->show_cadence_target);
    ui->checkBox_targetPower->setChecked(account->show_power_target);
    ui->checkBox_CadenceCurve->setChecked(account->show_cadence_curve);
    ui->checkBox_HeartRateCurve->setChecked(account->show_hr_curve);
    ui->checkBox_PowerCurve->setChecked(account->show_power_curve);
    ui->checkBox_SpeedCurve->setChecked(account->show_speed_curve);

    ui->horizontalSlider_volume->setValue(account->sound_player_vol);
    ui->label_valueVolume->setText(QString::number(account->sound_player_vol) + "%");
    ui->checkBox_sound->setChecked(account->enable_sound);
    ui->checkBox_intervalSound->setChecked(account->sound_interval);
    ui->checkBox_achievementSound->setChecked(account->sound_achievement);
    ui->checkBox_pauseStartSound->setChecked(account->sound_pause_resume_workout);
    ui->checkBox_endWorkoutSound->setChecked(account->sound_end_workout);

    ui->checkBox_underTargetPower->setChecked(account->sound_alert_power_under_target);
    ui->checkBox_aboveTargetPower->setChecked(account->sound_alert_power_above_target);
    ui->checkBox_underTargetCadence->setChecked(account->sound_alert_cadence_under_target);
    ui->checkBox_aboveTargetCadence->setChecked(account->sound_alert_cadence_above_target);

    ui->spinBox_value_intervalmessage_duration_sec->setValue(account->nb_sec_show_interval);

}


/// SHOW & HIDE TARGET
void DialogConfig::on_checkBox_targetPower_clicked(bool checked) {
    if (parentDialog) parentDialog->mainPlot->showHideTargetPower(checked);
}
void DialogConfig::on_checkBox_targetCadence_clicked(bool checked) {
    if (parentDialog) parentDialog->mainPlot->showHideTargetCadence(checked);
}
void DialogConfig::on_checkBox_targetHr_clicked(bool checked) {
    if (parentDialog) parentDialog->mainPlot->showHideTargetHr(checked);
}
void DialogConfig::on_checkBox_PowerCurve_clicked(bool checked) {
    if (parentDialog) parentDialog->mainPlot->showHideCurvePower(checked);
}
void DialogConfig::on_checkBox_CadenceCurve_clicked(bool checked) {
    if (parentDialog) parentDialog->mainPlot->showHideCurveCadence(checked);
}
void DialogConfig::on_checkBox_HeartRateCurve_clicked(bool checked) {
    if (parentDialog) parentDialog->mainPlot->showHideCurveHeartRate(checked);
}
void DialogConfig::on_checkBox_SpeedCurve_clicked(bool checked) {
    if (parentDialog) parentDialog->mainPlot->showHideCurveSpeed(checked);
}

void DialogConfig::on_checkBox_seperator_clicked(bool checked)
{
    if (parentDialog) parentDialog->mainPlot->showHideSeperator(checked);
}
void DialogConfig::on_checkBox_showGrid_clicked(bool checked)
{
    if (parentDialog) parentDialog->mainPlot->showHideGrid(checked);
}


/// Timers



void DialogConfig::on_checkBox_showIntervalRemainingTime_clicked(bool checked)
{

    if (parentDialog) parentDialog->showTimerIntervalRemaining(checked);
}

void DialogConfig::on_checkBox_showWorkoutRemainingTime_clicked(bool checked)
{

    if (parentDialog) parentDialog->showTimerWorkoutRemaining(checked);
}

void DialogConfig::on_checkBox_showElapsedTime_clicked(bool checked)
{

    if (parentDialog) parentDialog->showTimerWorkoutElapsed(checked);
}

void DialogConfig::on_checkBox_showCurrentTarget_clicked(bool checked)
{
    if (parentDialog) parentDialog->showTimerCurrentTarget(checked);
}

/// CHANGE DISPLAY WIDGET

void DialogConfig::on_checkBox_enableHR_clicked(bool checked) {

    account->show_hr_widget = checked;
    ui->comboBox_displayHR->setEnabled(checked);

    if (checked) {
        if (parentDialog) parentDialog->showHeartRateDisplayWidget(ui->comboBox_displayHR->currentIndex() + 1);
    }
    else {
        if (parentDialog) parentDialog->showHeartRateDisplayWidget(-1);
    }

}
//---------
void DialogConfig::on_comboBox_displayHR_currentIndexChanged(int index) {

    if (ui->checkBox_enableHR->isChecked())
        if (parentDialog) parentDialog->showHeartRateDisplayWidget(index + 1);
}

void DialogConfig::on_checkBox_showTrainerSpeed_clicked(bool checked)
{

    account->show_trainer_speed = checked;
    if (parentDialog) parentDialog->showTrainerSpeed(checked);
}


void DialogConfig::on_checkBox_enablePower_clicked(bool checked) {

    account->show_power_widget = checked;
    ui->comboBox_displayPower->setEnabled(checked);

    if (checked) {
        if (parentDialog) parentDialog->showPowerDisplayWidget(ui->comboBox_displayPower->currentIndex() + 1);
    }
    else {
        if (parentDialog) parentDialog->showPowerDisplayWidget(-1);
    }

}
//---------
void DialogConfig::on_comboBox_displayPower_currentIndexChanged(int index) {

    if (ui->checkBox_enablePower->isChecked())
        if (parentDialog) parentDialog->showPowerDisplayWidget(index + 1);
}

void DialogConfig::on_checkBox_enablePowerBalance_clicked(bool checked) {

    account->show_power_balance_widget = checked;
    ui->comboBox_displayPowerBalance->setEnabled(checked);

    if (checked) {
        if (parentDialog) parentDialog->showPowerBalanceWidget(ui->comboBox_displayPowerBalance->currentIndex());
    }
    else {
        if (parentDialog) parentDialog->showPowerBalanceWidget(-1);
    }
}
//---------
void DialogConfig::on_comboBox_displayPowerBalance_currentIndexChanged(int index) {

    if (ui->checkBox_enablePowerBalance->isChecked())
        if (parentDialog) parentDialog->showPowerBalanceWidget(index);
}

void DialogConfig::on_checkBox_enableCadence_clicked(bool checked) {

    account->show_cadence_widget = checked;
    ui->comboBox_displayCadence->setEnabled(checked);

    if (checked) {
        if (parentDialog) parentDialog->showCadenceDisplayWidget(ui->comboBox_displayCadence->currentIndex() + 1);
    }
    else {
        if (parentDialog) parentDialog->showCadenceDisplayWidget(-1);
    }
}
//---------
void DialogConfig::on_comboBox_displayCadence_currentIndexChanged(int index) {

    if (ui->checkBox_enableCadence->isChecked())
        if (parentDialog) parentDialog->showCadenceDisplayWidget(index + 1);
}

void DialogConfig::on_checkBox_enableSpeed_clicked(bool checked) {

    account->show_speed_widget = checked;
    if (parentDialog) parentDialog->showSpeedDisplayWidget();

}
void DialogConfig::on_checkBox_enableOxygen_clicked(bool checked)
{
    account->show_oxygen_widget = checked;
    if (parentDialog) parentDialog->showOxygenDisplayWidget();
}


void DialogConfig::on_checkBox_enableCalories_clicked(bool checked) {

    account->show_calories_widget = checked;
    if (parentDialog) parentDialog->showCaloriesDisplayWidget();

}


/// Start workout with 0- Cadence 1- Power 2- Button
void DialogConfig::on_comboBox_startTrigger_activated(int index) {

    if (index == 0) {
        ui->label_isOver->setVisible(true);
        ui->label_value->setVisible(true);
        ui->spinBox_value->setVisible(true);
        ui->label_value->setText(tr("rpm"));
        ui->spinBox_value->setValue(account->value_cadence_start);
    }
    else if (index == 1) {
        ui->label_isOver->setVisible(true);
        ui->label_value->setVisible(true);
        ui->spinBox_value->setVisible(true);
        ui->label_value->setText(tr("watts"));
        ui->spinBox_value->setValue(account->value_power_start);
    }
    else if (index == 2) {
        ui->label_isOver->setVisible(true);
        ui->label_value->setVisible(true);
        ui->spinBox_value->setVisible(true);
        if (account->distance_in_km)
            ui->label_value->setText(tr("km/h"));
        else ///MPH
            ui->label_value->setText(tr("mph"));
        ui->spinBox_value->setValue(account->value_speed_start);
    }
    else {
        ui->label_isOver->setVisible(false);
        ui->label_value->setVisible(false);
        ui->spinBox_value->setVisible(false);
    }
    saveSettings();

}


void DialogConfig::setCourseMode() {
    for (int row = 0; row < ui->listWidget_settings->count(); ++row) {
        QListWidgetItem *item = ui->listWidget_settings->item(row);
        const int page = item->data(Qt::UserRole).toInt();
        item->setHidden(page != SoundsPage && page != RadioPage);
        if (page == RadioPage)
            ui->listWidget_settings->setCurrentRow(row);
    }
    setWindowTitle(tr("Course Settings"));
}


void DialogConfig::on_comboBox_displayVideo_activated(int index) {
    if (parentDialog) parentDialog->showVideoPlayer(index);

    // Persist the choice immediately so the next workout opens on the same pane,
    // even if the user closes this dialog without pressing OK.
    account->display_video = index;
    account->saveDisplayPrefs();

    //web browser
    ui->label_homePage->setVisible(index == 1);
    ui->lineEdit_homePage->setVisible(index == 1);

}

void DialogConfig::accept() {

    saveSettings();
    QDialog::accept();
}
void DialogConfig::reject() {

    // Every change is already applied live and there is no Cancel button, so
    // closing with the window's X or Esc keeps the choices like OK does.
    saveSettings();
    QDialog::reject();
}

void DialogConfig::saveSettings() {


    /// Save last openned tab
    if (parentDialog && ui->listWidget_settings->currentItem())
        account->last_index_selected_config_workout = ui->listWidget_settings->currentItem()->data(Qt::UserRole).toInt();



    int startTrigger = ui->comboBox_startTrigger->currentIndex();


    /// 0 - Cadence
    if (startTrigger == 0) {
        account->start_trigger = 0;
        account->value_cadence_start = ui->spinBox_value->value();
    }
    /// 1- Power
    else if (startTrigger == 1) {
        account->start_trigger = 1;
        account->value_power_start = ui->spinBox_value->value();
    }
    else if (startTrigger == 2) {
        account->start_trigger = 2;
        account->value_speed_start = ui->spinBox_value->value();
    }
    /// 2 - Button
    else {
        account->start_trigger = 3;
    }


    /// Timers
    account->show_interval_remaining = ui->checkBox_showIntervalRemainingTime->isChecked();
    account->show_workout_remaining =  ui->checkBox_showWorkoutRemainingTime->isChecked();
    account->show_elapsed = ui->checkBox_showElapsedTime->isChecked();
    account->show_current_target = ui->checkBox_showCurrentTarget->isChecked();


    /// Display widgets
    account->display_hr = ui->comboBox_displayHR->currentIndex() + 1;
    account->display_power = ui->comboBox_displayPower->currentIndex() + 1;
    account->display_cadence = ui->comboBox_displayCadence->currentIndex() + 1;
    account->display_power_balance = ui->comboBox_displayPowerBalance->currentIndex();

    account->display_video =  ui->comboBox_displayVideo->currentIndex();


    account->show_seperator_interval = ui->checkBox_seperator->isChecked();
    account->show_grid = ui->checkBox_showGrid->isChecked();
    account->show_cadence_target = ui->checkBox_targetCadence->isChecked();
    account->show_hr_target = ui->checkBox_targetHr->isChecked();
    account->show_power_target = ui->checkBox_targetPower->isChecked();
    account->show_cadence_curve = ui->checkBox_CadenceCurve->isChecked();
    account->show_hr_curve = ui->checkBox_HeartRateCurve->isChecked();
    account->show_power_curve = ui->checkBox_PowerCurve->isChecked();
    account->show_speed_curve = ui->checkBox_SpeedCurve->isChecked();


    account->sound_player_vol = ui->horizontalSlider_volume->value();
    account->enable_sound = ui->checkBox_sound->isChecked();
    account->sound_interval = ui->checkBox_intervalSound->isChecked();
    account->sound_pause_resume_workout = ui->checkBox_pauseStartSound->isChecked();
    account->sound_achievement = ui->checkBox_achievementSound->isChecked();
    account->sound_end_workout = ui->checkBox_endWorkoutSound->isChecked();

    account->sound_alert_power_under_target = ui->checkBox_underTargetPower->isChecked();
    account->sound_alert_power_above_target = ui->checkBox_aboveTargetPower->isChecked();
    account->sound_alert_cadence_under_target = ui->checkBox_underTargetCadence->isChecked();
    account->sound_alert_cadence_above_target = ui->checkBox_aboveTargetCadence->isChecked();


    AppSettings::write(SettingKeys::Media::WorkoutBrowserHomePage, ui->lineEdit_homePage->text());

    // Persist display & sound prefs locally (the server putAccount endpoint is
    // defunct), so the user's choices — including the video player — survive a
    // restart.
    account->saveDisplayPrefs();

    // Save the new preference tabs
    saveIntervalSummary();

    if (parentDialog) parentDialog->setMessagePlot(); //update message start workout
}




void DialogConfig::on_spinBox_value_intervalmessage_duration_sec_valueChanged(int arg1)
{
    account->saveNbSecShowInterval(arg1);
}


void DialogConfig::on_spinBox_value_intervalmessage_before_valueChanged(int arg1)
{
    account->saveNbSecShowIntervalBefore(arg1);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Group C — Programmatic preference tabs
// ═══════════════════════════════════════════════════════════════════════════════

// ─── Interval-summary overlay (appended to the General tab) ──────────────────
void DialogConfig::buildIntervalSummaryGroup()
{
    auto *grpSummary = new QGroupBox(tr("Interval Summary Overlay"));
    auto *grpSummaryLayout = new QVBoxLayout(grpSummary);

    checkIntervalSummary = new QCheckBox(tr("Show summary overlay on interval transition"), grpSummary);
    grpSummaryLayout->addWidget(checkIntervalSummary);

    auto *durRow = new QHBoxLayout();
    durRow->addWidget(new QLabel(tr("Display duration (seconds):"), grpSummary));
    spinIntervalSummaryDuration = new QSpinBox(grpSummary);
    spinIntervalSummaryDuration->setRange(2, 15);
    spinIntervalSummaryDuration->setValue(5);
    // Native Windows spin buttons crowd a short value; reserve room.
    spinIntervalSummaryDuration->setMinimumWidth(80);
    durRow->addWidget(spinIntervalSummaryDuration);
    durRow->addStretch();
    grpSummaryLayout->addLayout(durRow);

    // Insert just before the trailing vertical spacer so the box sits directly
    // under the Interval Message group, with the spacer keeping the slack at the
    // bottom (appending would place it after the spacer, pushing it down).
    ui->verticalLayout_2->insertWidget(ui->verticalLayout_2->count() - 1, grpSummary);
}


void DialogConfig::initIntervalSummary()
{
    if (!checkIntervalSummary) return;

    checkIntervalSummary->setChecked(account->interval_summary_enabled);
    spinIntervalSummaryDuration->setValue(account->interval_summary_duration_s);
}

void DialogConfig::saveIntervalSummary()
{
    if (!checkIntervalSummary) return;

    account->interval_summary_enabled    = checkIntervalSummary->isChecked();
    account->interval_summary_duration_s = spinIntervalSummaryDuration->value();
    account->saveIntervalSummarySettings();
}


// ============================================================================
//  Radio list editor — Add / Edit / Delete
// ============================================================================

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include "util.h"
#include "asyncdialogs.h"
#include <functional>

namespace {
/// Tiny editor for a single Radio, shown via open() (exec() is fatal on WASM).
/// onAccepted receives the edited copy once the input passed validation.
/// Lives here (not in its own .h/.cpp pair) because it is only used by the
/// three Add/Edit/Delete handlers below.
void runRadioEditDialog(QWidget *parent, const Radio &initial,
                        std::function<void(const Radio &)> onAccepted) {
    auto *dlg = new QDialog(parent);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(QObject::tr("Radio Station"));

    auto *nameEdit    = new QLineEdit(initial.getName());
    auto *genreEdit   = new QLineEdit(initial.getGenre());
    auto *langEdit    = new QLineEdit(initial.getLanguage());
    auto *urlEdit     = new QLineEdit(initial.getUrl());
    urlEdit->setPlaceholderText(QStringLiteral("https://stream.example.com/stream.mp3"));
    auto *bitrateSpin = new QSpinBox();
    bitrateSpin->setRange(0, 1000);
    bitrateSpin->setSuffix(QStringLiteral(" kbps"));
    bitrateSpin->setValue(initial.getBitrate());

    auto *form = new QFormLayout();
    form->addRow(QObject::tr("Name"),     nameEdit);
    form->addRow(QObject::tr("Genre"),    genreEdit);
    form->addRow(QObject::tr("Language"), langEdit);
    form->addRow(QObject::tr("Bitrate"),  bitrateSpin);
    form->addRow(QObject::tr("URL"),      urlEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    /// Minimal validation, checked before accepting so the dialog stays open
    /// on bad input: name and url are required. We don't try to validate the
    /// URL itself — the player will reject anything unparseable on Play.
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, [dlg, nameEdit, urlEdit]() {
        if (nameEdit->text().trimmed().isEmpty() || urlEdit->text().trimmed().isEmpty()) {
            AsyncDialogs::warning(dlg, QObject::tr("Radio Station"),
                QObject::tr("Name and URL are required."));
            return;
        }
        dlg->accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    QObject::connect(dlg, &QDialog::accepted, dlg,
                     [nameEdit, genreEdit, langEdit, urlEdit, bitrateSpin,
                      onAccepted = std::move(onAccepted)]() {
        onAccepted(Radio(nameEdit->text().trimmed(),
                         genreEdit->text().trimmed(),
                         /*gotAds=*/false,
                         bitrateSpin->value(),
                         langEdit->text().trimmed(),
                         urlEdit->text().trimmed()));
    });

    auto *layout = new QVBoxLayout(dlg);
    layout->addLayout(form);
    layout->addWidget(buttons);
    dlg->resize(420, dlg->sizeHint().height());
    dlg->open();
}
}


void DialogConfig::on_pushButton_addRadio_clicked() {

    Radio newRadio(QString(), QString(), false, 128, QString(), QString());
    runRadioEditDialog(ui->tableView_radio->window(), newRadio, [this](const Radio &edited) {
        tableModel->addRadio(edited);
        Util::saveLocalRadioList(tableModel->getAllRadios());
    });
}


void DialogConfig::on_pushButton_editRadio_clicked() {

    QModelIndex idx = ui->tableView_radio->selectionModel()->currentIndex();
    if (!idx.isValid())
        idx = currentRadioIndex;
    if (!idx.isValid()) {
        AsyncDialogs::information(ui->tableView_radio->window(), tr("Edit Radio"),
            tr("Select a radio station to edit."));
        return;
    }

    const int row = idx.row();
    const Radio existing = tableModel->getRadioAtRow(idx);
    runRadioEditDialog(ui->tableView_radio->window(), existing, [this, row](const Radio &edited) {
        tableModel->replaceRadioAtRow(row, edited);
        Util::saveLocalRadioList(tableModel->getAllRadios());
    });
}


void DialogConfig::on_pushButton_deleteRadio_clicked() {

    QModelIndex idx = ui->tableView_radio->selectionModel()->currentIndex();
    if (!idx.isValid())
        idx = currentRadioIndex;
    if (!idx.isValid()) {
        AsyncDialogs::information(ui->tableView_radio->window(), tr("Delete Radio"),
            tr("Select a radio station to delete."));
        return;
    }

    const int row = idx.row();
    const Radio radio = tableModel->getRadioAtRow(idx);

    AsyncDialogs::question(ui->tableView_radio->window(), tr("Delete Radio"),
                           tr("Delete \"%1\"?").arg(radio.getName()),
                           [this, row]() {
        tableModel->removeRadioAtRow(row);
        if (currentRadioIndex.row() == row)
            currentRadioIndex = QModelIndex();
        Util::saveLocalRadioList(tableModel->getAllRadios());
    });
}
