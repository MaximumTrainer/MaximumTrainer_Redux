#include "dialogconfig.h"
#include "ui_dialogconfig.h"

#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QDesktopServices>
#include <QSettings>
#include <QMessageBox>
#include <QApplication>

#include "workoutdialog.h"
#include "account.h"



DialogConfig::~DialogConfig() {


    //Save volume radio
    QSettings settings;
    settings.beginGroup("radioPlayer");
    settings.setValue("volume", ui->slider_volumeRadio->value());
    settings.endGroup();


    delete ui;
}



DialogConfig::DialogConfig(QList<Radio> lstRadio, QWidget *parent,  WorkoutDialog *ptrParent) : QDialog(parent), ui(new Ui::DialogConfig) {


    qDebug() << "DialogConfig constructor";

    ui->setupUi(this);



    currentRadioName = "";
    isPlayingRadio = false;

    /// List widgets
    ui->listWidget_settings->setIconSize(QSize(24, 24));

    QListWidgetItem *item1 = new QListWidgetItem(QIcon(":/image/icon/general"), tr("General"), ui->listWidget_settings);
    QListWidgetItem *item2 = new QListWidgetItem(QIcon(":/image/icon/display"), tr("Widgets"), ui->listWidget_settings);
    QListWidgetItem *item3 = new QListWidgetItem(QIcon(":/image/icon/chart"),   tr("Graph"), ui->listWidget_settings);
    QListWidgetItem *item4 = new QListWidgetItem(QIcon(":/image/icon/sound"),   tr("Sounds"), ui->listWidget_settings);
    QListWidgetItem *item5 = new QListWidgetItem(QIcon(":/image/icon/movie"),   tr("Video Player"), ui->listWidget_settings);
    QListWidgetItem *item6 = new QListWidgetItem(QIcon(":/image/icon/radio"),   tr("Radio"), ui->listWidget_settings);
    item1->setSizeHint(QSize(35,35));
    item2->setSizeHint(QSize(35,35));
    item3->setSizeHint(QSize(35,35));
    item4->setSizeHint(QSize(35,35));
    item5->setSizeHint(QSize(35,35));
    item6->setSizeHint(QSize(35,35));

    ui->listWidget_settings->addItem(item1);
    ui->listWidget_settings->addItem(item2);
    ui->listWidget_settings->addItem(item3);
    ui->listWidget_settings->addItem(item4);
    ui->listWidget_settings->addItem(item5);
    ui->listWidget_settings->addItem(item6);

    // The interval-summary overlay setting lives at the bottom of the General
    // tab now (the standalone "Workout"/Studio tabs were removed). Studio mode
    // is configured on the main-window Studio page; logging in Preferences.
    buildIntervalSummaryGroup();


    connect(ui->listWidget_settings, SIGNAL(currentRowChanged(int)), this, SLOT(currentListViewSelectionChanged(int)) );

    ui->listWidget_settings->setCurrentRow(0);
    ///-----






    isHoldingSliderSound = true;
    playOnNextSliderRelease = false;

    this->parentDialog = ptrParent;
    /// Set up initial settings
    //    this->settings = qApp->property("User_Settings").value<Settings*>();
    this->account = qApp->property("Account").value<Account*>();
    this->soundPlayer =  qApp->property("SoundPlayer").value<SoundPlayer*>();


    connect(ui->horizontalSlider_volume, SIGNAL(valueChanged(int)), this, SLOT(sliderValueMoved(int)));
    connect(ui->horizontalSlider_volume, SIGNAL(sliderPressed()), this, SLOT(sliderSoundPressed()));
    connect(ui->horizontalSlider_volume, SIGNAL(sliderReleased()), this, SLOT(sliderSoundReleased()));


    initUi();
    isHoldingSliderSound = false;




    /// ------------------------------ Internet Radio --------------------------------------
    tableModel = new RadioTableModel(this);
    tableModel->addListRadio(lstRadio);
    ui->tableView_radio->setModel(tableModel);

    ui->tableView_radio->verticalHeader()->setDefaultSectionSize(30);
    ui->tableView_radio->verticalHeader()->setVisible(false);

    //first one
    currentRadioIndex = ui->tableView_radio->model()->index(0, 0);


    //    ui->tableView_radio->sortByColumn(0, Qt::AscendingOrder);
    ui->tableView_radio->setColumnWidth(0, 160);
    ui->tableView_radio->setColumnWidth(1, 160);
    ui->tableView_radio->setColumnWidth(2, 50);
    ui->tableView_radio->horizontalHeader()->setStretchLastSection(true);


    //-- restore last used volume
    QSettings settings;
    settings.beginGroup("radioPlayer");
    int volume = settings.value("volume", 100 ).toInt();
    settings.endGroup();
    ui->slider_volumeRadio->setValue(volume);



    connect(ui->tableView_radio, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(RadioDoubleClicked(QModelIndex)) );

    //    connect(ui->tableView_workout->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
    //            this, SLOT(tableViewSelectionChanged(QItemSelection,QItemSelection)));
    //    connect(ui->tableView_workout, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customMenuRequested(QPoint)) );
    //    connect(ui->tableView_workout->verticalHeader(), SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(customMenuRequested(QPoint)) );

    ui->pushButton_prevRadio->setVisible(false);
    ui->pushButton_nextRadio->setVisible(false);


    connect(ui->slider_volumeRadio, SIGNAL(valueChanged(int)), this, SIGNAL(signal_volumeRadioChanged(int)) );
    connect(ui->pushButton_playPause, SIGNAL(clicked()), this, SLOT(playPauseRadio()) );



}




void DialogConfig::on_pushButton_prevRadio_clicked()
{
    playNextOrPreviousRadio(false);
}

void DialogConfig::on_pushButton_nextRadio_clicked()
{

    playNextOrPreviousRadio(true);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::playNextOrPreviousRadio(bool next) {

    qDebug() << "playNextOrPreviousRadio" << next;

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


//---------------------------------------------------------------------------------------------
void DialogConfig::RadioDoubleClicked(QModelIndex index) {

    qDebug() << "RadioDoubleClicked" << index << "currentNow:" << currentRadioIndex;

    currentRadioIndex = index;
    qDebug() << "RadioDoubleClicked1" << index << "currentNow:" << currentRadioIndex;



    qDebug() << "Getting radio from model...";
    Radio radio = tableModel->getRadioAtRow(index);
    qDebug() << "Done getting radio from model...";
    currentRadioName = radio.getName();
    qDebug() << "Radio url is" << radio.getUrl();


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


//---------------------------------------------------------------------------------------------
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


////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::playPauseRadio() {


    if (isPlayingRadio) {
        emit signal_stopPlayingRadio();
    }
    else {
        qDebug() << "ok play this one.." << currentRadioIndex;
        RadioDoubleClicked(currentRadioIndex);
    }





}


//---------------------------------------------------------------------------------------------
void DialogConfig::currentListViewSelectionChanged(int section) {

    ui->stackedWidget->setCurrentIndex(section);

    //    if (section == 0) {
    //        ui->label_headerSettings->setText(tr("General"));
    //    }
    //    else if (section == 1) {
    //        ui->label_headerSettings->setText(tr("Widgets"));
    //    }
    //    else if (section == 2) {
    //        ui->label_headerSettings->setText(tr("Graph"));
    //    }
    //    else if (section == 3) {
    //        ui->label_headerSettings->setText(tr("Sounds"));
    //    }
    //    else if (section == 4) {
    //        ui->label_headerSettings->setText(tr("Video Player"));
    //    }
    //    else if (section == 5) {
    //        ui->label_headerSettings->setText(tr("Radio"));
    //    }
    //    else {
    //        ui->label_headerSettings->setText(tr("-"));
    //    }

}


//////////////////////////////////////////////////////////////////
void DialogConfig::sliderSoundPressed() {
    isHoldingSliderSound = true;
    playOnNextSliderRelease = true;
}
//----------------------------------------------
void DialogConfig::sliderSoundReleased() {
    isHoldingSliderSound = false;

    if (playOnNextSliderRelease) {
        playSoundTestEffect();
        playOnNextSliderRelease = false;
    }
}
//------------------------------------------------
void DialogConfig::sliderValueMoved(int value) {

    ui->label_valueVolume->setText(QString::number(value)+ "%");

    if (!isHoldingSliderSound) {
        playSoundTestEffect();
    }
}
//-----------------------------------------------
void DialogConfig::playSoundTestEffect() {
    soundPlayer->setVolume(ui->horizontalSlider_volume->value());
    soundPlayer->playSoundEffectTest();



}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::initUi() {


    /// Show last openned tab
    ui->listWidget_settings->setCurrentRow(account->last_index_selected_config_workout);
    /// Show last openned subtab
    QTabWidget *myTab = ui->stackedWidget->currentWidget()->findChild<QTabWidget*>();
    if (myTab)
        myTab->setCurrentIndex(account->last_tab_sub_config_selected);

    // Init the programmatic tabs
    initIntervalSummary();


    ///Timers
    ui->checkBox_showTimerOnTop->setChecked(account->show_timer_on_top);
    if (account->show_timer_on_top) {
        ui->comboBox_fontSizeTimer->setDisabled(true);
        ui->pushButton_leftTimer->setDisabled(true);
        ui->pushButton_rightTimer->setDisabled(true);
    }
    ui->checkBox_showIntervalRemainingTime->setChecked(account->show_interval_remaining);
    ui->checkBox_showWorkoutRemainingTime->setChecked(account->show_workout_remaining);
    ui->checkBox_showElapsedTime->setChecked(account->show_elapsed);
    ui->comboBox_fontSizeTimer->setCurrentText(QString::number(account->font_size_timer) );
    // Connect after setCurrentText() so seeding the initial value does not emit.
    connect(ui->comboBox_fontSizeTimer, &QComboBox::currentTextChanged,
            this, &DialogConfig::onTimerFontSizeChanged);

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
    ui->comboBox_powerAverage->setEnabled(account->show_power_widget);

    ///Power balance widget
    ui->checkBox_enablePowerBalance->setChecked(account->show_power_balance_widget);
    ui->comboBox_displayPowerBalance->setEnabled(account->show_power_balance_widget);

    ///Cadence widget
    ui->checkBox_enableCadence->setChecked(account->show_cadence_widget);
    ui->comboBox_displayCadence->setEnabled(account->show_cadence_widget);

    ///Speed widget
    ui->checkBox_enableSpeed->setChecked(account->show_speed_widget);

    ///virtual speed
    if (account->use_virtual_speed) {
        ui->comboBox_virtualSpeed->setCurrentIndex(0);
    }
    else {
        ui->comboBox_virtualSpeed->setCurrentIndex(1);
    }

    ///Speed widget
    ui->checkBox_showTrainerSpeed->setChecked(account->show_trainer_speed);


    ///Calories widget
    ui->checkBox_enableCalories->setChecked(account->show_calories_widget);

    ///Oxygen widget
    ui->checkBox_enableOxygen->setChecked(account->show_oxygen_widget);


    /// Display
    ui->comboBox_displayHR->setCurrentIndex(account->display_hr);
    ui->comboBox_displayPower->setCurrentIndex(account->display_power);
    ui->comboBox_displayCadence->setCurrentIndex(account->display_cadence);
    ui->comboBox_displayPowerBalance->setCurrentIndex(account->display_power_balance);
    ui->comboBox_powerAverage->setCurrentIndex(account->averaging_power);
    ui->spinBox_offsetPower->setValue(account->offset_power);

    ui->comboBox_displayVideo->setCurrentIndex(account->display_video);
    ui->label_homePage->setVisible(account->display_video);
    ui->lineEdit_homePage->setVisible(account->display_video);

    QSettings settings;
    settings.beginGroup("webBrowserWorkout");
    // One-time reset: clear any previously saved home page and force YouTube.
    // DRM services (e.g. the old Netflix default) need Widevine, which the
    // embedded QWebEngineView cannot bundle, so they never played. Guarded by a
    // flag so the user can still set their own URL afterwards.
    if (!settings.value("youtubeResetDone", false).toBool()) {
        settings.setValue("defaultUrl", "https://www.youtube.com");
        settings.setValue("youtubeResetDone", true);
    }
    QString urlSaved = settings.value("defaultUrl", "https://www.youtube.com" ).toString();
    settings.endGroup();
    ui->lineEdit_homePage->setText(urlSaved);

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
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::on_checkBox_targetPower_clicked(bool checked) {
    parentDialog->mainPlot->showHideTargetPower(checked);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::on_checkBox_targetCadence_clicked(bool checked) {
    parentDialog->mainPlot->showHideTargetCadence(checked);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::on_checkBox_targetHr_clicked(bool checked) {
    parentDialog->mainPlot->showHideTargetHr(checked);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::on_checkBox_PowerCurve_clicked(bool checked) {
    parentDialog->mainPlot->showHideCurvePower(checked);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::on_checkBox_CadenceCurve_clicked(bool checked) {
    parentDialog->mainPlot->showHideCurveCadence(checked);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::on_checkBox_HeartRateCurve_clicked(bool checked) {
    parentDialog->mainPlot->showHideCurveHeartRate(checked);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::on_checkBox_SpeedCurve_clicked(bool checked) {
    parentDialog->mainPlot->showHideCurveSpeed(checked);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::on_checkBox_seperator_clicked(bool checked)
{
    parentDialog->mainPlot->showHideSeperator(checked);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::on_checkBox_showGrid_clicked(bool checked)
{
    parentDialog->mainPlot->showHideGrid(checked);
}


/// Timers

void DialogConfig::on_checkBox_showTimerOnTop_clicked(bool checked)
{
    ui->comboBox_fontSizeTimer->setDisabled(checked);
    ui->pushButton_leftTimer->setDisabled(checked);
    ui->pushButton_rightTimer->setDisabled(checked);

    parentDialog->showTimerOnTop(checked);

}



void DialogConfig::on_checkBox_showIntervalRemainingTime_clicked(bool checked)
{

    parentDialog->showTimerIntervalRemaining(checked);
}

void DialogConfig::on_checkBox_showWorkoutRemainingTime_clicked(bool checked)
{

    parentDialog->showTimerWorkoutRemaining(checked);
}

void DialogConfig::on_checkBox_showElapsedTime_clicked(bool checked)
{

    parentDialog->showTimerWorkoutElapsed(checked);
}

void DialogConfig::onTimerFontSizeChanged(const QString &arg1)
{
    parentDialog->setTimerFontSize(arg1.toInt());
}



/// CHANGE DISPLAY WIDGET
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DialogConfig::on_checkBox_enableHR_clicked(bool checked) {

    qDebug() << "on_checkBox_enableHR_clicked" << checked;
    account->show_hr_widget = checked;
    ui->comboBox_displayHR->setEnabled(checked);

    if (checked) {
        parentDialog->showHeartRateDisplayWidget(ui->comboBox_displayHR->currentIndex());
    }
    else {
        parentDialog->showHeartRateDisplayWidget(-1);
    }

}
//---------
void DialogConfig::on_comboBox_displayHR_currentIndexChanged(int index) {

    if (ui->checkBox_enableHR->isChecked())
        parentDialog->showHeartRateDisplayWidget(index);
}
//----------------
void DialogConfig::on_comboBox_virtualSpeed_currentIndexChanged(int index)
{
    qDebug() << "OK CLICKED!" << index;


    if (index == 0) { //Virtual Speed
        account->use_virtual_speed = true;
        parentDialog->useVirtualSpeedData(true);
        ui->checkBox_showTrainerSpeed->setVisible(true);
    }
    else {
        account->use_virtual_speed = false;
        parentDialog->useVirtualSpeedData(false);     // Trainer Speed
        ui->checkBox_showTrainerSpeed->setVisible(false);
    }

}


//-----------------------------------------------------------------------------------
void DialogConfig::on_checkBox_showTrainerSpeed_clicked(bool checked)
{

    account->show_trainer_speed = checked;
    parentDialog->showTrainerSpeed(checked);
}



//-----------------------------------------------------------------------------------
void DialogConfig::on_checkBox_enablePower_clicked(bool checked) {

    account->show_power_widget = checked;
    ui->comboBox_displayPower->setEnabled(checked);
    ui->comboBox_powerAverage->setEnabled(checked);

    if (checked) {
        parentDialog->showPowerDisplayWidget(ui->comboBox_displayPower->currentIndex());
    }
    else {
        parentDialog->showPowerDisplayWidget(-1);
    }

}
//---------
void DialogConfig::on_comboBox_displayPower_currentIndexChanged(int index) {

    if (ui->checkBox_enablePower->isChecked())
        parentDialog->showPowerDisplayWidget(index);
}

//------------------------------------------------------------------------------------
void DialogConfig::on_checkBox_enablePowerBalance_clicked(bool checked) {

    account->show_power_balance_widget = checked;
    ui->comboBox_displayPowerBalance->setEnabled(checked);

    if (checked) {
        parentDialog->showPowerBalanceWidget(ui->comboBox_displayPowerBalance->currentIndex());
    }
    else {
        parentDialog->showPowerBalanceWidget(-1);
    }
}
//---------
void DialogConfig::on_comboBox_displayPowerBalance_currentIndexChanged(int index) {

    if (ui->checkBox_enablePowerBalance->isChecked())
        parentDialog->showPowerBalanceWidget(index);
}

//------------------------------------------------------------------------------------
void DialogConfig::on_checkBox_enableCadence_clicked(bool checked) {

    account->show_cadence_widget = checked;
    ui->comboBox_displayCadence->setEnabled(checked);

    if (checked) {
        parentDialog->showCadenceDisplayWidget(ui->comboBox_displayCadence->currentIndex());
    }
    else {
        parentDialog->showCadenceDisplayWidget(-1);
    }
}
//---------
void DialogConfig::on_comboBox_displayCadence_currentIndexChanged(int index) {

    if (ui->checkBox_enableCadence->isChecked())
        parentDialog->showCadenceDisplayWidget(index);
}

//------------------------------------------------------------------------------------
void DialogConfig::on_checkBox_enableSpeed_clicked(bool checked) {

    account->show_speed_widget = checked;
    parentDialog->showSpeedDisplayWidget();

}
//------------------------------------------------------------------------------------
void DialogConfig::on_checkBox_enableOxygen_clicked(bool checked)
{
    account->show_oxygen_widget = checked;
    parentDialog->showOxygenDisplayWidget();
}



//------------------------------------------------------------------------------------
void DialogConfig::on_checkBox_enableCalories_clicked(bool checked) {

    account->show_calories_widget = checked;
    parentDialog->showCaloriesDisplayWidget();

}






///////////////////////////////////////////////////////////////////////////////////////////////////////////
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





///////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::on_comboBox_displayVideo_activated(int index) {
    parentDialog->showVideoPlayer(index);

    //web browser
    ui->label_homePage->setVisible(index == 1);
    ui->lineEdit_homePage->setVisible(index == 1);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::setStudioMode() {

    qDebug() << "DConfig- setStudioMode";

    ui->page_general->setDisabled(true);

    //timer
    ui->checkBox_showTimerOnTop->setVisible(false);
    ui->comboBox_fontSizeTimer->setVisible(false);
    ui->groupBox_4->setVisible(false);
    ui->label_fontSizeTxt->setVisible(false);

    //power
    ui->checkBox_enablePower->setVisible(false);
    ui->label_5->setVisible(false);
    ui->comboBox_displayPower->setVisible(false);
    ui->label_26->setVisible(false);
    ui->spinBox_offsetPower->setVisible(false);
    ui->label_3->setVisible(false);
    ui->groupBox_7->setVisible(false);
    ui->groupBox_2->setVisible(false);

    ui->tab_6->setDisabled(true);
    ui->tab_hr->setDisabled(true);
    ui->tab_5->setDisabled(true);
    ui->tab_3->setDisabled(true);
    ui->tab_2->setDisabled(true);

    //graph
    ui->checkBox_HeartRateCurve->setDisabled(true);
    ui->checkBox_PowerCurve->setDisabled(true);
    ui->checkBox_CadenceCurve->setDisabled(true);
    ui->checkBox_SpeedCurve->setDisabled(true);

    ui->tab_12->setDisabled(true);
    ui->tab_14->setDisabled(true);


}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::accept() {

    saveSettings();
    QDialog::accept();
}
//-----------------------
void DialogConfig::reject() {

    // On cancel only persist the last-selected tab index so the dialog
    // reopens on the same page; do NOT commit widget values.
    account->last_index_selected_config_workout = ui->listWidget_settings->currentRow();
    QTabWidget *myTab = ui->stackedWidget->currentWidget()->findChild<QTabWidget*>();
    if (myTab)
        account->last_tab_sub_config_selected = myTab->currentIndex();

    QDialog::reject();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogConfig::saveSettings() {

    qDebug() << "saveSettings";


    /// Save last openned tab
    account->last_index_selected_config_workout = ui->listWidget_settings->currentRow();

    ///Save last openned subtab
    QTabWidget *myTab = ui->stackedWidget->currentWidget()->findChild<QTabWidget*>();
    if (myTab)
        account->last_tab_sub_config_selected = myTab->currentIndex();


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
    account->show_timer_on_top = ui->checkBox_showTimerOnTop->isChecked();
    account->show_interval_remaining = ui->checkBox_showIntervalRemainingTime->isChecked();
    account->show_workout_remaining =  ui->checkBox_showWorkoutRemainingTime->isChecked();
    account->show_elapsed = ui->checkBox_showElapsedTime->isChecked();
    account->font_size_timer = ui->comboBox_fontSizeTimer->currentText().toInt();



    /// Display widgets
    account->display_hr = ui->comboBox_displayHR->currentIndex();
    account->display_power = ui->comboBox_displayPower->currentIndex();
    account->display_cadence = ui->comboBox_displayCadence->currentIndex();
    account->display_power_balance = ui->comboBox_displayPowerBalance->currentIndex();
    account->averaging_power = ui->comboBox_powerAverage->currentIndex();
    account->offset_power = ui->spinBox_offsetPower->value();

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


    qDebug() << "OK SAVED SETTINGS DONE";

    // WebBrowser settings, save Url
    QSettings settings;
    settings.beginGroup("webBrowserWorkout");
    settings.setValue("defaultUrl", ui->lineEdit_homePage->text());
    settings.endGroup();

    // Persist display & sound prefs locally (the server putAccount endpoint is
    // defunct), so the user's choices — including the video player — survive a
    // restart.
    account->saveDisplayPrefs();

    // Save the new preference tabs
    saveIntervalSummary();

    parentDialog->setMessagePlot(); //update message start workout
}










//-------------------------------------------------------------------------------------
void DialogConfig::moveElement(QString widgetIdentifier, bool moveRight) {


    //    qDebug() << "Before moving";
    //    for (int i=0; i<settings->getNumberWidget(); i++) {
    //        qDebug() << settings->tabDisplay[i];
    //    }


    //get position current widget
    int positionWidget = -1;
    for (int i=0; i<account->getNumberWidget(); i++) {
        if (account->tab_display[i] == widgetIdentifier)
            positionWidget = i;
    }

    //check that element is not already on border
    if (moveRight && positionWidget == account->getNumberWidget()-1)
        return;
    if (!moveRight && positionWidget == 0)
        return;

    //get the element to permute
    int posToSwitch;
    QString widgetToSwitch;
    moveRight ? posToSwitch=positionWidget+1 : posToSwitch=positionWidget-1;
    widgetToSwitch = account->tab_display[posToSwitch];

    // permute
    account->tab_display[posToSwitch] = widgetIdentifier;
    account->tab_display[positionWidget] = widgetToSwitch;


    //    qDebug() << "**After moving";
    //    for (int i=0; i<settings->getNumberWidget(); i++) {
    //        qDebug() << settings->tabDisplay[i];
    //    }


    //redraw
    parentDialog->moveWidgetsPosition();
}






//---------------------------------------------------------
void DialogConfig::on_pushButton_leftTimer_clicked()
{
    moveElement(account->getTimerStr(), false);
}
void DialogConfig::on_pushButton_rightTimer_clicked()
{
    moveElement(account->getTimerStr(), true);
}

void DialogConfig::on_pushButton_leftHr_clicked()
{
    moveElement(account->getHrStr(), false);
}
void DialogConfig::on_pushButton_rightHr_clicked()
{
    moveElement(account->getHrStr(), true);
}

void DialogConfig::on_pushButton_leftPower_clicked()
{
    moveElement(account->getPowerStr(), false);
}
void DialogConfig::on_pushButton_rightPower_clicked()
{
    moveElement(account->getPowerStr(), true);
}

void DialogConfig::on_pushButton_leftPowerBal_clicked()
{
    moveElement(account->getPowerBalanceStr(), false);
}
void DialogConfig::on_pushButton_rightPowerBal_clicked()
{
    moveElement(account->getPowerBalanceStr(), true);
}

void DialogConfig::on_pushButton_leftCadence_clicked()
{
    moveElement(account->getCadenceStr(), false);
}
void DialogConfig::on_pushButton_rightCadence_clicked()
{
    moveElement(account->getCadenceStr(), true);
}

void DialogConfig::on_pushButton_leftSpeed_clicked()
{
    moveElement(account->getSpeedStr(), false);
}
void DialogConfig::on_pushButton_rightSpeed_clicked()
{
    moveElement(account->getSpeedStr(), true);
}

void DialogConfig::on_pushButton_leftInfoWorkout_clicked()
{
    moveElement(account->getInfoWorkoutStr(), false);
}
void DialogConfig::on_pushButton_rightInfoWorkout_clicked()
{
    moveElement(account->getInfoWorkoutStr(), true);
}

void DialogConfig::on_pushButton_leftOxygen_clicked()
{
    moveElement(account->getOxygenStr(), false);
}
void DialogConfig::on_pushButton_rightOxygen_clicked()
{
    moveElement(account->getOxygenStr(), true);
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
    durRow->addWidget(spinIntervalSummaryDuration);
    durRow->addStretch();
    grpSummaryLayout->addLayout(durRow);

    ui->verticalLayout_2->addWidget(grpSummary);
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

namespace {
/// Tiny modal that edits a single Radio. Returns the edited copy on accept.
/// Lives here (not in its own .h/.cpp pair) because it is only used by the
/// three Add/Edit/Delete handlers below.
bool runRadioEditDialog(QWidget *parent, Radio& inOut) {
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Radio Station"));

    auto *nameEdit    = new QLineEdit(inOut.getName());
    auto *genreEdit   = new QLineEdit(inOut.getGenre());
    auto *langEdit    = new QLineEdit(inOut.getLanguage());
    auto *urlEdit     = new QLineEdit(inOut.getUrl());
    urlEdit->setPlaceholderText(QStringLiteral("https://stream.example.com/stream.mp3"));
    auto *bitrateSpin = new QSpinBox();
    bitrateSpin->setRange(0, 1000);
    bitrateSpin->setSuffix(QStringLiteral(" kbps"));
    bitrateSpin->setValue(inOut.getBitrate());

    auto *form = new QFormLayout();
    form->addRow(QObject::tr("Name"),     nameEdit);
    form->addRow(QObject::tr("Genre"),    genreEdit);
    form->addRow(QObject::tr("Language"), langEdit);
    form->addRow(QObject::tr("Bitrate"),  bitrateSpin);
    form->addRow(QObject::tr("URL"),      urlEdit);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dlg);
    layout->addLayout(form);
    layout->addWidget(buttons);
    dlg.resize(420, dlg.sizeHint().height());

    if (dlg.exec() != QDialog::Accepted)
        return false;

    /// Minimal validation: name and url are required. We don't try to validate
    /// the URL itself — libvlc will reject anything unparseable when the user
    /// hits Play.
    if (nameEdit->text().trimmed().isEmpty() || urlEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(parent, QObject::tr("Radio Station"),
            QObject::tr("Name and URL are required."));
        return false;
    }

    inOut = Radio(nameEdit->text().trimmed(),
                  genreEdit->text().trimmed(),
                  /*gotAds=*/false,
                  bitrateSpin->value(),
                  langEdit->text().trimmed(),
                  urlEdit->text().trimmed());
    return true;
}
}


//----------------------------------------------------------------------------
void DialogConfig::on_pushButton_addRadio_clicked() {

    Radio newRadio(QString(), QString(), false, 128, QString(), QString());
    if (!runRadioEditDialog(this, newRadio))
        return;

    tableModel->addRadio(newRadio);
    Util::saveLocalRadioList(tableModel->getAllRadios());
}


//----------------------------------------------------------------------------
void DialogConfig::on_pushButton_editRadio_clicked() {

    QModelIndex idx = ui->tableView_radio->selectionModel()->currentIndex();
    if (!idx.isValid())
        idx = currentRadioIndex;
    if (!idx.isValid()) {
        QMessageBox::information(this, tr("Edit Radio"),
            tr("Select a radio station to edit."));
        return;
    }

    const int row = idx.row();
    Radio existing = tableModel->getRadioAtRow(idx);
    if (!runRadioEditDialog(this, existing))
        return;

    tableModel->replaceRadioAtRow(row, existing);
    Util::saveLocalRadioList(tableModel->getAllRadios());
}


//----------------------------------------------------------------------------
void DialogConfig::on_pushButton_deleteRadio_clicked() {

    QModelIndex idx = ui->tableView_radio->selectionModel()->currentIndex();
    if (!idx.isValid())
        idx = currentRadioIndex;
    if (!idx.isValid()) {
        QMessageBox::information(this, tr("Delete Radio"),
            tr("Select a radio station to delete."));
        return;
    }

    const int row = idx.row();
    const Radio radio = tableModel->getRadioAtRow(idx);

    if (QMessageBox::question(this, tr("Delete Radio"),
            tr("Delete \"%1\"?").arg(radio.getName()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    tableModel->removeRadioAtRow(row);
    if (currentRadioIndex.row() == row)
        currentRadioIndex = QModelIndex();
    Util::saveLocalRadioList(tableModel->getAllRadios());
}
