#include "workoutdialog.h"
#include "ui_workoutdialog.h"

#ifdef Q_OS_WIN32
#   include <windows.h>
#endif

#include <stdexcept>

#include <QLibrary>
#include <QMessageBox>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QGuiApplication>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QFileInfo>
#include <QDateTime>

#include "interval.h"
#include "workout.h"
#include "util.h"
#include "qwt_symbol.h"
#include "datacadence.h"
#include "datapower.h"
#include "dataheartrate.h"
#include "dataspeed.h"
#include "userdao.h"
#include "sensordao.h"
#include "faderlabel.h"
#include "clock.h"
#include "workoututil.h"
#include "dialogconfig.h"
#include "dialogcalibrate.h"
#include "dialogcalibratepm.h"
#include "dialogkeyboardshortcuts.h"
#include "logger.h"
#include "strava_service.h"
#include "trainingpeaks_service.h"
#include "selfloops_service.h"
#include "intervalsicuservice.h"
#include "extrequest.h"
#include "intervalsummaryutil.h"






WorkoutDialog::~WorkoutDialog() {

    qDebug() << "WorkoutDialog destructor!----------";

    emit stopDecodingMsgHub();


    delete ui;

#ifdef Q_OS_MAC
    macUtil.releaseScreensaverLock();
#endif
#ifdef Q_OS_WIN32
    SetThreadExecutionState(ES_CONTINUOUS);
#endif

    // stop and delete Clock thread
    emit finishClock();


    // Wait for clock thread to stop (5 s guard prevents hang if thread ignores quit())
    thread->quit();
    if (!thread->wait(5000)) {
        qCritical() << "[WorkoutDialog] Clock thread did not finish in 5 s — terminating";
        thread->terminate();
        thread->wait(1000);
    }

    DataCadence::instance().clearData();
    DataPower::instance().clearData();
    DataHeartRate::instance().clearData();
    DataSpeed::instance().clearData();

    qDebug() << "----Destructor over WorkoutDialog";
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WorkoutDialog::WorkoutDialog(Workout workout,  QList<Radio> lstRadio, QVector<UserStudio> vecUserStudio,
                             QWidget *parent) : QDialog(parent), ui(new Ui::WorkoutDialog) {

    ui->setupUi(this);
    this->setFocusPolicy(Qt::ClickFocus);
    // Disable ScreenSaver
#ifdef Q_OS_MAC
    macUtil.disableScreensaver();
#endif
#ifdef Q_OS_WIN32
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);
#endif


    this->account = qApp->property("Account").value<Account*>();
    this->settings = qApp->property("User_Settings").value<Settings*>();
    this->soundPlayer =  qApp->property("SoundPlayer").value<SoundPlayer*>();
    this->achievementManager = qApp->property("ManagerAchievement").value<ManagerAchievement*>();
    this->workout = workout;
    this->vecUserStudio = vecUserStudio;
    soundPlayer->setVolume(account->sound_player_vol);
    msgPairingDone = tr("Done!");


    //Init
    usingHr = false;
    usingSpeedCadence = false;
    usingCadence = false;
    usingSpeed = false;
    usingPower = false;
    usingFEC = false;
    usingOxygen = false;

    hrPairingDone = false;
    scPairingDone = false;
    cadencePairingDone = false;
    speedPairingDone = false;
    powerPairingDone = false;
    fecPairingDone = false;
    oxygenPairingDone = false;

    idFecMainUser = -1;

    isUsingSlopeMode = false;
    timerAlertCalibrateCt = new QTimer(this);

    //data points
    nbPointHr1sec = QVector<int>(constants::nbMaxUserStudio, 0);
    averageHr1sec = QVector<double>(constants::nbMaxUserStudio, -1);

    nbPointCadence1sec = QVector<int>(constants::nbMaxUserStudio, 0);
    averageCadence1sec = QVector<double>(constants::nbMaxUserStudio, -1);

    nbPointSpeed1sec = QVector<int>(constants::nbMaxUserStudio, 0);
    averageSpeed1sec = QVector<double>(constants::nbMaxUserStudio, -1);

    nbPointPower1sec = QVector<int>(constants::nbMaxUserStudio, 0);
    averagePower1sec = QVector<double>(constants::nbMaxUserStudio, -1);

    avgRightPedal1sec = QVector<double>(constants::nbMaxUserStudio, -1);
    avgLeftTorqueEff = QVector<double>(constants::nbMaxUserStudio, -1);
    avgRightTorqueEff = QVector<double>(constants::nbMaxUserStudio, -1);
    avgLeftPedalSmooth = QVector<double>(constants::nbMaxUserStudio, -1);
    avgRightPedalSmooth = QVector<double>(constants::nbMaxUserStudio, -1);
    avgCombinedPedalSmooth = QVector<double>(constants::nbMaxUserStudio, -1);
    avgSaturatedHemoglobinPercent1sec = QVector<double>(constants::nbMaxUserStudio, -1);
    avgTotalHemoglobinConc1sec = QVector<double>(constants::nbMaxUserStudio, -1);



    //// ------------- Clock thread -------------------------
    thread = new QThread(this);
    Clock *clock1 = new Clock("clock1");
    clock1->moveToThread(thread);

    ///This --> Clock
    connect(this, SIGNAL(startClock()), clock1, SLOT(startClock()) );
    connect(this, SIGNAL(pauseClock()), clock1, SLOT(pauseClock()) );
    connect(this, SIGNAL(resumeClock()), clock1, SLOT(resumeClock()) );
    connect(this, SIGNAL(finishClock()), clock1, SLOT(finishClock()) );

    connect(this, SIGNAL(sendUserInfo(int, double,double,int)), clock1, SLOT(receiveUserInfo(int, double,double,int)) );
    connect(this, SIGNAL(sendPowerData(int, int)), clock1, SLOT(receivePowerData(int, int)) );
    //    connect(this, SIGNAL(sendSlopeData(int,double)), clock1, SLOT(receiveSlopeData(int, double)) );
    connect(this, SIGNAL(startClockSpeed()), clock1, SLOT(startClockSpeed()) );

    ///Clock --> This
    connect(clock1, SIGNAL(oneCyclePassed(double)), this, SLOT(updateRealTimeGraph(double)) );
    connect(clock1, SIGNAL(oneSecPassed(double)), this, SLOT(update1sec(double)) );
    connect(clock1, SIGNAL(updateTimePaused(double)), this, SLOT(updatePausedTime(double)) );
    connect(clock1, SIGNAL(virtualSpeed(int,double,double)), this, SLOT(VirtualSpeedDataReceived(int,double,double)) );

    connect(clock1, SIGNAL(finished()), thread, SLOT(quit()));
    connect(clock1, SIGNAL(finished()), clock1, SLOT(deleteLater()));
    connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

    thread->start();
    ///----------------------------------------------------
    timeElapsed_sec = 0;
    lastIntervalEndTime_msec = 0;

    lastIntervalTotalTimePausedWorkout_msec = 0;
    totalTimePausedWorkout_msec = 0;
    lastIntervalEndTime_sec = 0;


    sendUserInfoToClock();
    emit startClockSpeed();

    // Calibration
    connect(ui->widget_topMenu, SIGNAL(startCalibrateFEC()), this, SLOT(startCalibrateFEC()) );
    connect(ui->widget_topMenu, SIGNAL(startCalibrationPM()), this, SLOT(startCalibrationPM()) );


    // Ignore click on workout plot while widget is loading
    timerIgnoreClickPlot = new QTimer(this);
    bignoreClickPlot = true;
    timerIgnoreClickPlot->start(1000);
    connect(timerIgnoreClickPlot, SIGNAL(timeout()), this, SLOT(ignoreClickPlot()) );

    // Sensor dropout watchdog — fires every second while WorkoutDialog is open.
    m_dropoutWatchdog = new QTimer(this);
    m_dropoutWatchdog->setInterval(1000);
    connect(m_dropoutWatchdog, &QTimer::timeout, this, &WorkoutDialog::checkSensorDropout);
    m_dropoutWatchdog->start();


    /// Sounds timer
    timerCheckToActivateSound = new QTimer(this);
    timerCheckToActivateSound->setInterval(4000);
    connect(timerCheckToActivateSound, SIGNAL(timeout()), this, SLOT(activateSoundBool()) );
    soundsActive = false;

    durationReactivateSameSoundMsec = 10000; //to edit if 10sec is too long
    timerCheckReactivateSoundPowerTooLow =  new QTimer(this);
    timerCheckReactivateSoundPowerTooHigh =  new QTimer(this);
    timerCheckReactivateSoundCadenceTooLow =  new QTimer(this);
    timerCheckReactivateSoundCadenceTooHigh =  new QTimer(this);
    timerCheckReactivateSoundPowerTooLow->setInterval(durationReactivateSameSoundMsec);
    timerCheckReactivateSoundPowerTooHigh->setInterval(durationReactivateSameSoundMsec);
    timerCheckReactivateSoundCadenceTooLow->setInterval(durationReactivateSameSoundMsec);
    timerCheckReactivateSoundCadenceTooHigh->setInterval(durationReactivateSameSoundMsec);
    connect(timerCheckReactivateSoundPowerTooLow, SIGNAL(timeout()), this, SLOT(activateSoundPowerTooLow()) );
    connect(timerCheckReactivateSoundPowerTooHigh, SIGNAL(timeout()), this, SLOT(activateSoundPowerTooHigh()) );
    connect(timerCheckReactivateSoundCadenceTooLow, SIGNAL(timeout()), this, SLOT(activateSoundCadenceTooLow()) );
    connect(timerCheckReactivateSoundCadenceTooHigh, SIGNAL(timeout()), this, SLOT(activateSoundCadenceTooHigh()) );
    soundPowerTooLowActive = true;
    soundPowerTooHighActive = true;
    soundCadenceTooLowActive = true;
    soundCadenceTooHighActive = true;



    ///-------------------------- Widget Sensor Loading  ----------------------
    widgetLoading = new QWidget(ui->widget_allSpeedo);
    widgetLoading->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    widgetLoading->setFocusPolicy(Qt::NoFocus);
    QVBoxLayout *vLayout = new QVBoxLayout(widgetLoading);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    QSpacerItem *spacer = new QSpacerItem(200, 200, QSizePolicy::Expanding, QSizePolicy::Expanding);

    QFont fontLabel;
    fontLabel.setPointSize(10);
    labelPairHr = new FaderLabel(widgetLoading);
    labelPairHr->setFont(fontLabel);
    labelPairHr->setMinimumHeight(20);
    labelPairHr->setMaximumHeight(20);
    labelPairHr->setMaximumWidth(600);
    labelPairHr->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelPairHr->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelPairHr->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelPairHr->setStyleSheet("background-color : rgba(1,1,1,220); color : white;");

    labelSpeedCadence = new FaderLabel(widgetLoading);
    labelSpeedCadence->setFont(fontLabel);
    labelSpeedCadence->setMinimumHeight(20);
    labelSpeedCadence->setMaximumHeight(20);
    labelSpeedCadence->setMaximumWidth(600);
    labelSpeedCadence->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelSpeedCadence->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelSpeedCadence->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelSpeedCadence->setStyleSheet("background-color : rgba(1,1,1,220); color : white;");

    labelCadence = new FaderLabel(widgetLoading);
    labelCadence->setFont(fontLabel);
    labelCadence->setMinimumHeight(20);
    labelCadence->setMaximumHeight(20);
    labelCadence->setMaximumWidth(600);
    labelCadence->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelCadence->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelCadence->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelCadence->setStyleSheet("background-color : rgba(1,1,1,220); color : white;");

    labelSpeed = new FaderLabel(widgetLoading);
    labelSpeed->setFont(fontLabel);
    labelSpeed->setMinimumHeight(20);
    labelSpeed->setMaximumHeight(20);
    labelSpeed->setMaximumWidth(600);
    labelSpeed->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelSpeed->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelSpeed->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelSpeed->setStyleSheet("background-color : rgba(1,1,1,220); color : white;");

    labelFEC = new FaderLabel(widgetLoading);
    labelFEC->setFont(fontLabel);
    labelFEC->setMinimumHeight(20);
    labelFEC->setMaximumHeight(20);
    labelFEC->setMaximumWidth(600);
    labelFEC->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelFEC->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelFEC->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelFEC->setStyleSheet("background-color : rgba(1,1,1,220); color : white;");

    labelOxygen = new FaderLabel(widgetLoading);
    labelOxygen->setFont(fontLabel);
    labelOxygen->setMinimumHeight(20);
    labelOxygen->setMaximumHeight(20);
    labelOxygen->setMaximumWidth(600);
    labelOxygen->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelOxygen->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelOxygen->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelOxygen->setStyleSheet("background-color : rgba(1,1,1,220); color : white;");

    labelPower = new FaderLabel(widgetLoading);
    labelPower->setFont(fontLabel);
    labelPower->setMinimumHeight(20);
    labelPower->setMaximumHeight(20);
    labelPower->setMaximumWidth(600);
    labelPower->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelPower->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelPower->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelPower->setStyleSheet("background-color : rgba(1,1,1,220); color : white;");

    labelCtPower = new FaderLabel(widgetLoading);
    labelCtPower->setFont(fontLabel);
    labelCtPower->setMinimumHeight(20);
    labelCtPower->setMaximumHeight(20);
    labelCtPower->setMaximumWidth(600);
    labelCtPower->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelCtPower->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelCtPower->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelCtPower->setStyleSheet("background-color : rgba(1,1,1,220); color : green;");

    labelPairHr->setVisible(false);
    labelSpeedCadence->setVisible(false);
    labelCadence->setVisible(false);
    labelSpeed->setVisible(false);
    labelFEC->setVisible(false);
    labelOxygen->setVisible(false);
    labelPower->setVisible(false);
    labelCtPower->setVisible(false);

    vLayout->addSpacerItem(spacer);
    vLayout->addWidget(labelPairHr, Qt::AlignBottom);
    vLayout->addWidget(labelSpeedCadence, Qt::AlignBottom);
    vLayout->addWidget(labelCadence, Qt::AlignBottom);
    vLayout->addWidget(labelSpeed, Qt::AlignBottom);
    vLayout->addWidget(labelFEC, Qt::AlignBottom);
    vLayout->addWidget(labelOxygen, Qt::AlignBottom);
    vLayout->addWidget(labelPower, Qt::AlignBottom);
    vLayout->addWidget(labelCtPower, Qt::AlignBottom);


    QGridLayout *glayout = static_cast<QGridLayout*>( ui->widget_allSpeedo->layout()  );
    glayout->addWidget(widgetLoading, 0, 0, 0, 0);


    widgetLoading->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    widgetLoading->setWindowFlags(Qt::WindowStaysOnTopHint);
    ///----------------------------- End Calibration widgets ------------------------


    //-- Check Session_ID is in DB and session is not expired
    numberFailCheckSessionExpired = 0;
    if (!account->isOffline) {
        labelPairHr->setText(tr(" Checking Session..."));
        labelPairHr->setVisible(true);
        labelPairHr->fadeIn(500);

        replyPutAccountToCheckSessionExpired = UserDAO::putAccount(account);
        connect(replyPutAccountToCheckSessionExpired, SIGNAL(finished()), this, SLOT(slotPutAccountFinished()) );
    }


    ///-------------------------- Battery widgets ----------------------
    widgetBattery = new QWidget(ui->widget_allSpeedo);
    widgetBattery->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    widgetBattery->setFocusPolicy(Qt::NoFocus);
    QHBoxLayout *hLayout = new QHBoxLayout(widgetBattery);
    QVBoxLayout *vLayoutSub = new QVBoxLayout(widgetBattery);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);
    vLayoutSub->setContentsMargins(0, 0, 0, 0);
    vLayoutSub->setSpacing(0);

    QSpacerItem *spacer2 = new QSpacerItem(200, 200, QSizePolicy::Expanding, QSizePolicy::Expanding);
    QSpacerItem *spacer3 = new QSpacerItem(200, 200, QSizePolicy::Expanding, QSizePolicy::Expanding);

    labelBattery = new FaderLabel(widgetBattery);
    labelBattery->setFont(fontLabel);
    labelBattery->setMinimumHeight(20);
    labelBattery->setMaximumHeight(20);
    labelBattery->setMinimumWidth(250);
    labelBattery->setMaximumWidth(600);
    labelBattery->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelBattery->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelBattery->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelBattery->setStyleSheet("padding-left: 5px; background-color : rgba(1,1,1,220); color : red;");
    labelBattery->setText("labelBattery");

    labelBatteryStatus = new FaderLabel(widgetBattery);
    labelBatteryStatus->setFont(fontLabel);
    labelBatteryStatus->setMinimumHeight(20);
    labelBatteryStatus->setMaximumHeight(20);
    labelBatteryStatus->setMaximumWidth(600);
    labelBatteryStatus->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelBatteryStatus->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelBatteryStatus->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelBatteryStatus->setStyleSheet("padding-left: 5px; background-color : rgba(1,1,1,220); color : red;");
    labelBatteryStatus->setText("");

    vLayoutSub->addSpacerItem(spacer2);
    vLayoutSub->addWidget(labelBattery, Qt::AlignBottom);
    vLayoutSub->addWidget(labelBatteryStatus, Qt::AlignBottom);

    hLayout->addSpacerItem(spacer3);
    hLayout->addLayout(vLayoutSub);

    labelBattery->setVisible(false);
    labelBatteryStatus->setVisible(false);

    QGridLayout *glayout3 = static_cast<QGridLayout*>( ui->widget_allSpeedo->layout()  );
    glayout3->addWidget(widgetBattery, 0, 0, 0, 0, Qt::AlignRight);
    widgetBattery->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    ///----------------------------- End Battery widgets ------------------------



    ////  ----------------------  Achievement Window --------------------------
    widgetAchievement = new FaderFrame(ui->widget_allSpeedo);
    widgetAchievement->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    widgetAchievement->setFocusPolicy(Qt::NoFocus);
    widgetAchievement->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    widgetAchievement->setFixedSize(220, 110);
    widgetAchievement->setObjectName("frameAchievement");
    widgetAchievement->setStyleSheet("QWidget#frameAchievement { background-color : rgba(1,1,1,250); "
                                     "border: 2px solid gray; } "
                                     "QLabel { color: white; }");
    QGridLayout *gridAchievement = new QGridLayout(widgetAchievement);
    labelIcon = new QLabel(widgetAchievement);
    labelIcon->setFixedSize(48,48);
    labelIcon->setObjectName("labelIcon");

    QLabel *labelAchievementReceived = new QLabel(widgetAchievement);
    labelAchievementReceived->setMinimumHeight(20);
    labelAchievementReceived->setMaximumHeight(20);
    labelAchievementReceived->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelAchievementReceived->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelAchievementReceived->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelAchievementReceived->setText(tr("New Achievement!"));

    labelAchievementName = new QLabel(widgetAchievement);
    labelAchievementName->setMinimumHeight(20);
    labelAchievementName->setMaximumHeight(20);
    labelAchievementName->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    labelAchievementName->setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    labelAchievementName->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    labelAchievementName->setText(tr("Name here!"));

    gridAchievement->addWidget(labelIcon, 0, 0, 2, 1);
    gridAchievement->addWidget(labelAchievementReceived, 0, 1);
    gridAchievement->addWidget(labelAchievementName, 1, 1);

    QGridLayout *glayout2 = static_cast<QGridLayout*>( ui->widget_allSpeedo->layout()  );
    glayout2->addWidget(widgetAchievement, 0, 0, 0, 0, Qt::AlignRight | Qt::AlignBottom);
    widgetAchievement->setAttribute(Qt::WA_TransparentForMouseEvents,true);

    timerLastAnimationAchievementComplete = new QTimer(this);
    connect(timerLastAnimationAchievementComplete, SIGNAL(timeout()), this, SLOT(lastAchievementAnimationDone()) );
    achievementCurrentlyPlaying = false;
    widgetAchievement->setVisible(false);
    ////////////////////////////////////-----------------------------------------


    isTransparent = false;
    Qt::WindowFlags flags;
    if (account->force_workout_window_on_top)
        flags = flags | Qt::WindowStaysOnTopHint;

#ifdef Q_OS_MAC
    flags = flags | Qt::Window;
#else
    flags = flags | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint;
#endif
    this->setWindowFlags(flags);
    installEventFilter(this); //For Hotkeys
    loadInterface();


    connect(ui->widget_topMenu, SIGNAL(config()), this, SLOT(showConfig()));
    connect(ui->widget_topMenu, SIGNAL(minimize()), this, SLOT(minimizeWindow()));
    connect(ui->widget_topMenu, SIGNAL(expand()), this, SLOT(expandWindow()));
    connect(ui->widget_topMenu, SIGNAL(exit()), this, SLOT(closeWindow()));
    connect(ui->widget_topMenu, SIGNAL(startOrPause()), this, SLOT(start_or_pause_workout()));
    connect(ui->widget_topMenu, SIGNAL(lap()), this, SLOT(lapButtonPressed()) );
    connect(this, SIGNAL(insideConfig(bool)), ui->widget_topMenu, SLOT(changeConfigIcon(bool)));


    connect(this, SIGNAL(increaseDifficulty()), ui->widget_workoutPlot, SLOT(increaseDifficulty()) );
    connect(this, SIGNAL(decreaseDifficulty()), ui->widget_workoutPlot, SLOT(decreaseDifficulty()) );
    connect(ui->widget_workoutPlot, SIGNAL(workoutDifficultyChanged(int)), this, SLOT(adjustWorkoutDifficulty(int)) );
    connect(ui->widget_workoutPlot, SIGNAL(intervalClicked(int,double,double,bool)), this, SLOT(moveToInterval(int,double,double,bool)) );


    // Achievement
    connect(achievementManager, SIGNAL(achievementCompleted(Achievement)), this, SLOT(achievementReceived(Achievement)) );
    // Video
    connect(this, SIGNAL(playPlayer()), ui->widgetVideo, SLOT(resume()) );
    connect(this, SIGNAL(pausePlayer()), ui->widgetVideo, SLOT(pause()) );



    // Initialise DataWorkout
    initDataWorkout();
    //createUserStudio Widget
    createUserStudioWidget();
    // Connect DataWorkout
    connectDataWorkout();



    ui->widgetVideo->setVisible(false);
    ui->wid_1_infoBoxHr->setTypeInfoBox(InfoWidget::HEART_RATE);
    ui->wid_2_infoBoxPower->setTypeInfoBox(InfoWidget::POWER);
    ui->wid_3_infoBoxCadence->setTypeInfoBox(InfoWidget::CADENCE);
    ui->wid_4_infoBoxSpeed->setTypeInfoBox(InfoWidget::SPEED);
    if (!account->distance_in_km) {
        ui->wid_4_infoBoxSpeed->setUseMiles(true);
        ui->wid_5_infoWorkout->setDistanceInMile(true);
    }

    //Minimalist
    ui->wid_1_minimalistHr->setTypeWidget(MinimalistWidget::HEART_RATE);
    ui->wid_2_minimalistPower->setTypeWidget(MinimalistWidget::POWER);
    ui->wid_3_minimalistCadence->setTypeWidget(MinimalistWidget::CADENCE);

    //Set User Data in all widgets
    ui->widget_workoutPlot->setUserData(account->FTP, account->LTHR);
    ui->wid_1_workoutPlot_HeartrateZoom->setUserData(account->FTP, account->LTHR);
    ui->wid_2_workoutPlot_PowerZoom->setUserData(account->FTP, account->LTHR);
    ui->wid_3_workoutPlot_CadenceZoom->setUserData(account->FTP, account->LTHR);
    ui->wid_1_minimalistHr->setUserData(account->FTP, account->LTHR);
    ui->wid_2_minimalistPower->setUserData(account->FTP, account->LTHR);
    ui->wid_3_minimalistCadence->setUserData(account->FTP, account->LTHR);
    ui->wid_1_infoBoxHr->setUserData(account->FTP, account->LTHR);
    ui->wid_2_infoBoxPower->setUserData(account->FTP, account->LTHR);
    ui->wid_3_infoBoxCadence->setUserData(account->FTP, account->LTHR);
    ui->wid_4_infoBoxSpeed->setUserData(account->FTP, account->LTHR);
    // Set Workout Data to widgets
    ui->widget_workoutPlot->setWorkoutData(workout, true);
    ui->wid_1_workoutPlot_HeartrateZoom->setWorkoutData(workout, WorkoutPlotZoomer::HEART_RATE, true);
    ui->wid_2_workoutPlot_PowerZoom->setWorkoutData(workout, WorkoutPlotZoomer::POWER, true);
    ui->wid_3_workoutPlot_CadenceZoom->setWorkoutData(workout, WorkoutPlotZoomer::CADENCE, true);


    mainPlot = ui->widget_workoutPlot;

    qDebug() << "GOT HERE WORKOTUDI1";


    currentWorkoutDifficultyPercentage = 0;

    isAskingUserQuestion = false;
    isCalibrating = false;
    isWorkoutStarted = false;
    isWorkoutPaused = true;
    isWorkoutOver = false;
    changeIntervalDisplayNextSecond = false;
    ignoreCondition = false;
    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {
        currentInterval = -1;
        isUsingSlopeMode = true;
    }
    else {
        currentInterval = 0;
        currentIntervalObj = workout.getInterval(currentInterval);
    }



    //    lastSecondPower = 0;
    //    nbPointsPower = 0;
    //New
    for (int i=0; i<constants::nbMaxUserStudio; i++) {
        arrLastSecondPower[i] = 0;
        arrNbPointPower[i] = 0;
    }

    timeElapsedTotal = QTime(0,0,0,0);
    nbUpdate1Sec = 0;

    idFecMainUser = -1;
    currentTargetPower = -1;
    currentTargetPowerRange = -1;
    currentTargetCadence = -1;
    currentTargetCadenceRange = -1;
    currMAPInterval = 1;
    totalSecOffTargetInInterval = 0;
    totalConsecutiveOffTarget = 0;



    //Dialog config — shown non-modally so the workout session continues uninterrupted (#137)
    dconfig = new DialogConfig(lstRadio, this, this);
    dconfig->setModal(false);
    connect(dconfig, &QDialog::finished, this, [this](int) { emit insideConfig(false); });

    //Internet Radio Player (QtMultimedia: QMediaPlayer streaming a network URL)
#ifdef GC_HAVE_QTMULTIMEDIA
    radioPlayer = new MyVlcPlayer(this);
    radioPlayer->setVisible(false);
    radioPlayer->setRadio(true);

    connect(dconfig, SIGNAL(signal_connectToRadioUrl(QString)), radioPlayer, SLOT(openUrlRadio(QString)) );
    connect(dconfig, SIGNAL(signal_volumeRadioChanged(int)), radioPlayer, SLOT(changeVolume(int)) );
    connect(dconfig, SIGNAL(signal_stopPlayingRadio()), radioPlayer, SLOT(stop()) );

    connect(radioPlayer, SIGNAL(playing()), dconfig, SLOT(radioStartedPlaying()) );
    connect(radioPlayer, SIGNAL(paused()), dconfig, SLOT(radioStoppedPlaying()) );
    connect(radioPlayer, SIGNAL(stopped()), dconfig, SLOT(radioStoppedPlaying()) );
    connect(radioPlayer, SIGNAL(playing()), ui->widget_topMenu, SLOT(radioStartedPlaying()) );
    connect(radioPlayer, SIGNAL(paused()), ui->widget_topMenu, SLOT(radioStoppedPlaying()) );
    connect(radioPlayer, SIGNAL(stopped()), ui->widget_topMenu, SLOT(radioStoppedPlaying()) );
#endif

    connect(dconfig, SIGNAL(radioStatus(QString)), ui->widget_topMenu, SLOT(updateRadioStatus(QString)) );

    //radio
    connect(this, SIGNAL(F6previous()), dconfig, SLOT(on_pushButton_prevRadio_clicked()) );
    connect(this, SIGNAL(F7playPause()), dconfig, SLOT(playPauseRadio()) );
    connect(this, SIGNAL(F8next()), dconfig, SLOT(on_pushButton_nextRadio_clicked()) );
    connect(ui->widget_topMenu, SIGNAL(prevRadio()), dconfig, SLOT(on_pushButton_prevRadio_clicked()) );
    connect(ui->widget_topMenu, SIGNAL(playPauseRadio()), dconfig, SLOT(playPauseRadio()) );
    connect(ui->widget_topMenu, SIGNAL(nextRadio()), dconfig, SLOT(on_pushButton_nextRadio_clicked()) );




    //Disable button in Test mode
    if (workout.getWorkoutNameEnum() == Workout::FTP_TEST || workout.getWorkoutNameEnum() == Workout::FTP8min_TEST ||
            workout.getWorkoutNameEnum() == Workout::CP5min_TEST || workout.getWorkoutNameEnum() == Workout::CP20min_TEST ||
            workout.getWorkoutNameEnum() == Workout::MAP_TEST ) {
        isTestWorkout = true;
        ui->widget_topMenu->setButtonLapVisible(false);
    }
    else {
        isTestWorkout = false;
    }


    initUI();
    setWidgetsStopped(true);

    this->setFocus();

    // Remove loading Cursor
    QApplication::restoreOverrideCursor();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::toggleTransparent() {

    if (isTransparent) {
        qDebug() << "SHOW NORMAL!";
        //revert back to normal QDialog, use saved stylesheet
        this->setStyleSheet(bakStylesheet);
        //        Qt::WindowFlags flags = Qt::Window;
        //        setWindowFlags(flags);
    }
    else {
        qDebug() << "SHOW TRANSPARENT!!";

        //save current stylesheet
        bakStylesheet = this->styleSheet();

        setStyleSheet("background:transparent;");
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowFlags(Qt::FramelessWindowHint); //this close the QDialog if not called in Constructor, why?.

        //        setWindowOpacity(0.5);
        //        ui->widget_time->setStyleSheet("background:transparent;");
        //        ui->widget_time->setAttribute(Qt::WA_TranslucentBackground);
        //        ui->widget_allSpeedo->setWindowOpacity(0.5);
    }
    isTransparent = !isTransparent;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::addToControlList(int antID, int fromHubNumber) {

    qDebug() << "Asking Workout Dialog for control" << "antID" << antID << "fromHubNumber" << fromHubNumber;

    if (!hashControlList.contains(antID)) {
        qDebug() << "Ok antID" << antID << "not in my List, you can control it!" << fromHubNumber;
        hashControlList.insert(antID, fromHubNumber);
        //emit signal back to Hub to let him add this ID
        emit permissionGrantedControl(antID, fromHubNumber);
    }
    /// DO NOT USE, because sending duplicate cause transfer to fail
    //    else if (hashControlList.size() >= nbTotalFecTrainer) {
    //        qDebug() << "OK we already control all trainer, we can control more for better reception!";
    //        hashControlList.insert(antID, fromHubNumber);
    //        emit permissionGrantedControl(antID, fromHubNumber);
    //    }
    else {
        qDebug() << "Sorry this trainer is already being controlled" <<  antID;
    }

}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::startCalibrateFEC() {

    if (isCalibrating || account->enable_studio_mode) {
        return;
    }

    return; // No hub available (BTLE-only build)
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::startCalibrationPM() {

    if (isCalibrating || account->enable_studio_mode) {
        return;
    }

    return; // No hub available (BTLE-only build)
}




//------------------------------------------------------------------------------------------------------------
void WorkoutDialog::batteryStatusReceived(QString sensorType, int percentage) {

    qDebug() << "batteryStatusReceived" << sensorType << "level:" << percentage << "%";

    // Check against configurable threshold
    if (percentage > account->battery_warning_threshold)
        return;

    // Suppress re-warning unless level dropped ≥ 5% below the last warned level
    if (m_warnedBatteryLevels.contains(sensorType)) {
        int lastWarned = m_warnedBatteryLevels.value(sensorType);
        if (percentage > lastWarned - 5)
            return;
    }

    m_warnedBatteryLevels[sensorType] = percentage;

    labelBatteryStatus->setVisible(true);
    labelBatteryStatus->setText(
        tr("%1 sensor battery: %2%").arg(sensorType).arg(percentage));
    labelBatteryStatus->fadeInAndFadeOutAfterPause(400, 1000, 15000);
}



//------------------------------------------------------------------------------------------------------------
void WorkoutDialog::slotFinishedGetPixmap() {

    qDebug() << "slow pixmap finished, show image!";

    QByteArray m_DownloadedData = replyGetBixmap->readAll();
    QPixmap pixmap; //48,48 icon

    pixmap.loadFromData(m_DownloadedData);
    labelIcon->setPixmap(pixmap);

    replyGetBixmap->deleteLater();
}


//------------------------------------------------------------------------------------------------------------
void WorkoutDialog::animateAchievement() {


    achievementCurrentlyPlaying = true;
    if (queueAchievement.size() == 0)
        return;

    /// Play sound
    if (account->enable_sound && account->sound_achievement)
        soundPlayer->playSoundAchievement();

    qDebug() << "QUEUE SIZE IS" << queueAchievement.size();

    ///Take achievement from top of queue
    Achievement achievement = queueAchievement.head();
    /// change name and icon (64x64)
    labelAchievementName->setText(achievement.getName());

    //Load image !
    QNetworkAccessManager *managerWS = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    QNetworkRequest request;
    request.setUrl(QUrl(achievement.getIconUrl()));
    request.setRawHeader("User-Agent", "MyOwnBrowser 1.0");
    replyGetBixmap = managerWS->get(request);
    connect(replyGetBixmap, SIGNAL(finished()), this, SLOT(slotFinishedGetPixmap()) );
    //-----------------
    widgetAchievement->setVisible(true);
    widgetAchievement->fadeInAndFadeOutAfterPause(600, 2000, 6500);
    timerLastAnimationAchievementComplete->start(9000); ///Check other achievement to play in the queue
}

//------------------------------------------------------------------------------------------------------------
void WorkoutDialog::lastAchievementAnimationDone() {

    timerLastAnimationAchievementComplete->stop();
    achievementCurrentlyPlaying = false;

    ///Remove queue item, check if queue is empty, if empty, put flag to not playing;
    queueAchievement.dequeue();
    if (queueAchievement.isEmpty()) {
        qDebug() << "NO MORE ACHIEVEMENT";
    }
    else {
        animateAchievement();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::initUI() {

    if (!account->enable_studio_mode) {
        moveWidgetsPosition();
    }

    ///Timers
    showTimerOnTop(account->show_timer_on_top);
    showTimerIntervalRemaining(account->show_interval_remaining);
    showTimerWorkoutRemaining(account->show_workout_remaining);
    showTimerWorkoutElapsed(account->show_elapsed);
    setTimerFontSize(account->font_size_timer);

    /// Widgets
    showHeartRateDisplayWidget(account->display_hr);
    showPowerDisplayWidget(account->display_power);
    showCadenceDisplayWidget(account->display_cadence);
    showSpeedDisplayWidget();
    showTrainerSpeed(account->show_trainer_speed);
    useVirtualSpeedData(account->use_virtual_speed);
    showOxygenDisplayWidget();
    showCaloriesDisplayWidget();
    showPowerBalanceWidget(account->display_power_balance);

    /// Main plot Target and curve
    mainPlot->showHideSeperator(account->show_seperator_interval);
    mainPlot->showHideGrid(account->show_grid);
    mainPlot->showHideTargetCadence(account->show_cadence_target);
    mainPlot->showHideTargetHr(account->show_hr_target);
    mainPlot->showHideTargetPower(account->show_power_target);
    mainPlot->showHideCurveCadence(account->show_cadence_curve);
    mainPlot->showHideCurveHeartRate(account->show_hr_curve);
    mainPlot->showHideCurvePower(account->show_power_curve);
    mainPlot->showHideCurveSpeed(account->show_speed_curve);


    ui->widget_topMenu->setWorkoutNameLabel(workout.getName());

    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {
        ui->widget_topMenu->setWorkoutNameLabel(tr("Free Ride"));
        ui->widget_time->setFreeRideMode();
        ui->widget_topMenu->setFreeRideMode();
    }
    else if (workout.getWorkoutNameEnum() == Workout::MAP_TEST) {
        ui->widget_time->setMAPMode();
        ui->widget_topMenu->setMAPMode();
    }
    if (workout.getWorkoutNameEnum() == Workout::MAP_TEST || workout.getWorkoutNameEnum() == Workout::OPEN_RIDE)
        ui->widget_workoutPlot->setSpinBoxDisabled();


    /// First Interval
    if (workout.getWorkoutNameEnum() != Workout::OPEN_RIDE) {

        timeWorkoutRemaining = workout.getDurationQTime();
        ui->widget_time->setWorkoutRemainingTime(timeWorkoutRemaining);
        ui->widget_topMenu->setWorkoutRemainingTime(timeWorkoutRemaining);

        Interval firstInterval = workout.getInterval(0);
        timeInterval = firstInterval.getDurationQTime();
        ui->widget_time->setIntervalTime(timeInterval);
        ui->widget_topMenu->setIntervalTime(timeInterval);

        adjustTargets(firstInterval);
    }
    else {
        // open ride
        timeWorkoutRemaining = timeInterval = QTime(0,0,0,0);
        ui->widget_time->setIntervalTime(timeInterval);
        ui->widget_topMenu->setIntervalTime(timeInterval);

        targetPowerChanged_f(-1, 30);
        targetCadenceChanged_f(-1, 20);
        targetHrChanged_f(-1, 20);
    }
    ui->wid_1_workoutPlot_HeartrateZoom->setPosition(0);
    ui->wid_2_workoutPlot_PowerZoom->setPosition(0);
    ui->wid_3_workoutPlot_CadenceZoom->setPosition(0);


    if (account->enable_studio_mode) {
        showTimerOnTop(true);
        ui->wid_1_infoBoxHr->setVisible(false);
        ui->wid_1_infoBoxHr->setVisible(false);
        ui->wid_1_minimalistHr->setVisible(false);
        ui->wid_1_workoutPlot_HeartrateZoom->setVisible(false);
        ui->wid_2_balancePower->setVisible(false);
        ui->wid_2_infoBoxPower->setVisible(false);
        ui->wid_2_minimalistPower->setVisible(false);
        ui->wid_2_workoutPlot_PowerZoom->setVisible(false);
        ui->wid_3_infoBoxCadence->setVisible(false);
        ui->wid_3_minimalistCadence->setVisible(false);
        ui->wid_3_workoutPlot_CadenceZoom->setVisible(false);
        ui->wid_4_infoBoxSpeed->setVisible(false);
        ui->wid_5_infoWorkout->setVisible(false);
        ui->wid_oxygen->setVisible(false);

        //Disable Workout Config Option not applicable
        dconfig->setStudioMode();
    }

    setMessagePlot();
}



//-----------------------------------------------------------------------------------------------------------
void WorkoutDialog::moveWidgetsPosition() {


    QHBoxLayout *horizontalLayout = static_cast<QHBoxLayout*>(ui->horizontalLayout_Bottom->layout());

    horizontalLayout->removeWidget(ui->widget_time);
    horizontalLayout->removeWidget(ui->wid_1_infoBoxHr);
    horizontalLayout->removeWidget(ui->wid_1_workoutPlot_HeartrateZoom);
    horizontalLayout->removeWidget(ui->wid_1_minimalistHr);

    horizontalLayout->removeWidget(ui->wid_2_balancePower);
    horizontalLayout->removeWidget(ui->wid_2_infoBoxPower);
    horizontalLayout->removeWidget(ui->wid_2_workoutPlot_PowerZoom);
    horizontalLayout->removeWidget(ui->wid_2_minimalistPower);

    horizontalLayout->removeWidget(ui->wid_3_infoBoxCadence);
    horizontalLayout->removeWidget(ui->wid_3_workoutPlot_CadenceZoom);
    horizontalLayout->removeWidget(ui->wid_3_minimalistCadence);

    horizontalLayout->removeWidget(ui->wid_4_infoBoxSpeed);
    horizontalLayout->removeWidget(ui->wid_oxygen);
    horizontalLayout->removeWidget(ui->wid_5_infoWorkout);


    // insert in good order
    for (int i=0; i<account->getNumberWidget(); i++) {
        if (account->tab_display[i] == account->getTimerStr() ) {
            horizontalLayout->addWidget(ui->widget_time);
        }
        else if (account->tab_display[i] == account->getHrStr()) {
            horizontalLayout->addWidget(ui->wid_1_infoBoxHr);
            horizontalLayout->addWidget(ui->wid_1_workoutPlot_HeartrateZoom);
            horizontalLayout->addWidget(ui->wid_1_minimalistHr);
        }
        else if (account->tab_display[i] == account->getPowerStr()) {
            horizontalLayout->addWidget(ui->wid_2_infoBoxPower);
            horizontalLayout->addWidget(ui->wid_2_workoutPlot_PowerZoom);
            horizontalLayout->addWidget(ui->wid_2_minimalistPower);
        }
        else if (account->tab_display[i] == account->getCadenceStr()) {
            horizontalLayout->addWidget(ui->wid_3_infoBoxCadence);
            horizontalLayout->addWidget(ui->wid_3_workoutPlot_CadenceZoom);
            horizontalLayout->addWidget(ui->wid_3_minimalistCadence);
        }
        else if (account->tab_display[i] == account->getPowerBalanceStr()) {
            horizontalLayout->addWidget(ui->wid_2_balancePower);
        }
        else if (account->tab_display[i] == account->getSpeedStr()) {
            horizontalLayout->addWidget(ui->wid_4_infoBoxSpeed);
        }
        else if (account->tab_display[i] == account->getInfoWorkoutStr()) {
            horizontalLayout->addWidget(ui->wid_5_infoWorkout);
        }
        else { //settings->tabDisplay[i] == settings->getOxygenStr()
            horizontalLayout->addWidget(ui->wid_oxygen);
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::updatePausedTime(double totalTimePaused_msec) {

    this->totalTimePausedWorkout_msec = totalTimePaused_msec;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::updateRealTimeGraph(double totalTimeElapsed_msec) {

    timeElapsed_sec = totalTimeElapsed_msec /1000;

    if (!account->enable_studio_mode) {
        ui->wid_1_workoutPlot_HeartrateZoom->moveIntervalTime(totalTimeElapsed_msec);
        ui->wid_2_workoutPlot_PowerZoom->moveIntervalTime(totalTimeElapsed_msec);
        ui->wid_3_workoutPlot_CadenceZoom->moveIntervalTime(totalTimeElapsed_msec);
    }
}


// For MAP Test, check last second average, if it's under the target for more than 20secs total or 10 sec consecutive, MAP test over
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::checkMAPTestOver() {

    qDebug() << "checkMAPTestOver!";


    bool fadeIn = false;
    //(ignore 3 first and 3 last second of Intervals with soundsActive check)
    if (soundsActive && averagePower1sec.at(0) < currentTargetPower-currentTargetPowerRange) {
        totalSecOffTargetInInterval++;
        totalConsecutiveOffTarget++;
        fadeIn = true;
    }
    else {
        totalConsecutiveOffTarget = 0;
    }

    QString redFontStart  = "<font color=\"red\">";
    QString fontEnd = "</font>";

    QString textTotalOffTargetInterval = tr("Total remaining: ")  + QString::number(20-totalSecOffTargetInInterval) + " sec.";
    if (totalSecOffTargetInInterval >= 15)
        textTotalOffTargetInterval = tr("Total remaining: ")+ redFontStart + QString::number(20-totalSecOffTargetInInterval) + " sec." + fontEnd;

    QString textConsecutiveOffTarget =  tr("Consecutive remaining: ") + QString::number(10-totalConsecutiveOffTarget) + " sec.";
    if (totalConsecutiveOffTarget >= 5)
        textConsecutiveOffTarget = tr("Consecutive remaining: ")+ redFontStart + QString::number(10-totalConsecutiveOffTarget) + " sec." + fontEnd;

    //Update Time remaining off target (top Left of graph)
    mainPlot->setAlertMessage(fadeIn, false, tr("MAP Interval ") + "#" + QString::number(currMAPInterval) + " - " + QString::number(currentTargetPower) + " watts"
                              + "<div style='color:white;height:7px;'>---------------------------------</div><br/> " + textTotalOffTargetInterval +
                              + "<br/>" + textConsecutiveOffTarget, 500);
    //Check if test interval over
    if (totalSecOffTargetInInterval >= 20 || totalConsecutiveOffTarget >= 10) {

        int secCompletedInInterval = 180 - Util::convertQTimeToSecD(timeInterval);
        double mapResult = (currentTargetPower - 30) + (secCompletedInInterval/180.0 * currentTargetPower/10.0);
        mainPlot->setAlertMessage(true, false, workout.getName() + tr(" Result")
                                  + "<div style='color:white;height:7px;'>-------------------</div><br/> "  + QString::number(mapResult, 'f', 1)  + " watts", 500);
        //Go to cooldown interval (last interval)
        moveToInterval(workout.getNbInterval()-1, -1, -1, false);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::update1sec(double totalTimeElapsed_sec) {

    //    qDebug() << "#Update 1sec" << totalTimeElapsed_sec;
    int totalTimeElapsed_sec_i = (int)totalTimeElapsed_sec;

    // Send last second data to DataWorkout, at second 2, send data received between [second 1 - second 2]
    sendLastSecondData(totalTimeElapsed_sec_i);

    // Check MAP Test stop condition
    if (!account->enable_studio_mode && workout.getWorkoutNameEnum() == Workout::MAP_TEST && currentIntervalObj.isTestInterval() && averagePower1sec.at(0) != -1) {
        checkMAPTestOver();
    }


    // Reset mean 1 sec
    averageHr1sec.fill(-1);
    averageCadence1sec.fill(-1);
    averageSpeed1sec.fill(-1);
    averagePower1sec.fill(-1);

    nbPointHr1sec.fill(0);
    nbPointCadence1sec.fill(0);
    nbPointSpeed1sec.fill(0);
    nbPointPower1sec.fill(0);

    //temp
    for (int i=0; i<constants::nbMaxUserStudio; i++) {
        arrNbPointPower[i] = 0;
    }


    avgRightPedal1sec.fill(-1);
    avgLeftTorqueEff.fill(-1);
    avgRightTorqueEff.fill(-1);
    avgLeftPedalSmooth.fill(-1);
    avgRightPedalSmooth.fill(-1);
    avgCombinedPedalSmooth.fill(-1);
    avgSaturatedHemoglobinPercent1sec.fill(-1);
    avgTotalHemoglobinConc1sec.fill(-1);

    // Update the graph 'dark' area
    ui->widget_workoutPlot->updateMarkerTimeNow(totalTimeElapsed_sec);

    // Update minutes rode after 60secs
    nbUpdate1Sec++ ;
    if (nbUpdate1Sec == 60) {
        achievementManager->updateMinuteRode(1);
        nbUpdate1Sec = 0;
    }

    // Update Timers
    timeElapsedTotal = timeElapsedTotal.addSecs(1);
    ui->widget_time->setWorkoutTime(timeElapsedTotal);
    ui->widget_topMenu->setWorkoutTime(timeElapsedTotal);

    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {
        timeInterval = timeInterval.addSecs(1);
        ui->widget_time->setIntervalTime(timeInterval);
        ui->widget_topMenu->setIntervalTime(timeInterval);
    }
    else {
        timeInterval = timeInterval.addSecs(-1);
        ui->widget_time->setIntervalTime(timeInterval);
        ui->widget_topMenu->setIntervalTime(timeInterval);
        timeWorkoutRemaining = timeWorkoutRemaining.addSecs(-1);
        ui->widget_time->setWorkoutRemainingTime(timeWorkoutRemaining);
        ui->widget_topMenu->setWorkoutRemainingTime(timeWorkoutRemaining);

    }

    // Early exit
    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {
        sendSlopes(0);
        return;
    }


    // To show next interval length instead of 0:00 in last second of an interval
    if (changeIntervalDisplayNextSecond) {
        moveToNextInterval();
        //if mamp test, insert interval until failure
        if (workout.getWorkoutNameEnum() == Workout::MAP_TEST && currentIntervalObj.isTestInterval())   //22 interval, start test interval
            insertInterval();
    }


    // Calculate new target
    if (!ignoreCondition) {
        Interval currInterval = workout.getLstInterval().at(currentInterval);
        double newTargetPower = calculateNewTargetPower();
        int range = currInterval.getFTP_range();
        targetPowerChanged_f(newTargetPower, range);

        int newTargetCadence2 = calculateNewTargetCadence();
        int range2 = currInterval.getCadence_range();
        targetCadenceChanged_f(newTargetCadence2, range2);

        double newTargetHr = calculateNewTargetHr();
        int range3 = currInterval.getHR_range();
        targetHrChanged_f(newTargetHr, range3);
    }



    if(timeInterval.minute()==0 && timeInterval.hour()== 0 && (timeInterval.second()==account->nb_sec_show_interval_before || timeInterval.second()==3 || timeInterval.second()==2 || timeInterval.second()==1) )  {
        soundsActive = false;
        if (currentInterval+1 != this->workout.getNbInterval()) {

            if (timeInterval.second() == account->nb_sec_show_interval_before) {
                //show next interval message
                Interval newInterval = workout.getLstInterval().at(currentInterval+1);
                if (newInterval.getDisplayMessage() != "")
                    ui->widget_workoutPlot->setDisplayIntervalMessage(true, tr("Next Interval: ") + newInterval.getDisplayMessage(), account->nb_sec_show_interval);
            }
            else if (timeInterval.second()==3 || timeInterval.second()==2 || timeInterval.second()==1) {
                if (account->enable_sound && account->sound_interval)
                    soundPlayer->playSoundFirstBeepInterval();
                if (timeInterval.second()==1 && (currentInterval+1 < this->workout.getNbInterval()) ) {
                    changeIntervalDisplayNextSecond = true;
                }
            }
        }
    }
    else if(timeInterval.second()==0 && timeInterval.minute()==0 && timeInterval.hour()== 0) {

        //calculate pausedTime
        int intervalPausedTime_msec = totalTimePausedWorkout_msec - lastIntervalTotalTimePausedWorkout_msec;
        changeIntervalsDataWorkout(lastIntervalEndTime_msec, totalTimeElapsed_sec, intervalPausedTime_msec, false, currentIntervalObj.isTestInterval());


        ///---- Check if it's a SufferFestworkout, to sync video with workout on second interval
        if (workout.getWorkoutNameEnum() == Workout::SUFFERFEST_WORKOUT && currentInterval == 0) {
            qDebug() << "SufferFest workout - Adjust to start!";
            int startVideoMs = WorkoutUtil::startVideoSufferfest(workout.getName());
            if (startVideoMs != -1)
                ui->widgetVideo->setMovieTime(startVideoMs);
        }

        timerCheckToActivateSound->start();
        currentInterval++;

        if (workout.getWorkoutNameEnum() == Workout::MAP_TEST && currentIntervalObj.isTestInterval()) {
            achievementManager->checkMAPAchievement(currMAPInterval);
            totalSecOffTargetInInterval = 0;
            currMAPInterval++;
        }


        // Workout over?
        if ( currentInterval >= workout.getNbInterval() ) {
            emit pauseClock();
            qDebug()<< "GOT HERE CHECK #3 WORKOUT OVER!!!**";
            workoutOver();
            ui->wid_1_workoutPlot_HeartrateZoom->setPosition(Util::convertQTimeToSecD(workout.getDurationQTime())*1000);
            ui->wid_2_workoutPlot_PowerZoom->setPosition(Util::convertQTimeToSecD(workout.getDurationQTime())*1000);
            ui->wid_3_workoutPlot_CadenceZoom->setPosition(Util::convertQTimeToSecD(workout.getDurationQTime())*1000);
            return;
        }

        if (account->enable_sound && account->sound_interval)
            soundPlayer->playSoundLastBeepInterval();

        currentIntervalObj = workout.getInterval(currentInterval);
        timeInterval = currentIntervalObj.getDurationQTime();
    }


    ignoreCondition = false;
}



//--------------------------------------------------------------------------------------------------------------
void WorkoutDialog::lapButtonPressed() {

    qDebug() << "LAP BUTTON PRESSED" << timeElapsed_sec;

    if (isTestWorkout) {
        qDebug() << "Test workout, cant do laps!";
        return;
    }

    int intervalPausedTime_msec = totalTimePausedWorkout_msec - lastIntervalTotalTimePausedWorkout_msec;
    changeIntervalsDataWorkout(lastIntervalEndTime_msec, timeElapsed_sec, intervalPausedTime_msec, false, false);

    ui->widget_workoutPlot->addMarkerInterval(timeElapsed_sec);
    ui->wid_1_workoutPlot_HeartrateZoom->addMarkerInterval(timeElapsed_sec);
    ui->wid_2_workoutPlot_PowerZoom->addMarkerInterval(timeElapsed_sec);
    ui->wid_3_workoutPlot_CadenceZoom->addMarkerInterval(timeElapsed_sec);
    ui->widget_workoutPlot->replot();

    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {
        timeInterval = QTime(0,0,0,0);
        ui->widget_time->setIntervalTime(timeInterval);
        ui->widget_topMenu->setIntervalTime(timeInterval);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::adjustWorkoutDifficulty(int percentageIncrease) {

    double diffFromActualDifficulty = percentageIncrease - currentWorkoutDifficultyPercentage;
    diffFromActualDifficulty = diffFromActualDifficulty/100.0;

    //    qDebug() << "ADJUST WORKOUT DIFFICULTY!!" << percentageIncrease << "diff100: " << diffFromActualDifficulty;

    // pause workout if not paused
    if (!isWorkoutPaused) {
        start_or_pause_workout();
    }

    // Compute new workout
    QList<Interval> lstIntervalAdjusted;

    foreach (Interval interval, workout.getLstInterval()) {

        if (interval.getPowerStepType() != Interval::NONE) {
            //do not check negative, could change progressive interval to flat if so..
            interval.setTargetFTP_start(interval.getFTP_start() + diffFromActualDifficulty);
            interval.setTargetFTP_end(interval.getFTP_end() + diffFromActualDifficulty);
        }
        if (interval.getHRStepType() != Interval::NONE) {

            interval.setTargetHR_start(interval.getHR_start() + diffFromActualDifficulty);
            interval.setTargetHR_end(interval.getHR_end() + diffFromActualDifficulty);
        }
        lstIntervalAdjusted.append(interval);
    }


    Workout workoutEdited(workout.getFilePath(), workout.getWorkoutNameEnum(), lstIntervalAdjusted,
                          workout.getName(), workout.getCreatedBy(), workout.getDescription(), workout.getPlan(), workout.getType());
    this->workout = workoutEdited;

    // refresh view
    ui->widget_workoutPlot->setWorkoutData(workout, false);
    ui->wid_1_workoutPlot_HeartrateZoom->setWorkoutData(workout, WorkoutPlotZoomer::HEART_RATE, false);
    ui->wid_2_workoutPlot_PowerZoom->setWorkoutData(workout, WorkoutPlotZoomer::POWER, false);
    ui->wid_3_workoutPlot_CadenceZoom->setWorkoutData(workout, WorkoutPlotZoomer::CADENCE, false);

    //adjust mini-graph and widget to new target
    currentIntervalObj = workout.getInterval(currentInterval);
    adjustTargets(currentIntervalObj);

    currentWorkoutDifficultyPercentage = percentageIncrease;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::insertInterval() {

    qDebug() << "INSERT INTERVAL!";

    //add parameter function
    int whereToInsert = currentInterval+2;


    // pause workout if not paused [to remove, should be done in real time]
    //    if (!isWorkoutPaused) {
    //        start_or_pause_workout();
    //    }

    // Compute new workout
    QList<Interval> lstIntervalModified = workout.getLstInterval();

    // for Mamp test, take next interval and increase 30W
    Interval nextInterval = lstIntervalModified.at(currentInterval+1);
    double currentWattsLevel = nextInterval.getFTP_start() * account->FTP;
    double nextWattsLevel = currentWattsLevel + 30;
    double inFTP = nextWattsLevel/account->FTP;

    QString msgNextInterval = QString::number(nextWattsLevel) + " watts";
    nextInterval.setTargetFTP_start(inFTP);
    nextInterval.setDisplayMsg(msgNextInterval);

    lstIntervalModified.insert(whereToInsert, nextInterval);

    Workout workoutEdited(workout.getFilePath(), workout.getWorkoutNameEnum(), lstIntervalModified,
                          workout.getName(), workout.getCreatedBy(), workout.getDescription(), workout.getPlan(), workout.getType());
    this->workout = workoutEdited;

    // refresh view
    ui->widget_workoutPlot->setWorkoutData(workout, false);
    ui->wid_1_workoutPlot_HeartrateZoom->setWorkoutData(workout, WorkoutPlotZoomer::HEART_RATE, false);
    ui->wid_2_workoutPlot_PowerZoom->setWorkoutData(workout, WorkoutPlotZoomer::POWER, false);
    ui->wid_3_workoutPlot_CadenceZoom->setWorkoutData(workout, WorkoutPlotZoomer::CADENCE, false);

    ui->widget_workoutPlot->updateMarkerTimeNow(timeElapsed_sec);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::moveToInterval(int nbInterval, double secWorkout, double startIntervalSec, bool showConfirmation) {

    //clear focus on QwtPlot for hotkeys needs focus on this QDialog
    this->setFocus();

    // Not legal for test workout or Open Ride (except Manual = showConfirmation at false)
    if (showConfirmation && (bignoreClickPlot || isTestWorkout  || workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) )
        return;

    qDebug() << "Workout Dialog, move to interval" << nbInterval << "sec:" << secWorkout << "startIntervalSec" << startIntervalSec;

    int nbIntervalToDelete = 0;
    if (currentInterval >= nbInterval)
        return;
    else {
        nbIntervalToDelete = nbInterval - currentInterval -1;
    }
    qDebug() << "we have to delete " << nbIntervalToDelete << "interval";

    // pause workout if not paused and manually clicked
    if (!isWorkoutPaused && showConfirmation) {
        start_or_pause_workout();
    }

    QString timeStartInterval = Util::showQTimeAsString(Util::convertMinutesToQTime(startIntervalSec/60.0));

    //ask confirmation
    if (showConfirmation) {
        isAskingUserQuestion = true;
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setText(tr("Move to the interval starting at: ") + timeStartInterval + "?");
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::No);
        if (msgBox.exec() == QMessageBox::No) {
            isAskingUserQuestion = false;
            return;
        }
        isAskingUserQuestion = false;
    }


    //Calculate timeDone in currentInterval
    double currentIntervalTimeDone =  timeElapsed_sec - lastIntervalEndTime_sec;
    double timeSkippedInInterval = Util::convertQTimeToSecD(currentIntervalObj.getDurationQTime()) - currentIntervalTimeDone;
    lastIntervalEndTime_sec += currentIntervalTimeDone;
    qDebug() << "We did:" << currentIntervalTimeDone << " of the current interval, we skipped:" << timeSkippedInInterval;

    // Compute new workout and new interval
    QList<Interval> copyLstInterval = workout.getLstInterval();
    Interval intervalToModify = currentIntervalObj;

    // Update time interval
    QTime timeActuallyDone(0,0,0,0);
    timeActuallyDone = timeActuallyDone.addMSecs(currentIntervalTimeDone*1000);
    intervalToModify.setTime(timeActuallyDone);
    qDebug() << "OLD INTERVAL TIME WAS:" << currentIntervalObj.getDurationQTime() << " NOW IT'S:" << intervalToModify.getDurationQTime();

    // Adjust interval target (for drawing purpose only)
    if (intervalToModify.getPowerStepType() == Interval::PROGRESSIVE) {
        double totalSec = Util::convertQTimeToSecD(currentIntervalObj.getDurationQTime());
        /// y=ax+b
        double b = currentIntervalObj.getFTP_start();
        double a = (currentIntervalObj.getFTP_end() - b) / totalSec;
        double x = currentIntervalTimeDone;
        double y = a*x + b;
        intervalToModify.setTargetFTP_end(y);
    }
    if (intervalToModify.getCadenceStepType() == Interval::PROGRESSIVE) {
        double totalSec = Util::convertQTimeToSecD(currentIntervalObj.getDurationQTime());
        /// y=ax+b
        double b = currentIntervalObj.getCadence_start();
        double a = (currentIntervalObj.getCadence_end() - b) / totalSec;
        double x = currentIntervalTimeDone;
        double y = a*x + b;
        intervalToModify.setTargetCadence_end(y);
    }
    if (intervalToModify.getHRStepType() == Interval::PROGRESSIVE) {
        double totalSec = Util::convertQTimeToSecD(currentIntervalObj.getDurationQTime());
        /// y=ax+b
        double b = currentIntervalObj.getHR_start();
        double a = (currentIntervalObj.getHR_end() - b) / totalSec;
        double x = currentIntervalTimeDone;
        double y = a*x + b;
        intervalToModify.setTargetHR_end(y);
    }
    copyLstInterval.replace(currentInterval, intervalToModify);


    //Remove interval that we skipped (if clicked more than 1 interval ahead)
    for (int i=0; i<nbIntervalToDelete; i++) {
        copyLstInterval.removeAt(currentInterval+1);
    }

    Workout workoutEdited(workout.getFilePath(), workout.getWorkoutNameEnum(), copyLstInterval,
                          workout.getName(), workout.getCreatedBy(), workout.getDescription(), workout.getPlan(), workout.getType());


    this->workout = workoutEdited;

    // refresh view
    ui->widget_workoutPlot->setWorkoutData(workout, false);
    ui->wid_1_workoutPlot_HeartrateZoom->setWorkoutData(workout, WorkoutPlotZoomer::HEART_RATE, false);
    ui->wid_2_workoutPlot_PowerZoom->setWorkoutData(workout, WorkoutPlotZoomer::POWER, false);
    ui->wid_3_workoutPlot_CadenceZoom->setWorkoutData(workout, WorkoutPlotZoomer::CADENCE, false);


    //Create a lap for FIT FIle
    if (timeElapsed_sec - lastIntervalEndTime_msec > 1) {
        int intervalPausedTime_msec = totalTimePausedWorkout_msec - lastIntervalTotalTimePausedWorkout_msec;
        changeIntervalsDataWorkout(lastIntervalEndTime_msec, timeElapsed_sec, intervalPausedTime_msec, false, currentIntervalObj.isTestInterval());
    }

    //Go to selected interval
    currentInterval++;
    currentIntervalObj = workout.getInterval(currentInterval);
    adjustTargets(currentIntervalObj);

    if (currentIntervalObj.getDisplayMessage() != "")
        ui->widget_workoutPlot->setDisplayIntervalMessage(false, currentIntervalObj.getDisplayMessage(), account->nb_sec_show_interval);


    //-- Update Timers
    timeInterval = currentIntervalObj.getDurationQTime();
    qDebug() << "NEXT INTERVAL IS LONG :" << timeInterval;
    ui->widget_time->setIntervalTime(timeInterval);
    ui->widget_topMenu->setIntervalTime(timeInterval);

    //calculate workoutRemainingTime
    timeWorkoutRemaining = QTime(0,0,0,0);
    for (int i=currentInterval; i<workout.getLstInterval().size(); i++) {
        timeWorkoutRemaining = timeWorkoutRemaining.addSecs(Util::convertQTimeToSecD(workout.getInterval(i).getDurationQTime()));
    }
    ui->widget_time->setWorkoutRemainingTime(timeWorkoutRemaining);
    ui->widget_topMenu->setWorkoutRemainingTime(timeWorkoutRemaining);

    //start timer ignore click (click on QDialog response trigger a new click event)
    bignoreClickPlot = true;
    timerIgnoreClickPlot->start(500);
}


//--------------------------------------------------------------------------------------------------------------
void WorkoutDialog::moveToNextInterval() {

    qDebug() << "moveToNextInterval";

    int timeToAdd = Util::convertQTimeToSecD(currentIntervalObj.getDurationQTime());
    lastIntervalEndTime_sec += timeToAdd;
    qDebug() << "ok adding " << timeToAdd << "to lastIntervalEndTime is now:" << lastIntervalEndTime_sec;

    changeIntervalDisplayNextSecond = false;
    ignoreCondition = true;


    Interval newInterval = workout.getLstInterval().at(currentInterval+1);
    if (newInterval.getDisplayMessage() != "")
        ui->widget_workoutPlot->setDisplayIntervalMessage(false, newInterval.getDisplayMessage(), account->nb_sec_show_interval);

    ui->widget_time->setIntervalTime(newInterval.getDurationQTime());
    ui->widget_topMenu->setIntervalTime(newInterval.getDurationQTime());

    adjustTargets(newInterval);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::startWorkout() {


    emit startClock();

    QDateTime dateTimeStartedWorkout;
    dateTimeStartedWorkout = QDateTime::currentDateTime();

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            arrDataWorkout[i]->setStartTimeWorkout(dateTimeStartedWorkout.toUTC());
            UserStudio userStudio = vecUserStudio.at(i);
            QString userIdentifier = "user" + QString::number(i+1) + "-" + Util::cleanForOsSaving(userStudio.getDisplayName());
            arrDataWorkout[i]->initFitFile(true, userIdentifier, workout.getName(), dateTimeStartedWorkout.toUTC() );
        }
    }
    else {
        arrDataWorkout[0]->setStartTimeWorkout(dateTimeStartedWorkout.toUTC());
        arrDataWorkout[0]->initFitFile(false, account->email_clean, workout.getName(), dateTimeStartedWorkout.toUTC() );
    }

    timerCheckToActivateSound->start();
}




////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::workoutOver() {

    isWorkoutOver = true;
    stopErgSmoothing();

    qDebug() << "STOPPING WORKOUT";
    if (account->enable_sound && account->sound_end_workout)
        soundPlayer->playSoundEndWorkout();

//    ui->widget_workoutPlot->setMessageEndWorkout();
    ui->widget_workoutPlot->setDisplayIntervalMessage(true, tr("Workout Completed!"), 20000);

    isWorkoutPaused = true;
    ui->widget_topMenu->setButtonStartReady(false);
    ui->widget_topMenu->setButtonLapVisible(false);
    ui->widget_workoutPlot->setSpinBoxDisabled();


    setWidgetsStopped(true);

    qDebug() << "OK CHECKING IF WORKOUT ACHIEVEMENT WITH LENGTH:" << QString::number(Util::convertQTimeToSecD(workout.getDurationQTime()) );
    achievementManager->updateMinuteRode(1);
    achievementManager->workoutCompleted(workout);


    //Close FIT FILE
    closeFitFiles(timeElapsed_sec);


    // Show Test Result?
    showTestResult();



    // Set workout to done
    account->hashWorkoutDone.insert(workout.getName());

    // Save data DB (update Achievements, stats etc.)
    if (!account->isOffline)
        UserDAO::putAccount(account);
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::onBleConnectionError(const QString &errorString)
{
    // On WASM the DOM overlay (#ble-reconnect-overlay) already provides the
    // Reconnect button; this slot handles the C++-side notification.
    // Pause the workout so the rider is not penalised while reconnecting.
    LOG_WARN("WorkoutDialog", QStringLiteral("BLE connection error: ") + errorString);
    if (isWorkoutStarted && !isWorkoutPaused) {
        start_or_pause_workout(); // pause
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::checkSensorDropout()
{
    if (!account->sensor_dropout_enabled) return;
    if (account->enable_studio_mode) return;
    if (!isWorkoutStarted || isWorkoutOver) return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 timeout_ms = (qint64)account->sensor_dropout_timeout_s * 1000;

    // Only check a sensor if we've ever received data from it (m_last*Ms > 0)
    const bool powerLost = (m_lastPowerMs > 0) && (now - m_lastPowerMs) > timeout_ms;
    const bool signalLost = powerLost;   // future: add HR option here

    if (!m_dropoutPaused) {
        if (!isWorkoutPaused && signalLost) {
            m_dropoutPaused = true;
            m_recoveryCountdown = 0;
            LOG_INFO("WorkoutDialog", "Sensor dropout detected — auto-pausing workout");
            start_or_pause_workout();  // pause
            ui->widget_workoutPlot->setAlertMessage(false, false,
                tr("Sensor signal lost — workout paused"), 0);
        }
        return;
    }

    // We are in dropout-pause state: check for recovery
    if (signalLost) {
        // Signal still absent (or dropped again during countdown)
        if (m_recoveryCountdown > 0) {
            m_recoveryCountdown = 0;
            ui->widget_workoutPlot->setAlertMessage(false, false,
                tr("Sensor signal lost — workout paused"), 0);
        }
    } else {
        // Signal has returned
        if (m_recoveryCountdown == 0) {
            m_recoveryCountdown = 3;
            ui->widget_workoutPlot->setAlertMessage(false, false,
                tr("Signal recovered — resuming in 3…"), 0);
        } else {
            m_recoveryCountdown--;
            if (m_recoveryCountdown == 0) {
                m_dropoutPaused = false;
                LOG_INFO("WorkoutDialog", "Sensor recovered — auto-resuming workout");
                ui->widget_workoutPlot->removeAlertMessage();
                if (isWorkoutPaused)
                    start_or_pause_workout();  // resume
            } else {
                ui->widget_workoutPlot->setAlertMessage(false, false,
                    tr("Signal recovered — resuming in %1…").arg(m_recoveryCountdown), 0);
            }
        }
    }
}

void WorkoutDialog::start_or_pause_workout() {


    qDebug() << "start_or_pause_workout!";


    if (isWorkoutOver) {
        return;
    }

    // If workout not started, we start it
    if (!isWorkoutStarted) {
        if (account->enable_sound  && account->sound_pause_resume_workout)
            soundPlayer->playSoundStartWorkout();
        ui->widget_topMenu->setButtonStartPaused(true);
        ui->widget_workoutPlot->removeMainMessage();
        isWorkoutStarted = true;
        isWorkoutPaused = false;
        if (this->workout.getInterval(0).getDisplayMessage() != "")
            ui->widget_workoutPlot->setDisplayIntervalMessage(true, this->workout.getInterval(0).getDisplayMessage(), account->nb_sec_show_interval);
        setWidgetsStopped(false);
        startWorkout();
        emit playPlayer();

    }
    // If workout paused, we resume it
    else if (isWorkoutStarted && isWorkoutPaused) {
        qDebug() << "RESUME WORKOUT!!!";
        if (account->enable_sound  && account->sound_pause_resume_workout)
            soundPlayer->playSoundStartWorkout();
        ui->widget_topMenu->setButtonStartPaused(true);
        ui->widget_workoutPlot->removeMainMessage();
        isWorkoutPaused = false;

        // Clear dropout state if user manually resumes
        if (m_dropoutPaused) {
            m_dropoutPaused = false;
            m_recoveryCountdown = 0;
            m_lastPowerMs = QDateTime::currentMSecsSinceEpoch(); // reset to avoid immediate re-trigger
            ui->widget_workoutPlot->removeAlertMessage();
        }

        setWidgetsStopped(false);
        emit resumeClock();
        emit playPlayer();

    }
    // If not paused, we pause it
    else if (isWorkoutStarted && !isWorkoutPaused) {
        qDebug() << "PAUSE WORKOUT!!!";
        if (account->enable_sound  && account->sound_pause_resume_workout)
            soundPlayer->playSoundPauseWorkout();
        ui->widget_topMenu->setButtonStartPaused(false);
        isWorkoutPaused = true;
        stopErgSmoothing();
        setWidgetsStopped(true);
        setMessagePlot();
        emit pauseClock();
        emit pausePlayer();
    }
}




//--------------------------------------------------------------------------------------------------
void WorkoutDialog::sendLastSecondData(int seconds) {

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            //            qDebug() << "Update Data Workout, User: " << i <<   "HR:" << averageHr1sec.at(i) << "CAD:" << averageCadence1sec.at(i) << "Speed:" << averageSpeed1sec.at(i) << "Power:" << averagePower1sec.at(i) <<
            //                        "rightPedal:";
            arrDataWorkout[i]->updateData(account->enable_studio_mode, seconds, averageHr1sec.at(i), averageCadence1sec.at(i), averageSpeed1sec.at(i), averagePower1sec.at(i),
                                          avgRightPedal1sec.at(i), avgLeftTorqueEff.at(i), avgRightTorqueEff.at(i), avgLeftPedalSmooth.at(i), avgRightPedalSmooth.at(i), avgCombinedPedalSmooth.at(i),
                                          avgSaturatedHemoglobinPercent1sec.at(i), avgTotalHemoglobinConc1sec.at(i));
        }
    }
    else {
        arrDataWorkout[0]->updateData(account->enable_studio_mode, seconds, averageHr1sec.at(0), averageCadence1sec.at(0), averageSpeed1sec.at(0), averagePower1sec.at(0),
                                      avgRightPedal1sec.at(0), avgLeftTorqueEff.at(0), avgRightTorqueEff.at(0), avgLeftPedalSmooth.at(0), avgRightPedalSmooth.at(0), avgCombinedPedalSmooth.at(0),
                                      avgSaturatedHemoglobinPercent1sec.at(0), avgTotalHemoglobinConc1sec.at(0));
    }
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::HrDataReceived(int userID, int value) {


    //    qDebug() << "UserID" << userID << "userHR:" << value;

    // invalid value, show "-" to the user
    if (value == -1) {
        ui->wid_1_infoBoxHr->setValue(value);
        ui->wid_1_workoutPlot_HeartrateZoom->updateTextLabelValue(value);
        ui->wid_1_minimalistHr->setValue(value);
        if (account->enable_studio_mode) {
            arrUserStudioWidget[userID-1]->setHrValue(value);
        }
        return;
    }
    if (value < 0)
        return;

    // Track for sensor dropout detection
    if (!account->enable_studio_mode && isWorkoutStarted && !isWorkoutOver)
        m_lastHrMs = QDateTime::currentMSecsSinceEpoch();


    // Mark pairing as done
    if (!account->enable_studio_mode && !hrPairingDone) {
        hrPairingDone = true;
        labelPairHr->setText(labelPairHr->text() + msgPairingDone);
        labelPairHr->setStyleSheet("background-color : rgba(1,1,1,220); color : green;");
        checkPairingCompleted();
    }


    if (!isWorkoutPaused && !isWorkoutOver) {

        arrDataWorkout[userID-1]->checkUpdateMaxHr(value);

        if (nbPointHr1sec.at(userID-1) == 0) {
            averageHr1sec.replace(userID-1, value);
        }
        else {
            int nbPoint = nbPointHr1sec.at(userID-1);
            double firstEle = averageHr1sec.at(userID-1) *((double)nbPoint/(nbPoint+1));
            double secondEle = ((double)value)/(nbPoint+1);
            averageHr1sec.replace(userID-1, firstEle + secondEle);
        }
        nbPointHr1sec.replace(userID-1, nbPointHr1sec.at(userID-1) + 1);


        //Update Graph (zoomer)
        ui->wid_1_workoutPlot_HeartrateZoom->updateCurve(timeElapsed_sec, value);
    }

    // Show data to the display
    ui->wid_1_infoBoxHr->setValue(value);
    ui->wid_1_workoutPlot_HeartrateZoom->updateTextLabelValue(value);
    ui->wid_1_minimalistHr->setValue(value);
    if (account->enable_studio_mode) {
        arrUserStudioWidget[userID-1]->setHrValue(value);
    }
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::CadenceDataReceived(int userID, int value) {


    // invalid value, show "-" to the user
    if (value == -1 || value > 250) {
        ui->wid_3_infoBoxCadence->setValue(value);
        ui->wid_3_workoutPlot_CadenceZoom->updateTextLabelValue(value);
        ui->wid_3_minimalistCadence->setValue(value);
        if (account->enable_studio_mode) {
            arrUserStudioWidget[userID-1]->setCadenceValue(value);
        }
        return;
    }
    if (value < 0)
        return;


    // Mark pairing as done
    if (!account->enable_studio_mode && !cadencePairingDone) {
        cadencePairingDone = true;
        labelCadence->setText(labelCadence->text() + msgPairingDone);
        labelCadence->setStyleSheet("background-color : rgba(1,1,1,220); color : green;");
        checkPairingCompleted();
    }
    if (!account->enable_studio_mode && !scPairingDone) {
        scPairingDone = true;
        labelSpeedCadence->setText(labelSpeedCadence->text() + msgPairingDone);
        labelSpeedCadence->setStyleSheet("background-color : rgba(1,1,1,220); color : green;");
        checkPairingCompleted();
    }


    if (!isWorkoutPaused && !isWorkoutOver) {

        arrDataWorkout[userID-1]->checkUpdateMaxCad(value);

        ///Below Pause treshold?
        if (!account->enable_studio_mode && (account->start_trigger == 0) && (value < account->value_cadence_start) ) {
            start_or_pause_workout();
        }

        ///Check to play sound, 10sec cooldown between sound and not first/last 3 second of an interval
        if (!account->enable_studio_mode && currentTargetCadence != -1 && soundsActive && account->enable_sound) {
            /// TOO LOW
            if (account->sound_alert_cadence_under_target && (value < currentTargetCadence - currentTargetCadenceRange) && soundCadenceTooLowActive) {
                soundPlayer->playSoundCadenceTooLow();
                soundCadenceTooLowActive = false;
                timerCheckReactivateSoundCadenceTooLow->start();
            }
            /// TOO HIGH
            else if (account->sound_alert_cadence_above_target && (value > currentTargetCadence + currentTargetCadenceRange) && soundCadenceTooHighActive) {
                soundPlayer->playSoundCadenceTooHigh();
                soundCadenceTooHighActive = false;
                timerCheckReactivateSoundCadenceTooHigh->start();
            }
        }


        //averaging 1sec
        if (nbPointCadence1sec.at(userID-1) == 0) {
            averageCadence1sec.replace(userID-1, value);
        }
        else {
            int nbPoint = nbPointCadence1sec.at(userID-1);
            double firstEle = averageCadence1sec.at(userID-1) *((double)nbPoint/(nbPoint+1));
            double secondEle = ((double)value)/(nbPoint+1);
            averageCadence1sec.replace(userID-1, firstEle + secondEle);
        }
        nbPointCadence1sec.replace(userID-1, nbPointCadence1sec.at(userID-1) + 1);


        //Update Graph (zoomer)
        ui->wid_3_workoutPlot_CadenceZoom->updateCurve(timeElapsed_sec, value);

    }
    ///Resume Workout?
    if (!account->enable_studio_mode && !isCalibrating && !isAskingUserQuestion && (account->start_trigger == 0) && isWorkoutPaused && !isWorkoutOver && (value > account->value_cadence_start) ) {
        start_or_pause_workout();
    }
    //----------------



    // Show raw data to the display
    ui->wid_3_infoBoxCadence->setValue(value);
    ui->wid_3_workoutPlot_CadenceZoom->updateTextLabelValue(value);
    ui->wid_3_minimalistCadence->setValue(value);
    // UserStudio
    if (account->enable_studio_mode) {
        arrUserStudioWidget[userID-1]->setCadenceValue(value);
    }
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::TrainerSpeedDataReceived(int userID, double value) {

    //    qDebug() <<  "TrainerSpeedDataReceived" << "userId" << userID << "value" << value;

    // invalid value, show "-" to the user
    if (value == -1) {
        ui->wid_4_infoBoxSpeed->setTrainerSpeed(value);
        return;
    }
    if (value < 0)
        return;


    // Mark pairing as done
    if (!account->enable_studio_mode && !speedPairingDone) {
        speedPairingDone = true;
        labelSpeed->setText(labelSpeed->text() + msgPairingDone);
        labelSpeed->setStyleSheet("background-color : rgba(1,1,1,220); color : green;");
        checkPairingCompleted();
    }
    if (!account->enable_studio_mode &&!scPairingDone) {
        scPairingDone = true;
        labelSpeedCadence->setText(labelSpeedCadence->text() + msgPairingDone);
        labelSpeedCadence->setStyleSheet("background-color : rgba(1,1,1,220); color : green;");
        checkPairingCompleted();
    }

    ui->wid_4_infoBoxSpeed->setTrainerSpeed(value);

    // if user want trainer data to represent his speed
    if ( !account->use_virtual_speed || account->enable_studio_mode) {
        speedDataChosen(userID, value);
    }

}


/// VIRTUAL SPEED, COMING FROM POWER DATA CALCULATED TO SPEED
/// VALUE IS M/S
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::VirtualSpeedDataReceived(int userID, double valueMS, double timeAtThisSpeedSec) {


    // invalid value, show "-" to the user
    if (valueMS == -1) {
        ui->wid_4_infoBoxSpeed->setValue(valueMS);
        return;
    }
    if (valueMS < 0)
        return;

    //conver to KM/H
    double valueKMH = valueMS * 3.6;



    if ( account->use_virtual_speed && !account->enable_studio_mode ) {
        speedDataChosen(userID, valueKMH);

        //Update Distance Counter
        if (!isWorkoutPaused && !isWorkoutOver) {
//            qDebug() << "valueMS" << valueMS << "valueKMH" << valueKMH << "timeAtThisSpeedSec" << timeAtThisSpeedSec;
            arrDataWorkout[userID-1]->updateDistance(valueMS*timeAtThisSpeedSec);
        }
    }

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// value = KMH,
void WorkoutDialog::speedDataChosen(int userID, double value) {


    double valueUnit;
    if (account->distance_in_km)
        valueUnit = value;
    else //mph
        valueUnit = value*constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;


    if (!isWorkoutPaused && !isWorkoutOver) {

        arrDataWorkout[userID-1]->checkUpdateMaxSpeed(value);

        ///Below Pause treshold?
        if (!account->enable_studio_mode && (account->start_trigger == 2) && (valueUnit < account->value_speed_start) ) {
            start_or_pause_workout();
        }

        //averaging 1sec
        if (nbPointSpeed1sec.at(userID-1) == 0) {
            averageSpeed1sec.replace(userID-1, value);
        }
        else {
            int nbPoint = nbPointSpeed1sec.at(userID-1);
            double firstEle = averageSpeed1sec.at(userID-1) *((double)nbPoint/(nbPoint+1));
            double secondEle = ((double)value)/(nbPoint+1);
            averageSpeed1sec.replace(userID-1, firstEle + secondEle);
        }
        nbPointSpeed1sec.replace(userID-1, nbPointSpeed1sec.at(userID-1) + 1);
    }
    ///Resume Workout?
    if (!account->enable_studio_mode && !isCalibrating && !isAskingUserQuestion && (account->start_trigger == 2) && isWorkoutPaused && !isWorkoutOver && (valueUnit > account->value_speed_start) ) {
        start_or_pause_workout();
    }


    ui->wid_4_infoBoxSpeed->setValue(value);

}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::PowerDataReceived(int userID, int value) {

    // invalid value, show "-" to the user
    if (value == -1) {
        ui->wid_2_infoBoxPower->setValue(value);
        ui->wid_2_workoutPlot_PowerZoom->updateTextLabelValue(value);
        ui->wid_2_minimalistPower->setValue(value);
        if (account->enable_studio_mode) {
            arrUserStudioWidget[userID-1]->setPowerValue(value);
        }
        return;
    }


    if (!account->enable_studio_mode)
        value = value + account->offset_power;
    if (value < 0)
        return;

    // Track for sensor dropout detection (non-studio, user 1 only)
    if (!account->enable_studio_mode && isWorkoutStarted && !isWorkoutOver)
        m_lastPowerMs = QDateTime::currentMSecsSinceEpoch();

    // Send power To Clock
    emit sendPowerData(userID, value);



    // Mark pairing as done
    if (!account->enable_studio_mode && !powerPairingDone) {
        powerPairingDone = true;
        labelPower->setText(labelPower->text() + msgPairingDone);
        labelPower->setStyleSheet("background-color : rgba(1,1,1,220); color : green;");
        checkPairingCompleted();
    }
    if (!account->enable_studio_mode && !fecPairingDone) {
        fecPairingDone = true;
        labelFEC->setText(labelFEC->text() + msgPairingDone);
        labelFEC->setStyleSheet("background-color : rgba(1,1,1,220); color : green;");
        checkPairingCompleted();
    }



    int rollingAverage = value;
    ///------- Averaging Power ------------------------------------------------------------------------------
    if (!isWorkoutPaused && !isWorkoutOver && account->averaging_power > 0) {

        ///For Testing ---------------------
        //        qDebug() << "Printing Queue Before Value:" << value;
        //        for (int i=0; i< arrQueuePower[userID-1].size(); i++) {
        //            qDebug() << "queue["<<i<<"]=" <<  arrQueuePower[userID-1].at(i);
        //        }


        // Get current second
        int currentSecondPower = (int) timeElapsed_sec;
        // If the second changed, add new value to top of queue
        if (currentSecondPower != arrLastSecondPower[userID-1]) {
            arrQueuePower[userID-1].enqueue(value);
            // check the queue size, remove element if needed (loop: if settings just changed need to remove more than 1 value)
            while (arrQueuePower[userID-1].size() > account->averaging_power) {
                arrQueuePower[userID-1].dequeue();
            }
        }
        // If the second is the same, recalculate average and replace value of the tail
        else {
            ///first second workout
            if (arrQueuePower[userID-1].size() < 1) {
                arrQueuePower[userID-1].enqueue(value);
            }
            else {
                double firstEle = arrQueuePower[userID-1].last() *((double)arrNbPointPower[userID-1]/(arrNbPointPower[userID-1]+1));
                double secondEle = ((double)value)/(arrNbPointPower[userID-1]+1);
                // replace last element of the queue
                double newAvg = firstEle + secondEle;
                if (newAvg > 0)
                    arrQueuePower[userID-1].replace( arrQueuePower[userID-1].size()-1,  newAvg);
            }
        }
        arrNbPointPower[userID-1]++;
        arrLastSecondPower[userID-1] = currentSecondPower;


        /// Get the average of the queue
        double avgQueue = 0.0;
        double totalQueue = 0.0;
        if (arrQueuePower[userID-1].size() > 0) {
            for (int i=0; i<arrQueuePower[userID-1].size(); i++) {
                totalQueue += arrQueuePower[userID-1].at(i);
            }
            avgQueue = totalQueue/arrQueuePower[userID-1].size();
            /// replace value with the rolling average
            if (avgQueue > 0)
                rollingAverage = qRound(avgQueue);
        }


        ///For Testing ---------------------
        //        qDebug() << "Printing Queue After Enqueue:";
        //        for (int i=0; i< arrQueuePower[userID-1].size(); i++) {
        //            qDebug() << "queue["<<i<<"]=" <<  arrQueuePower[userID-1].at(i);
        //        }
        //        qDebug() << "avg Queue is:" << avgQueue << "rollingAverage :" << rollingAverage;
    }
    /// -----------------------------------------------------------------------------------------------------




    if (!isWorkoutPaused && !isWorkoutOver) {

        arrDataWorkout[userID-1]->checkUpdateMaxPower(rollingAverage);

        ///Below Pause treshold?
        if (!account->enable_studio_mode && (account->start_trigger == 1) && (rollingAverage < account->value_power_start) ) {
            start_or_pause_workout();
        }

        ///Check to play sound, 10sec cooldown between sound and not first/last 3 second of an interval
        if (!account->enable_studio_mode && currentTargetPower != -1 && soundsActive && account->enable_sound) {
            /// TOO LOW
            if (account->sound_alert_power_under_target && (rollingAverage < currentTargetPower - currentTargetPowerRange) && soundPowerTooLowActive) {
                soundPlayer->playSoundPowerTooLow();
                soundPowerTooLowActive = false;
                timerCheckReactivateSoundPowerTooLow->start();
            }
            /// TOO HIGH
            else if (account->sound_alert_power_above_target && (rollingAverage > currentTargetPower + currentTargetPowerRange) && soundPowerTooHighActive) {
                soundPlayer->playSoundPowerTooHigh();
                soundPowerTooHighActive = false;
                timerCheckReactivateSoundPowerTooHigh->start();
            }
        }


        //averaging 1sec
        if (nbPointPower1sec.at(userID-1) == 0) {
            averagePower1sec.replace(userID-1, value);
        }
        else {
            int nbPoint = nbPointPower1sec.at(userID-1);
            double firstEle = averagePower1sec.at(userID-1) *((double)nbPoint/(nbPoint+1));
            double secondEle = ((double)value)/(nbPoint+1);
            averagePower1sec.replace(userID-1, firstEle + secondEle);
        }
        nbPointPower1sec.replace(userID-1, nbPointPower1sec.at(userID-1) + 1);

        //Update Graph (zoomer)
        ui->wid_2_workoutPlot_PowerZoom->updateCurve(timeElapsed_sec, rollingAverage);


    }
    ///Resume Workout?
    if (!account->enable_studio_mode && !isCalibrating && !isAskingUserQuestion && (account->start_trigger == 1) && isWorkoutPaused && !isWorkoutOver && (value >= account->value_power_start) ) {
        start_or_pause_workout();
    }


    // Show raw data to the display
    ui->wid_2_infoBoxPower->setValue(rollingAverage);
    ui->wid_2_workoutPlot_PowerZoom->updateTextLabelValue(rollingAverage);
    ui->wid_2_minimalistPower->setValue(rollingAverage);
    if (account->enable_studio_mode) {
        arrUserStudioWidget[userID-1]->setPowerValue(rollingAverage);
    }
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::PowerBalanceDataReceived(int userID, int rightPedalPercentage) {

    ui->wid_2_balancePower->setValue(rightPedalPercentage);
    avgRightPedal1sec.replace(userID-1, rightPedalPercentage);
}


void WorkoutDialog::pedalMetricReceived(int userID, double leftTorqueEff, double rightTorqueEff,
                                        double leftPedalSmooth, double rightPedalSmooth, double combinedPedalSmooth) {

    //    qDebug() << "WorkoutDialog::PEDALMETRIC Received. leftTorqueEff" << leftTorqueEff << "rightTorqueEff" << rightTorqueEff <<
    //                "leftPedalSmooth" << leftPedalSmooth << "rightPedalSmooth" << rightPedalSmooth << "combinedPedalSmooth" << combinedPedalSmooth;

    ui->wid_2_balancePower->pedalMetricChanged(leftTorqueEff, rightTorqueEff, leftPedalSmooth, rightPedalSmooth, combinedPedalSmooth);

    avgLeftTorqueEff.replace(userID-1, leftTorqueEff);
    avgRightTorqueEff.replace(userID-1, rightTorqueEff);
    avgLeftPedalSmooth.replace(userID-1, leftPedalSmooth);
    avgRightPedalSmooth.replace(userID-1, rightPedalSmooth);
    avgCombinedPedalSmooth.replace(userID-1, combinedPedalSmooth);

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::OxygenValueChanged(int userID, double percentageSaturatedHemoglobin, double totalHemoglobinConcentration) { // %, g/d;


    // Mark pairing as done
    if (!account->enable_studio_mode && !oxygenPairingDone) {
        oxygenPairingDone = true;
        labelOxygen->setText(labelOxygen->text() + msgPairingDone);
        labelOxygen->setStyleSheet("background-color : rgba(1,1,1,220); color : green;");
        checkPairingCompleted();
    }
    ui->wid_oxygen->oxygenValueChanged(percentageSaturatedHemoglobin, totalHemoglobinConcentration);

    avgSaturatedHemoglobinPercent1sec.replace(userID-1, percentageSaturatedHemoglobin);
    avgTotalHemoglobinConc1sec.replace(userID-1, totalHemoglobinConcentration);
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::sendTargetsPower(double percentageTarget, int range) {


    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            arrUserStudioWidget[i]->setTargetPower(percentageTarget, range);
        }
    }
    else {
        ui->wid_2_workoutPlot_PowerZoom->targetChanged(percentageTarget, range);
        ui->wid_2_infoBoxPower->targetChanged(percentageTarget, range);
        ui->wid_2_minimalistPower->setTarget(percentageTarget, range);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::sendTargetsCadence(int target, int range) {

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            arrUserStudioWidget[i]->setTargetCadence(target, range);
        }
    }
    else {
        ui->wid_3_workoutPlot_CadenceZoom->targetChanged(target, range);
        ui->wid_3_infoBoxCadence->targetChanged(target, range);
        ui->wid_3_minimalistCadence->setTarget(target, range);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::sendTargetsHr(double percentageTarget, int range) {

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            arrUserStudioWidget[i]->setTargetHr(percentageTarget, range);
        }
    }
    else {
        ui->wid_1_workoutPlot_HeartrateZoom->targetChanged(percentageTarget, range);
        ui->wid_1_infoBoxHr->targetChanged(percentageTarget, range);
        ui->wid_1_minimalistHr->setTarget(percentageTarget, range);
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::sendSlopes(double slope) {


    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            UserStudio myUserStudio = vecUserStudio.at(i);
            if (myUserStudio.getFecID() > 0) {
                emit setSlope(myUserStudio.getFecID(), 0);
            }
        }
    }
    else {
        if (idFecMainUser != -1) {
            emit setSlope(idFecMainUser, slope);
        }
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::sendLoads(double percentageFTP) {

    // Studio mode: no smoothing — each rider has a different FTP, broadcast directly.
    if (account->enable_studio_mode) {
        stopErgSmoothing();
        for (int i = 0; i < account->nb_user_studio; i++) {
            UserStudio myUserStudio = vecUserStudio.at(i);
            if (myUserStudio.getFecID() > 0 && myUserStudio.getFTP() > 0) {
                int userTarget = qRound(percentageFTP * myUserStudio.getFTP());
                qDebug() << "SeindgLOAD!." << userTarget;
                emit setLoad(myUserStudio.getFecID(), userTarget);
            }
        }
        return;
    }

    if (idFecMainUser == -1) {
        stopErgSmoothing();
        return;
    }

    // Compute the new target in watts from the percentage and current FTP.
    const double targetWatts = (account->FTP > 0) ? qRound(percentageFTP * account->FTP) : 0.0;

    // If smoothing is disabled or target is zero, send immediately.
    if (account->erg_smoothing_duration_s <= 0 || targetWatts <= 0) {
        stopErgSmoothing();
        m_ergSmoothLast = targetWatts;
        emit setLoad(idFecMainUser, qRound(targetWatts));
        return;
    }

    // If a ramp is already active toward the same target, let the timer proceed.
    if (m_ergSmoothTimer && m_ergSmoothTimer->isActive() &&
            qRound(targetWatts) == qRound(m_ergSmoothTo)) {
        return;
    }

    // If target matches the last emitted value and no ramp is running, nothing to do.
    if (qRound(targetWatts) == m_ergSmoothLast &&
            (!m_ergSmoothTimer || !m_ergSmoothTimer->isActive())) {
        return;
    }

    // Start a ramp from the last emitted value (handles mid-ramp retargeting correctly).
    const double fromWatts = (m_ergSmoothLast > 0) ? m_ergSmoothLast : targetWatts;
    startErgSmoothing(fromWatts, targetWatts);
}


void WorkoutDialog::startErgSmoothing(double fromWatts, double toWatts)
{
    stopErgSmoothing();

    m_ergSmoothFrom  = fromWatts;
    m_ergSmoothTo    = toWatts;
    m_ergSmoothStep  = 0;
    // Use (duration + 1) steps: step 0 is the immediate command, steps 1..N are timer-driven.
    m_ergSmoothSteps = qMax(1, account->erg_smoothing_duration_s);
    m_ergSmoothAntID = idFecMainUser;

    if (!m_ergSmoothTimer) {
        m_ergSmoothTimer = new QTimer(this);
        m_ergSmoothTimer->setInterval(1000);
        connect(m_ergSmoothTimer, &QTimer::timeout, this, &WorkoutDialog::ergSmoothStep);
    }

    // Emit step 0 immediately (the "from" value to start the transition).
    const int startWatts = qRound(fromWatts);
    m_ergSmoothLast = startWatts;
    emit setLoad(m_ergSmoothAntID, startWatts);

    m_ergSmoothTimer->start();
}

void WorkoutDialog::stopErgSmoothing()
{
    if (m_ergSmoothTimer)
        m_ergSmoothTimer->stop();
    m_ergSmoothStep  = 0;
    m_ergSmoothSteps = 0;
    // Do NOT clear m_ergSmoothLast — it is the "current position" for mid-ramp retargeting.
    m_ergSmoothAntID = -1;
}

void WorkoutDialog::ergSmoothStep()
{
    if (m_ergSmoothAntID == -1 || m_ergSmoothSteps <= 0) {
        stopErgSmoothing();
        return;
    }

    m_ergSmoothStep++;
    const double progress = static_cast<double>(m_ergSmoothStep) / static_cast<double>(m_ergSmoothSteps);
    const double watts    = m_ergSmoothFrom + (m_ergSmoothTo - m_ergSmoothFrom) * qMin(progress, 1.0);
    const int rounded     = qRound(watts);

    m_ergSmoothLast = rounded;
    emit setLoad(m_ergSmoothAntID, rounded);

    if (m_ergSmoothStep >= m_ergSmoothSteps)
        stopErgSmoothing();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::targetPowerChanged_f(double percentageTarget, int range) {


    if (percentageTarget <= 0 || isUsingSlopeMode || !account->control_trainer_resistance) {
        sendSlopes(0);
    }
    else if(account->control_trainer_resistance) {
        sendLoads(percentageTarget);
    }


    currentTargetPower = qRound(percentageTarget * account->FTP);
    currentTargetPowerRange =  range;
    ui->widget_time->setTargetPower(percentageTarget, range);
    ui->widget_topMenu->setTargetPower(percentageTarget, range);
    sendTargetsPower(percentageTarget, range);
}



//////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::targetCadenceChanged_f(int target, int range) {


    currentTargetCadence = target;
    currentTargetCadenceRange = range;
    ui->widget_time->setTargetCadence(target, range);
    ui->widget_topMenu->setTargetCadence(target, range);

    sendTargetsCadence(target, range);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::targetHrChanged_f(double percentageTarget, int range) {

    ui->widget_time->setTargetHeartRate(percentageTarget, range);
    ui->widget_topMenu->setTargetHeartRate(percentageTarget, range);

    sendTargetsHr(percentageTarget, range);
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::adjustTargets(Interval interval) {

    if (interval.isTestInterval() || interval.getPowerStepType() == Interval::NONE || workout.getWorkoutNameEnum() == Workout::OPEN_RIDE)
        isUsingSlopeMode = true;
    else
        isUsingSlopeMode = false;

    ///Power
    if (interval.getPowerStepType() != Interval::NONE) {
        targetPowerChanged_f(interval.getFTP_start(), interval.getFTP_range());
    }
    else
        targetPowerChanged_f(-1, currentIntervalObj.getFTP_range());
    //Cadence


    /// Target cadence
    if (interval.getCadenceStepType() != Interval::NONE)
        targetCadenceChanged_f(interval.getCadence_start(), interval.getCadence_range());
    else
        targetCadenceChanged_f(-1, interval.getCadence_range());


    /// Target hr
    if (interval.getHRStepType() != Interval::NONE)
        targetHrChanged_f(interval.getHR_start(), interval.getHR_range());
    else
        targetHrChanged_f(-1, interval.getHR_range());

    /// Power Balance -  No target
    if (interval.getRightPowerTarget() == -1) {
        ui->wid_2_balancePower->removeZone();
        if (account->display_power_balance == 1) { /// Hide (no target)
            ui->wid_2_balancePower->removeZone();
            ui->wid_2_balancePower->setVisible(false);
        }
    }
    /// Power Balance - Got Target
    else {
        ui->wid_2_balancePower->setZone(interval.getRightPowerTarget(), 3);
        if ((account->display_power_balance == 0 || account->display_power_balance == 1) && !account->enable_studio_mode) {
            ui->wid_2_balancePower->setVisible(true);
        }
    }

}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double WorkoutDialog::calculateNewTargetPower() {

    /// Intervale de base
    if (currentIntervalObj.getPowerStepType() == Interval::FLAT) {
        return currentIntervalObj.getFTP_start();
    }
    else if(currentIntervalObj.getPowerStepType() == Interval::NONE) {
        return -1;
    }

    double totalSec = Util::convertQTimeToSecD(currentIntervalObj.getDurationQTime());
    /// y=ax+b
    double b = currentIntervalObj.getFTP_start();
    double a = (currentIntervalObj.getFTP_end() - b) / totalSec;
    double x = totalSec - (Util::convertQTimeToSecD(timeInterval));
    double y = a*x + b;

    return y;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
double WorkoutDialog::calculateNewTargetHr() {

    /// Intervale de base
    if (currentIntervalObj.getHRStepType() == Interval::FLAT) {
        return currentIntervalObj.getHR_start();
    }
    else if(currentIntervalObj.getHRStepType() == Interval::NONE) {
        return -1;
    }

    double totalSec = Util::convertQTimeToSecD(currentIntervalObj.getDurationQTime());
    /// y=ax+b
    double b = currentIntervalObj.getHR_start();
    double a = (currentIntervalObj.getHR_end() - b) / totalSec;
    double x = totalSec - (Util::convertQTimeToSecD(timeInterval));
    double y = a*x + b;

    return y;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
int WorkoutDialog::calculateNewTargetCadence() {

    /// Intervale de base
    if (currentIntervalObj.getCadenceStepType() == Interval::FLAT) {
        return currentIntervalObj.getCadence_start();
    }
    else if(currentIntervalObj.getCadenceStepType() == Interval::NONE) {
        return -1;
    }

    double totalSec = Util::convertQTimeToSecD(currentIntervalObj.getDurationQTime());
    /// y=ax+b
    double b = currentIntervalObj.getCadence_start();
    double a = (currentIntervalObj.getCadence_end() - b) / totalSec;
    double x = totalSec - (Util::convertQTimeToSecD(timeInterval));
    double y = a*x + b;

    return y;
}

////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showTimerOnTop(bool showOnTop) {

    qDebug() << "SHOW TIMER ON TOP?!" << showOnTop;

    ui->widget_topMenu->setTimersVisible(showOnTop);
    ui->widget_time->setVisible(!showOnTop);
}

void WorkoutDialog::showTimerIntervalRemaining(bool show) {

    ui->widget_time->showIntervalRemaining(show);
    ui->widget_topMenu->showIntervalRemaining(show);
}

void WorkoutDialog::showTimerWorkoutRemaining(bool show) {

    ui->widget_time->showWorkoutRemaining(show);
    ui->widget_topMenu->showWorkoutRemaining(show);
}

void WorkoutDialog::showTimerWorkoutElapsed(bool show) {

    ui->widget_time->showWorkoutElapsed(show);
    ui->widget_topMenu->showWorkoutElapsed(show);
}
void WorkoutDialog::setTimerFontSize(int value) {

    ui->widget_time->setTimerFontSize(value);
}


///0 = Minimalist
///1 = Detailed
///2 = Graph
///3 = Hide
////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showHeartRateDisplayWidget(int display) {

    if (account->show_hr_widget) {
        if (display == 0) { /// Minimalist
            ui->wid_1_minimalistHr->setVisible(true);
            ui->wid_1_workoutPlot_HeartrateZoom->setVisible(false);
            ui->wid_1_infoBoxHr->setVisible(false);
        }
        else if (display == 1) { /// Detailed
            ui->wid_1_minimalistHr->setVisible(false);
            ui->wid_1_workoutPlot_HeartrateZoom->setVisible(false);
            ui->wid_1_infoBoxHr->setVisible(true);
        }
        else if (display == 2) { /// Graph
            ui->wid_1_minimalistHr->setVisible(false);
            ui->wid_1_workoutPlot_HeartrateZoom->setVisible(true);
            ui->wid_1_infoBoxHr->setVisible(false);
        }
    }
    else { /// Hide
        ui->wid_1_minimalistHr->setVisible(false);
        ui->wid_1_workoutPlot_HeartrateZoom->setVisible(false);
        ui->wid_1_infoBoxHr->setVisible(false);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showPowerDisplayWidget(int display) {

    if (account->show_power_widget) {
        if (display == 0) { /// Minimalist
            ui->wid_2_minimalistPower->setVisible(true);
            ui->wid_2_workoutPlot_PowerZoom->setVisible(false);
            ui->wid_2_infoBoxPower->setVisible(false);
        }
        else if (display == 1) { /// Detailed
            ui->wid_2_minimalistPower->setVisible(false);
            ui->wid_2_workoutPlot_PowerZoom->setVisible(false);
            ui->wid_2_infoBoxPower->setVisible(true);
        }
        else if (display == 2) { /// Graph
            ui->wid_2_minimalistPower->setVisible(false);
            ui->wid_2_workoutPlot_PowerZoom->setVisible(true);
            ui->wid_2_infoBoxPower->setVisible(false);
        }
        else if (display == 3) { ///Graph and Detailed
            ui->wid_2_minimalistPower->setVisible(false);
            ui->wid_2_workoutPlot_PowerZoom->setVisible(true);
            ui->wid_2_infoBoxPower->setVisible(true);
        }
    }
    else { /// Hide
        ui->wid_2_minimalistPower->setVisible(false);
        ui->wid_2_workoutPlot_PowerZoom->setVisible(false);
        ui->wid_2_infoBoxPower->setVisible(false);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showCadenceDisplayWidget(int display) {

    if (account->show_cadence_widget) {
        if (display == 0) { /// Minimalist
            ui->wid_3_minimalistCadence->setVisible(true);
            ui->wid_3_workoutPlot_CadenceZoom->setVisible(false);
            ui->wid_3_infoBoxCadence->setVisible(false);
        }
        else if (display == 1) { /// Detailed
            ui->wid_3_minimalistCadence->setVisible(false);
            ui->wid_3_workoutPlot_CadenceZoom->setVisible(false);
            ui->wid_3_infoBoxCadence->setVisible(true);
        }
        else if (display == 2) { /// Graph
            ui->wid_3_minimalistCadence->setVisible(false);
            ui->wid_3_workoutPlot_CadenceZoom->setVisible(true);
            ui->wid_3_infoBoxCadence->setVisible(false);
        }
    }
    else { /// Hide
        ui->wid_3_minimalistCadence->setVisible(false);
        ui->wid_3_workoutPlot_CadenceZoom->setVisible(false);
        ui->wid_3_infoBoxCadence->setVisible(false);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showSpeedDisplayWidget() {

    if (account->show_speed_widget) {
        ui->wid_4_infoBoxSpeed->setVisible(true);
    }
    else { /// Hide
        ui->wid_4_infoBoxSpeed->setVisible(false);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::useVirtualSpeedData(bool useIt){

    //Hide Trainer Speed field if we are using this data
    if (useIt && account->show_trainer_speed) {
        ui->wid_4_infoBoxSpeed->setTrainerSpeedVisible(useIt);
    }
    else if (!useIt) {
        ui->wid_4_infoBoxSpeed->setTrainerSpeedVisible(useIt);
    }
    ui->wid_5_infoWorkout->setDistanceVisible(useIt);

}

////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showTrainerSpeed(bool show) {

    ui->wid_4_infoBoxSpeed->setTrainerSpeedVisible(show);
}

////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showCaloriesDisplayWidget() {

    if (account->show_calories_widget) {
        ui->wid_5_infoWorkout->setVisible(true);
    }
    else { /// Hide
        ui->wid_5_infoWorkout->setVisible(false);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showOxygenDisplayWidget() {

    if (account->show_oxygen_widget) {
        ui->wid_oxygen->setVisible(true);
    }
    else { /// Hide
        ui->wid_oxygen->setVisible(false);
    }
}



////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showPowerBalanceWidget(int display) {

    if (account->show_power_balance_widget) {
        if (display == 0) {  ///Always Show
            ui->wid_2_balancePower->setVisible(true);
        }
        else if (display == 1 && workout.getWorkoutNameEnum() != Workout::OPEN_RIDE) { /// Show with target
            /// Check current interval has power balance ?
            if (workout.getInterval(currentInterval).getRightPowerTarget() == -1)
                ui->wid_2_balancePower->setVisible(false);
            else
                ui->wid_2_balancePower->setVisible(true);
        }
        else if (display == 1 && workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {
            ui->wid_2_balancePower->setVisible(false);
        }
    }
    else {  /// Never Show
        ui->wid_2_balancePower->setVisible(false);
    }
    ui->wid_2_balancePower->setShowMode(display);
}


////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::setMessagePlot() {

    if (isWorkoutOver)
        return;


    bool showMessage = false; //only show if workout is paused or not started

    /// 0=Cadence 1=Power 2=Speed 3=Button
    int trigger = account->start_trigger;
    QString toShow;


    if (!isWorkoutStarted) {
        showMessage = true;

        if (account->enable_studio_mode) {
            toShow = tr("To start workout, press Enter or the start button");
        }
        else if (trigger == 0) {
            toShow = tr("To start workout, pedal over ");
            toShow += QString::number(account->value_cadence_start) + " ";
            toShow += tr("rpm");
        }
        else if (trigger == 1) {
            toShow = tr("To start workout, pedal over ");
            toShow += QString::number(account->value_power_start) + " ";
            toShow += tr("watts");
        }
        else if (trigger == 2) {
            toShow = tr("To start workout, pedal over ");
            toShow += QString::number(account->value_speed_start) + " ";
            if (account->distance_in_km)
                toShow += tr("km/h");
            else
                toShow += tr("mph");
        }
        else {
            toShow = tr("To start workout, press Enter or the start button");
        }
    }
    else if (isWorkoutPaused) {
        showMessage = true;

        if (account->enable_studio_mode) {
            toShow = tr("To resume workout, press Enter or the resume button");
        }
        else if (trigger == 0) {
            toShow = tr("To resume workout, pedal over ");
            toShow += QString::number(account->value_cadence_start) + " ";
            toShow += tr("rpm");
        }
        else if (trigger == 1) {
            toShow = tr("To resume workout, pedal over ");
            toShow += QString::number(account->value_power_start) + " ";
            toShow += tr("watts");
        }
        else if (trigger == 2) {
            toShow = tr("To resume workout, pedal over ");
            toShow += QString::number(account->value_speed_start) + " ";
            if (account->distance_in_km)
                toShow += tr("km/h");
            else
                toShow += tr("mph");
        }
        else {
            toShow = tr("To resume workout, press Enter or the resume button");
        }
    }
    if (showMessage) {
        ui->widget_workoutPlot->setMessage(toShow);
    }
}



////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::setWidgetsStopped(bool b) {

    qDebug() << "SetWidgetStopped!";

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            arrUserStudioWidget[i]->setStopped(b);
        }
    }
    else {
        ui->widget_workoutPlot->setStopped(b);
        ui->wid_2_workoutPlot_PowerZoom->setStopped(b);
        ui->wid_3_workoutPlot_CadenceZoom->setStopped(b);
        ui->wid_1_workoutPlot_HeartrateZoom->setStopped(b);
        ui->wid_1_infoBoxHr->setStopped(b);
        ui->wid_2_infoBoxPower->setStopped(b);
        ui->wid_3_infoBoxCadence->setStopped(b);
        ui->wid_4_infoBoxSpeed->setStopped(b);
        ui->wid_5_infoWorkout->setStopped(b);
        ui->wid_2_balancePower->setStopped(b);
        ui->wid_1_minimalistHr->setStopped(b);
        ui->wid_2_minimalistPower->setStopped(b);
        ui->wid_3_minimalistCadence->setStopped(b);
    }
}



////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::achievementReceived(Achievement achievement) {

    qDebug() << "\n\n\n-----------------WORKOUT DIALOG, RECVEIVED NEW ACHIEVEMENT  -------------!!!";
    qDebug() << "achievement name:" << achievement.getName();

    //// Add achievement to Queue (for in-workout animated overlay)
    queueAchievement.enqueue(achievement);
    qDebug() << "Queue size is at" << queueAchievement.size();

    // Also collect for the post-workout summary panel
    earnedAchievements.append(achievement);

    if (!achievementCurrentlyPlaying) {
        qDebug() << "SHOULD SHOW ACHIEVEMENT NOW!";
        animateAchievement();
    }
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::slotGetSensorListFinished() {


    qDebug() << "slotGetSensorListFinished!-------------------------\n-------------------------------\n";

    //success, process data
    if (replyGetListSensor->error() == QNetworkReply::NoError) {
        qDebug() << "no error slotGetSensorListFinished!";
    }
    // error, retry request
    else {
        if (numberFailGetListSensor > 3) {
            LOG_WARN("WorkoutDialog",
                     QStringLiteral("getSensorList: 3 retries exhausted — closing dialog: ")
                     + replyGetListSensor->errorString());
            QMessageBox msgBox(this);
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(tr("Could not retrieve sensors from our server<br/>"
                              "Please re-open the workout again."));
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setDefaultButton(QMessageBox::Ok);
            msgBox.exec();
            this->close();
            return;
        }
        else {
            numberFailGetListSensor++;
            LOG_WARN("WorkoutDialog",
                     QStringLiteral("getSensorList error (attempt ")
                     + QString::number(numberFailGetListSensor) + QStringLiteral("): ")
                     + replyGetListSensor->errorString());
            replyGetListSensor = SensorDAO::getActiveSensorList(account->id);
            connect(replyGetListSensor, SIGNAL(finished()), this, SLOT(slotGetSensorListFinished()) );
            return;
        }
    }


    // No error if we got here, process sensor data...
    QByteArray arrayData =  replyGetListSensor->readAll();

    QString replyMsg(arrayData);
    qDebug() << replyMsg;

    QList<Sensor> lstSensor =  Util::parseJsonSensorList(replyMsg);

    if (lstSensor.size() < 1) {
        labelPairHr->setText("");
        labelPairHr->fadeOut(500);
        return;
    }


    foreach (Sensor sensor, lstSensor) {
        /// HR
        if (sensor.getDeviceType() == constants::hrDeviceType) {
            sensorHr = sensor;
            usingHr = true;
        }
        /// SC
        else if (sensor.getDeviceType() == constants::speedCadDeviceType) {
            sensorSpeedCadence = sensor;
            usingSpeedCadence = true;
        }
        /// Cadence
        else if (sensor.getDeviceType() == constants::cadDeviceType) {
            sensorCadence = sensor;
            usingCadence = true;
        }
        /// Speed
        else if (sensor.getDeviceType() == constants::speedDeviceType) {
            sensorSpeed = sensor;
            usingSpeed = true;
        }
        /// Power
        else if (sensor.getDeviceType() == constants::powerDeviceType) {
            sensorPower = sensor;
            usingPower = true;
        }
        /// FE-C
        else if (sensor.getDeviceType() == constants::fecDeviceType ) {
            sensorFEC = sensor;
            usingFEC = true;
            idFecMainUser = sensor.getAntId();
        }
        /// Oxygen
        else if (sensor.getDeviceType() == constants::oxyDeviceType ) {
            sensorOxygen = sensor;
            usingOxygen = true;
        }
    }


    /// If not using specific sensor, dont check for pairing completion
    if (!usingHr)
        hrPairingDone = true;
    if (!usingSpeedCadence)
        scPairingDone = true;
    if (!usingCadence)
        cadencePairingDone = true;
    if (!usingSpeed)
        speedPairingDone = true;
    if (!usingPower)
        powerPairingDone = true;
    if (!usingFEC)
        fecPairingDone = true;
    if (!usingOxygen)
        oxygenPairingDone = true;


    if (usingPower)
        ui->widget_topMenu->setButtonCalibratePMVisible(true);
    if (usingFEC)
        ui->widget_topMenu->setButtonCalibrationFECVisible(true);



    //// --------------- Show pairing window --------
    QString pairingWithMsg = tr(" Pairing with %1 sensor (%2, %3)...");

    labelPairHr->setText(""); //Erase previous msg

    if (usingHr) {
        QString pairingMsgHr = pairingWithMsg.arg(Sensor::getName(Sensor::SENSOR_HR), QString::number(sensorHr.getAntId()) , sensorHr.getName());
        labelPairHr->setText(pairingMsgHr);
        labelPairHr->setVisible(true);
    }
    if (usingSpeedCadence) {
        QString pairingMsgSC = pairingWithMsg.arg(Sensor::getName(Sensor::SENSOR_SPEED_CADENCE), QString::number(sensorSpeedCadence.getAntId()) , sensorSpeedCadence.getName());
        labelSpeedCadence->setText(pairingMsgSC);
        labelSpeedCadence->setVisible(true);
    }
    if (usingCadence) {
        QString pairingMsgCadence = pairingWithMsg.arg(Sensor::getName(Sensor::SENSOR_CADENCE), QString::number(sensorCadence.getAntId()) , sensorCadence.getName());
        labelCadence->setText(pairingMsgCadence);
        labelCadence->setVisible(true);
    }
    if (usingSpeed) {
        QString pairingMsgSpeed = pairingWithMsg.arg(Sensor::getName(Sensor::SENSOR_SPEED), QString::number(sensorSpeed.getAntId()) , sensorSpeed.getName());
        labelSpeed->setText(pairingMsgSpeed);
        labelSpeed->setVisible(true);
    }
    if (usingFEC) {
        QString pairingMsgFEC = pairingWithMsg.arg(Sensor::getName(Sensor::SENSOR_FEC), QString::number(sensorFEC.getAntId()) , sensorFEC.getName());
        labelFEC->setText(pairingMsgFEC);
        labelFEC->setVisible(true);
    }
    if (usingOxygen) {
        QString pairingMsgOxy = pairingWithMsg.arg(Sensor::getName(Sensor::SENSOR_OXYGEN), QString::number(sensorOxygen.getAntId()) , sensorOxygen.getName());
        labelOxygen->setText(pairingMsgOxy);
        labelOxygen->setVisible(true);
    }
    if (usingPower) {
        QString pairingMsgPower = pairingWithMsg.arg(Sensor::getName(Sensor::SENSOR_POWER), QString::number(sensorPower.getAntId()) , sensorPower.getName());
        labelPower->setText(pairingMsgPower);
        labelPower->setVisible(true);
    }
    else if (account->powerCurve.getId() > 0 && (usingSpeedCadence || usingSpeed) ) {
        labelPower->setText(tr(" Using Trainer Power Curve:") + account->powerCurve.getFullName());
        labelPower->setStyleSheet("background-color : rgba(1,1,1,220); color : green;");
        labelPower->setVisible(true);
    }


    labelPairHr->fadeIn(500);
    labelSpeedCadence->fadeIn(500);
    labelCadence->fadeIn(500);
    labelSpeed->fadeIn(500);
    labelFEC->fadeIn(500);
    labelOxygen->fadeIn(500);
    labelPower->fadeIn(500);
    //////////////////////////


    qDebug() << "Ok start sensors now !";
    sendSoloData(account->powerCurve, account->wheel_circ, lstSensor, account->use_pm_for_cadence, account->use_pm_for_speed);



    qDebug() << "slotGetSensorListFinished";
    replyGetListSensor->deleteLater();
}
//---------------------------------------------------------------------------------
void WorkoutDialog::checkPairingCompleted() {

    if (hrPairingDone && scPairingDone && cadencePairingDone && speedPairingDone && powerPairingDone && oxygenPairingDone && fecPairingDone)
    {
        ui->widget_topMenu->setButtonStartReady(true);

        labelPairHr->fadeOutAfterPause(2000, 6000);
        labelSpeedCadence->fadeOutAfterPause(2000, 6000);
        labelCadence->fadeOutAfterPause(2000, 6000);;
        labelSpeed->fadeOutAfterPause(2000, 6000);
        labelPower->fadeOutAfterPause(2000, 6000);
        labelFEC->fadeOutAfterPause(2000, 6000);
        labelOxygen->fadeOutAfterPause(2000, 6000);

    }
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::slotPutAccountFinished() {

    qDebug() << "slotPutAccountFinished!-------------------------\n-----------------------------------\n";

    //success, process data
    if (replyPutAccountToCheckSessionExpired->error() == QNetworkReply::NoError) {
        qDebug() << "no error postDataAccountFinished!";
        if (account->enable_studio_mode) {
            labelPairHr->fadeOut(500);

            emit sendDataUserStudio(vecUserStudio);
        }
    }
    // error, retry request
    else {
        if (numberFailCheckSessionExpired > 3) {
            LOG_WARN("WorkoutDialog",
                     QStringLiteral("putAccount: 3 retries exhausted — closing dialog: ")
                     + replyPutAccountToCheckSessionExpired->errorString());
            QMessageBox msgBox(this);
            msgBox.setIcon(QMessageBox::Warning);
            msgBox.setText(tr("Could not retrieve your session data.<br/>"
                              "Please reconnect to MaximumTrainer."));
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.setDefaultButton(QMessageBox::Ok);
            msgBox.exec();
            this->close();
            return;
        }
        else {
            numberFailCheckSessionExpired++;
            LOG_WARN("WorkoutDialog",
                     QStringLiteral("putAccount error (attempt ")
                     + QString::number(numberFailCheckSessionExpired) + QStringLiteral("): ")
                     + replyPutAccountToCheckSessionExpired->errorString());
            replyPutAccountToCheckSessionExpired = UserDAO::putAccount(account);
            connect(replyPutAccountToCheckSessionExpired, SIGNAL(finished()), this, SLOT(slotPutAccountFinished()) );
            return;
        }
    }

    // No error if we got here, process data...
    QByteArray arrayData =  replyPutAccountToCheckSessionExpired->readAll();

    QString replyMsg(arrayData);
    qDebug() << replyMsg;

    // Error message returned?
    // 555= Session ID and ID not present in DB (another user logged with this username, sessionID no longer good)
    // 666= Session_MT_expired < today (user kept program open more than 1 day)
    if (replyMsg.contains("##555") || replyMsg.contains("##666")) {
        qDebug() << "SESSION_ID AND ID NOT PRESENT IN DB!, kick out!";
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(tr("Your session has expired.<br/>"
                          "Please reconnect to MaximumTrainer."));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
        this->close();
        return;
    }
    // User is legit, retrieve User data
    else {
        if (!account->enable_studio_mode) {
            numberFailGetListSensor = 0;
            labelPairHr->setText(tr(" Retrieving Sensors..."));
            replyGetListSensor = SensorDAO::getActiveSensorList(account->id);
            connect(replyGetListSensor, SIGNAL(finished()), this, SLOT(slotGetSensorListFinished()) );
        }
    }

    replyPutAccountToCheckSessionExpired->deleteLater();
}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::closeWindow() {

    sureYouWantToQuit();
}

void WorkoutDialog::minimizeWindow() {

    this->showMinimized();
}


//////////////////////////////////////////////////////
void WorkoutDialog::expandWindow() {


    if (this->isFullScreen()) {
        qDebug() << "going call showNormalWin ";
        showNormalWin();
    }
    else {
        qDebug() << "going call showFullScreenWin ";
        showFullScreenWin();
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space:
        if (!event->isAutoRepeat())
            start_or_pause_workout();
        return;
    case Qt::Key_Right:
        if (!event->isAutoRepeat() &&
            isWorkoutStarted && !isWorkoutPaused && !isWorkoutOver && !isTestWorkout &&
            currentInterval + 1 < workout.getLstInterval().size())
            moveToNextInterval();
        return;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        adjustWorkoutDifficulty(currentWorkoutDifficultyPercentage + 5);
        return;
    case Qt::Key_Minus:
        adjustWorkoutDifficulty(currentWorkoutDifficultyPercentage - 5);
        return;
    case Qt::Key_L:
        if (!event->isAutoRepeat())
            lapButtonPressed();
        return;
    case Qt::Key_Escape:
        if (!event->isAutoRepeat())
            reject();
        return;
    case Qt::Key_Question:
    case Qt::Key_F1:
        if (!event->isAutoRepeat()) {
            DialogKeyboardShortcuts dlg(this);
            dlg.exec();
        }
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        // Enter starts/pauses the workout (matching the eventFilter and the
        // "press Enter or the start button" prompt). Handle it here too and do
        // NOT fall through to QDialog::keyPressEvent(), which would click the
        // dialog's default button and accept()/close the workout — triggered
        // unexpectedly when the embedded web video player (QWebEngineView) has
        // focus and forwards a Return key back to the dialog.
        if (!event->isAutoRepeat())
            start_or_pause_workout();
        return;
    default:
        break;
    }
    QDialog::keyPressEvent(event);
}

//////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::reject() {

    sureYouWantToQuit();
}


////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showConfig() {
    emit insideConfig(true);
    dconfig->show();
    dconfig->raise();
    dconfig->activateWindow();
    // insideConfig(false) is emitted via the QDialog::finished connection set up in setupUi
}


//------------------------------------------------------------------------------------------------------
void WorkoutDialog::sureYouWantToQuit() {

    saveInterface();

    if (!isWorkoutPaused) {
        start_or_pause_workout();
    }

    if (isWorkoutStarted && !isWorkoutOver) {

        isAskingUserQuestion = true;
        QMessageBox msgBox(this);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setText(tr("Workout is not completed."));
        msgBox.setInformativeText(tr("Save your progress?"));
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);

        int reply = msgBox.exec();
        if (reply == QMessageBox::Yes) {

            // Save FIT FILE
            if (Util::getSystemPathHistory() != "invalid_writable_path") {
                int intervalPausedTime_msec = totalTimePausedWorkout_msec - lastIntervalTotalTimePausedWorkout_msec;
                changeIntervalsDataWorkout(lastIntervalEndTime_msec, timeElapsed_sec, intervalPausedTime_msec, true, false);
                m_quitAfterSave = true;  // skip post-workout panel — dialog closes immediately after
                closeFitFiles(timeElapsed_sec);
            }

            /// Save data DB (update Achievements, stats etc.)
            if (!account->isOffline)
                UserDAO::putAccount(account);
            QDialog::accept();
        }
        else if (reply == QMessageBox::No) {
            closeAndDeleteFitFile();
            QDialog::accept();
        }
        else {
            qDebug() << "Cancel was clicked";
        }
        isAskingUserQuestion = false;
    }
    /// Not started or workout completed, no warning to show
    else {
        QDialog::accept();
    }

}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::loadInterface() {

    QSettings settings;

    settings.beginGroup("WorkoutDialog");

    resize(settings.value("size", QSize(1350, 800)).toSize());
    move(settings.value("pos", QPoint(100, 100)).toPoint());

    QList<int> mySize;
    mySize.append( settings.value("splitter0", 2).toInt() );
    mySize.append( settings.value("splitter1", 35).toInt() );
    mySize.append( settings.value("splitter2", 39).toInt() );
    mySize.append( settings.value("splitter3", 24).toInt() );
    ui->splitter->setSizes(mySize);


    bool wasFullscreen = settings.value("fullscreen", false).toBool();
    settings.endGroup();

    if (wasFullscreen) {
        wasFullscreenOnOpen = true;
        showFullScreenWin();
    }
    else {
        wasFullscreenOnOpen = false;
        isFullScreenFlag = false;
        this->setSizeGripEnabled(true);
        //        ui->widget_topMenu->setMinExpandExitVisible(false);
        ui->widget_topMenu->updateExpandIcon();
    }

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::saveInterface() {

    QSettings settings;

    settings.beginGroup("WorkoutDialog");
    settings.setValue("size", size());
    settings.setValue("pos", pos());

    QList<int> sizeNow = ui->splitter->sizes();
    settings.setValue("splitter0", sizeNow.at(0));
    settings.setValue("splitter1", sizeNow.at(1));
    settings.setValue("splitter2", sizeNow.at(2));
    settings.setValue("splitter3", sizeNow.at(3));

    if (this->isFullScreen()) {
        settings.setValue("fullscreen", true);
    }
    else {
        settings.setValue("fullscreen", false);
    }

    settings.endGroup();
}


//------------------------------------------------------------
void WorkoutDialog::ignoreClickPlot() {
    bignoreClickPlot = false;
    timerIgnoreClickPlot->stop();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::activateSoundBool() {
    soundsActive = true;
    timerCheckToActivateSound->stop();
}
void WorkoutDialog::activateSoundPowerTooLow() {
    soundPowerTooLowActive = true;
    timerCheckReactivateSoundPowerTooLow->stop();
}
void WorkoutDialog::activateSoundPowerTooHigh() {
    soundPowerTooHighActive = true;
    timerCheckReactivateSoundPowerTooHigh->stop();
}
void WorkoutDialog::activateSoundCadenceTooLow() {
    soundCadenceTooLowActive = true;
    timerCheckReactivateSoundCadenceTooLow->stop();
}
void WorkoutDialog::activateSoundCadenceTooHigh() {
    soundCadenceTooHighActive = true;
    timerCheckReactivateSoundCadenceTooHigh->stop();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::checkFitFileCreated() {

    qDebug() << "****checkFitFileCreated";
    QString fitFilename = arrDataWorkout[0]->getFitFilename();
    qDebug() << "FIT File name is" << fitFilename << "name:"<< workout.getName() << "desc" << workout.getDescription();
    if (fitFilename != "") {
        fitFilePath        = fitFilename;
        fitFileName        = workout.getName();
        fitFileDescription = workout.getDescription();
        if (!m_quitAfterSave)
            showPostWorkoutPanel();
        emit fitFileReady(fitFilename, workout.getName(), workout.getDescription());
#ifdef Q_OS_WASM
        // The WASM virtual filesystem is not accessible to the user.
        // Offer the completed FIT file via the browser's native Save dialog.
        QFile fitFile(fitFilename);
        if (fitFile.open(QIODevice::ReadOnly)) {
            QFileDialog::saveFileContent(fitFile.readAll(),
                                         QFileInfo(fitFilename).fileName());
            fitFile.close();
        }
#endif
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showPostWorkoutPanel()
{
    if (widgetPostWorkout) return;  // already shown

    widgetPostWorkout = new QWidget(ui->widget_allSpeedo);
    widgetPostWorkout->setObjectName("widgetPostWorkout");
    widgetPostWorkout->setStyleSheet(
        "QWidget#widgetPostWorkout { background-color: rgba(10,10,10,220); border-radius: 10px; }"
        "QLabel  { color: white; }"
        "QPushButton { background-color: #2a6099; color: white; border-radius: 4px; padding: 6px 12px; }"
        "QPushButton:disabled { background-color: #1a3a59; color: #888; }");

    auto *layout = new QVBoxLayout(widgetPostWorkout);
    layout->setSpacing(6);
    layout->setContentsMargins(16, 12, 16, 12);

    auto *titleLabel = new QLabel(tr("Workout Complete!"), widgetPostWorkout);
    titleLabel->setStyleSheet("font-size: 14pt; font-weight: bold; color: #80c0ff;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // — Achievements section —
    if (!earnedAchievements.isEmpty()) {
        auto *achHeader = new QLabel(tr("Achievements Earned:"), widgetPostWorkout);
        achHeader->setStyleSheet("font-size: 10pt; font-weight: bold; color: #f0c040; margin-top: 6px;");
        layout->addWidget(achHeader);
        for (const Achievement &a : earnedAchievements) {
            auto *row = new QLabel(QString("🏆  %1").arg(a.getName()), widgetPostWorkout);
            row->setStyleSheet("font-size: 10pt;");
            layout->addWidget(row);
        }
    }

    // — Upload section —
    const bool hasStrava   = !account->strava_access_token.isEmpty();
    const bool hasTP       = !account->training_peaks_access_token.isEmpty()
                          && !account->training_peaks_refresh_token.isEmpty();
    const bool hasSL       = !account->selfloops_user.isEmpty()
                          && !account->selfloops_pw.isEmpty();
    const bool hasIcu      = !account->intervals_icu_athlete_id.isEmpty() &&
                          (!account->intervals_icu_api_key.isEmpty() ||
                           !account->intervals_icu_access_token.isEmpty());

    if (hasStrava || hasTP || hasSL || hasIcu) {
        auto *upHeader = new QLabel(tr("Upload Activity:"), widgetPostWorkout);
        upHeader->setStyleSheet("font-size: 10pt; font-weight: bold; color: #80c0ff; margin-top: 8px;");
        layout->addWidget(upHeader);

        if (hasStrava) {
            auto *btn = new QPushButton(tr("Upload to Strava"), widgetPostWorkout);
            btn->setObjectName("btnStrava");
            connect(btn, &QPushButton::clicked, this, &WorkoutDialog::uploadToStrava);
            layout->addWidget(btn);
        }
        if (hasTP) {
            auto *btn = new QPushButton(tr("Upload to TrainingPeaks"), widgetPostWorkout);
            btn->setObjectName("btnTrainingPeaks");
            connect(btn, &QPushButton::clicked, this, &WorkoutDialog::uploadToTrainingPeaks);
            layout->addWidget(btn);
        }
        if (hasSL) {
            auto *btn = new QPushButton(tr("Upload to SelfLoops"), widgetPostWorkout);
            btn->setObjectName("btnSelfLoops");
            connect(btn, &QPushButton::clicked, this, &WorkoutDialog::uploadToSelfLoops);
            layout->addWidget(btn);
        }
        if (hasIcu) {
            auto *btn = new QPushButton(tr("Upload to Intervals.icu"), widgetPostWorkout);
            btn->setObjectName("btnIntervalsIcu");
            connect(btn, &QPushButton::clicked, this, &WorkoutDialog::uploadToIntervalsIcu);
            layout->addWidget(btn);
        }
    }

    widgetPostWorkout->adjustSize();
    const QSize ps = ui->widget_allSpeedo->size();
    const QSize ws = widgetPostWorkout->size();
    widgetPostWorkout->move((ps.width()  - ws.width())  / 2,
                            (ps.height() - ws.height()) / 2);
    widgetPostWorkout->show();
    widgetPostWorkout->raise();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::uploadToStrava()
{
    if (fitFilePath.isEmpty()) return;
    auto *btn = widgetPostWorkout ? widgetPostWorkout->findChild<QPushButton*>("btnStrava") : nullptr;

    StravaService svc;
    svc.setAccessToken(account->strava_access_token);
    replyPostStravaUpload = svc.uploadActivity(fitFilePath, fitFileName, fitFileDescription,
                                               account->strava_private_upload, true);
    if (!replyPostStravaUpload) {
        if (btn) { btn->setText(tr("Upload to Strava (Failed — Retry)")); }
        return;
    }
    if (btn) btn->setEnabled(false);
    connect(replyPostStravaUpload, &QNetworkReply::finished,
            this, &WorkoutDialog::slotPostStravaUploadDone);
}

void WorkoutDialog::slotPostStravaUploadDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostStravaUpload = nullptr;

    auto *btn = widgetPostWorkout ? widgetPostWorkout->findChild<QPushButton*>("btnStrava") : nullptr;
    if (reply->error() != QNetworkReply::NoError) {
        LOG_WARN("WorkoutDialog", QStringLiteral("Strava upload failed: ") + reply->errorString());
        if (btn) { btn->setEnabled(true); btn->setText(tr("Upload to Strava (Failed — Retry)")); }
        return;
    }

    const QByteArray body = reply->readAll();
    stravaUploadID_post = Util::parseIdJsonStravaUploadObject(QString(body));
    qDebug() << "Strava upload id:" << stravaUploadID_post;

    if (stravaUploadID_post <= 0) {
        if (btn) { btn->setEnabled(true); btn->setText(tr("Upload to Strava (Failed — Retry)")); }
        return;
    }

    if (btn) btn->setText(tr("Uploading to Strava…"));
    timerPostStravaStatus = new QTimer(this);
    connect(timerPostStravaStatus, &QTimer::timeout, this, &WorkoutDialog::slotPostStravaCheckStatus);
    timerPostStravaStatus->start(3000);
}

void WorkoutDialog::slotPostStravaCheckStatus()
{
    replyPostStravaStatus = ExtRequest::stravaCheckUploadStatus(account->strava_access_token, stravaUploadID_post);
    if (replyPostStravaStatus)
        connect(replyPostStravaStatus, &QNetworkReply::finished,
                this, &WorkoutDialog::slotPostStravaStatusDone);
}

void WorkoutDialog::slotPostStravaStatusDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostStravaStatus = nullptr;

    auto *btn = widgetPostWorkout ? widgetPostWorkout->findChild<QPushButton*>("btnStrava") : nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        if (timerPostStravaStatus) timerPostStravaStatus->stop();
        LOG_WARN("WorkoutDialog", QStringLiteral("Strava status check failed: ") + reply->errorString());
        if (btn) { btn->setEnabled(true); btn->setText(tr("Upload to Strava (Failed — Retry)")); }
        return;
    }

    const int code = Util::parseStravaUploadStatus(QString(reply->readAll()));
    // 0 = completed, 1 = in progress, 2 = error, -1 = unexpected
    if (code == 1) return;  // keep timer running

    if (timerPostStravaStatus) { timerPostStravaStatus->stop(); timerPostStravaStatus->deleteLater(); timerPostStravaStatus = nullptr; }

    if (code == 0) {
        if (btn) btn->setText(tr("✓ Uploaded to Strava"));
        LOG_INFO("WorkoutDialog", "Strava upload succeeded");
    } else {
        if (btn) { btn->setEnabled(true); btn->setText(tr("Upload to Strava (Failed — Retry)")); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::uploadToTrainingPeaks()
{
    if (fitFilePath.isEmpty()) return;
    auto *btn = widgetPostWorkout ? widgetPostWorkout->findChild<QPushButton*>("btnTrainingPeaks") : nullptr;

    // Refresh token first, then upload in the finished slot.
    replyPostTPRefresh = ExtRequest::trainingPeaksRefreshToken(account->training_peaks_access_token,
                                                               account->training_peaks_refresh_token);
    if (!replyPostTPRefresh) {
        if (btn) { btn->setText(tr("Upload to TrainingPeaks (Failed — Retry)")); }
        return;
    }
    if (btn) btn->setEnabled(false);
    connect(replyPostTPRefresh, &QNetworkReply::finished,
            this, &WorkoutDialog::slotPostTPRefreshDone);
}

void WorkoutDialog::slotPostTPRefreshDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostTPRefresh = nullptr;

    auto *btn = widgetPostWorkout ? widgetPostWorkout->findChild<QPushButton*>("btnTrainingPeaks") : nullptr;
    if (reply->error() != QNetworkReply::NoError) {
        LOG_WARN("WorkoutDialog", QStringLiteral("TP token refresh failed: ") + reply->errorString());
        if (btn) { btn->setEnabled(true); btn->setText(tr("Upload to TrainingPeaks (Failed — Retry)")); }
        return;
    }

    Util::parseJsonTPObject(QString::fromUtf8(reply->readAll()));

    replyPostTPUpload = ExtRequest::trainingPeaksUploadFile(account->training_peaks_access_token,
                                                            account->training_peaks_public_upload,
                                                            fitFileName, fitFileDescription, fitFilePath);
    if (!replyPostTPUpload) {
        if (btn) { btn->setEnabled(true); btn->setText(tr("Upload to TrainingPeaks (Failed — Retry)")); }
        return;
    }
    connect(replyPostTPUpload, &QNetworkReply::finished,
            this, &WorkoutDialog::slotPostTPUploadDone);
}

void WorkoutDialog::slotPostTPUploadDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostTPUpload = nullptr;

    auto *btn = widgetPostWorkout ? widgetPostWorkout->findChild<QPushButton*>("btnTrainingPeaks") : nullptr;
    if (reply->error() == QNetworkReply::NoError) {
        if (btn) btn->setText(tr("✓ Uploaded to TrainingPeaks"));
        LOG_INFO("WorkoutDialog", "TrainingPeaks upload succeeded");
    } else {
        LOG_WARN("WorkoutDialog", QStringLiteral("TP upload failed: ") + reply->errorString());
        if (btn) { btn->setEnabled(true); btn->setText(tr("Upload to TrainingPeaks (Failed — Retry)")); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::uploadToSelfLoops()
{
    if (fitFilePath.isEmpty()) return;
    auto *btn = widgetPostWorkout ? widgetPostWorkout->findChild<QPushButton*>("btnSelfLoops") : nullptr;

    replyPostSelfloopsUpload = ExtRequest::selfloopsUploadFile(account->selfloops_user,
                                                               account->selfloops_pw,
                                                               fitFilePath, fitFileDescription);
    if (!replyPostSelfloopsUpload) {
        if (btn) { btn->setText(tr("Upload to SelfLoops (Failed — Retry)")); }
        return;
    }
    if (btn) btn->setEnabled(false);
    connect(replyPostSelfloopsUpload, &QNetworkReply::finished,
            this, &WorkoutDialog::slotPostSelfloopsUploadDone);
}

void WorkoutDialog::slotPostSelfloopsUploadDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostSelfloopsUpload = nullptr;

    auto *btn = widgetPostWorkout ? widgetPostWorkout->findChild<QPushButton*>("btnSelfLoops") : nullptr;
    if (reply->error() != QNetworkReply::NoError) {
        LOG_WARN("WorkoutDialog", QStringLiteral("SelfLoops upload failed: ") + reply->errorString());
        if (btn) { btn->setEnabled(true); btn->setText(tr("Upload to SelfLoops (Failed — Retry)")); }
        return;
    }

    const QString msgReply = QString::fromUtf8(reply->readAll());
    if (msgReply.contains("Success", Qt::CaseInsensitive)) {
        if (btn) btn->setText(tr("✓ Uploaded to SelfLoops"));
        LOG_INFO("WorkoutDialog", "SelfLoops upload succeeded");
    } else if (msgReply.contains("already", Qt::CaseInsensitive)) {
        if (btn) btn->setText(tr("✓ Activity already on SelfLoops"));
    } else if (msgReply.contains("empty", Qt::CaseInsensitive)) {
        if (btn) { btn->setEnabled(true); btn->setText(tr("SelfLoops: activity is empty")); }
    } else {
        LOG_WARN("WorkoutDialog", QStringLiteral("SelfLoops unexpected response: ") + msgReply);
        if (btn) { btn->setEnabled(true); btn->setText(tr("Upload to SelfLoops (Failed — Retry)")); }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::uploadToIntervalsIcu()
{
    if (fitFilePath.isEmpty()) return;
    auto *btn = widgetPostWorkout ? widgetPostWorkout->findChild<QPushButton*>("btnIntervalsIcu") : nullptr;

    auto *svc = new IntervalsIcuService(this);
    svc->setCredentials(account->intervals_icu_api_key, account->intervals_icu_athlete_id);
    svc->setAccessToken(account->intervals_icu_access_token);
    const QString externalId = QFileInfo(fitFilePath).baseName();
    replyPostIntervalsIcuUpload = svc->uploadActivity(fitFilePath, fitFileName, externalId);
    svc->deleteLater();

    if (!replyPostIntervalsIcuUpload) {
        if (btn) btn->setText(tr("Upload to Intervals.icu (Failed — Retry)"));
        return;
    }
    if (btn) { btn->setEnabled(false); btn->setText(tr("Uploading…")); }
    connect(replyPostIntervalsIcuUpload, &QNetworkReply::finished,
            this, &WorkoutDialog::slotPostIntervalsIcuUploadDone);
}

void WorkoutDialog::slotPostIntervalsIcuUploadDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostIntervalsIcuUpload = nullptr;

    auto *btn = widgetPostWorkout ? widgetPostWorkout->findChild<QPushButton*>("btnIntervalsIcu") : nullptr;
    if (reply->error() != QNetworkReply::NoError) {
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 409) {
            if (btn) btn->setText(tr("✓ Activity already on Intervals.icu"));
        } else {
            LOG_WARN("WorkoutDialog", QStringLiteral("Intervals.icu upload failed: ") + reply->errorString());
            if (btn) { btn->setEnabled(true); btn->setText(tr("Upload to Intervals.icu (Failed — Retry)")); }
        }
        return;
    }
    if (btn) btn->setText(tr("✓ Uploaded to Intervals.icu"));
    LOG_INFO("WorkoutDialog", "Intervals.icu upload succeeded");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::closeFitFiles(double timeElapSec) {


    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            arrDataWorkout[i]->writeEndFile(timeElapSec);
            arrDataWorkout[i]->closeFitFile();
        }
    }
    else {
        arrDataWorkout[0]->writeEndFile(timeElapSec);
        arrDataWorkout[0]->closeFitFile();
        checkFitFileCreated();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::closeAndDeleteFitFile() {

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            arrDataWorkout[i]->closeAndDeleteFitFile();
        }
    }
    else {
        arrDataWorkout[0]->closeAndDeleteFitFile();
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::changeIntervalsDataWorkout(double timeStarted, double timeNow, int timePaused_msec, bool workoutOver, bool testInterval) {

    qDebug () << "changeIntervalsDataWorkout! - timeStarted" << timeStarted << "timeNow" << timeNow << "timePaused_msec" << timePaused_msec << "workoutOver" << workoutOver << "testInterval" << testInterval;

    // Capture interval summary stats BEFORE changeInterval() resets them.
    double summaryAvgPower = 0.0, summaryAvgHr = 0.0, summaryAvgCad = 0.0;
    double intervalDuration = timeNow - timeStarted;
    double targetPowerFraction = currentIntervalObj.getFTP_start();
    bool showSummary = !account->enable_studio_mode
                       && !workoutOver
                       && account->interval_summary_enabled
                       && intervalDuration >= 10.0;
    if (showSummary) {
        summaryAvgPower = arrDataWorkout[0]->getAvgIntervalPower();
        summaryAvgHr    = arrDataWorkout[0]->getAvgIntervalHr();
        summaryAvgCad   = arrDataWorkout[0]->getAvgIntervalCad();
    }

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            //            qDebug() << "change interval for this user" << i;
            arrDataWorkout[i]->changeInterval(timeStarted, timeNow, timePaused_msec, workoutOver, testInterval);
        }
    }
    else {
        qDebug() << "OK change interval, timePaused_msec: " << timePaused_msec;
        arrDataWorkout[0]->changeInterval(timeStarted, timeNow, timePaused_msec, workoutOver, testInterval);
    }

    if (showSummary)
        showIntervalSummaryOverlay(summaryAvgPower, summaryAvgHr, summaryAvgCad,
                                   static_cast<int>(intervalDuration), targetPowerFraction);

    lastIntervalEndTime_msec = timeElapsed_sec;
    lastIntervalTotalTimePausedWorkout_msec = totalTimePausedWorkout_msec;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::initDataWorkout() {

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            qDebug() << "Create DataWorkout for this user" << i;
            arrDataWorkout[i] = new DataWorkout(this->workout, account->FTP, this);
        }
    }
    else {
        arrDataWorkout[0] = new DataWorkout(this->workout, account->FTP, this);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showIntervalSummaryOverlay(double avgPower, double avgHr, double avgCad,
                                               int durationSec, double targetPowerFraction)
{
    const double targetWatts = targetPowerFraction * account->FTP;
    const auto adherence = IntervalSummaryUtil::classifyPowerAdherence(avgPower, targetWatts);

    QString colorHex;
    switch (adherence) {
    case IntervalSummaryUtil::PowerAdherence::Met:      colorHex = "#4CAF50"; break; // green
    case IntervalSummaryUtil::PowerAdherence::NearMiss: colorHex = "#FF9800"; break; // amber
    default:                                            colorHex = "#F44336"; break; // red
    }

    const int mins = durationSec / 60;
    const int secs = durationSec % 60;
    const QString duration = (mins > 0)
        ? QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0'))
        : QString("%1s").arg(secs);

    const int powerPct = (account->FTP > 0)
        ? qRound(avgPower * 100.0 / account->FTP)
        : 0;

    // Build HTML summary shown in the existing interval-message overlay
    QString html = QString(
        "<div style='text-align:center; line-height:1.5;'>"
        "<b>%1</b><br>"
        "<span style='color:%2;font-size:1.2em;'>&#9679; %3 W (%4%% FTP)</span><br>"
        "<span>HR: %5 bpm &nbsp;&nbsp; Cad: %6 rpm</span><br>"
        "<span style='color:#aaa;font-size:0.85em;'>%7</span>"
        "</div>")
        .arg(tr("Interval Complete"))
        .arg(colorHex)
        .arg(qRound(avgPower))
        .arg(powerPct)
        .arg(qRound(avgHr))
        .arg(qRound(avgCad))
        .arg(duration);

    ui->widget_workoutPlot->setDisplayIntervalMessage(
        true, html, account->interval_summary_duration_s);
}





/////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::createUserStudioWidget() {


    int widthWidgetStudio = 384; //384 and less = 5 widgets per row with HD res (1920 Width)
    int heightWidgetStudio = 190;


    // Get Main Screen Resolution
    int widthCurrentComputer = QGuiApplication::primaryScreen()->availableGeometry().width();
    qDebug() << "WHAT IS THE WIDTH??" << widthCurrentComputer;


    // Check how many Boxes we can put on a row
    int numberOfWidgetsPerRow = (widthCurrentComputer)/widthWidgetStudio;
    qDebug() << "we can have "<< numberOfWidgetsPerRow << " on this display..";
    int currentRowInserting = 0;
    int currentColInserting = 0;


    qDebug() <<  "createUserStudioWidget";
    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            qDebug() << "Create WidgeT!" << i;

            if (i+1 > (numberOfWidgetsPerRow*currentRowInserting)) {
                currentRowInserting++;
                currentColInserting = 0;
            }

            UserStudio myUserStudio = vecUserStudio.at(i);
            arrUserStudioWidget[i] = new UserStudioWidget(i+1, myUserStudio.getDisplayName(), myUserStudio.getFTP(), myUserStudio.getLTHR(), this);
            arrUserStudioWidget[i]->setMinimumSize(widthWidgetStudio, heightWidgetStudio);
            arrUserStudioWidget[i]->setMaximumSize(widthWidgetStudio, heightWidgetStudio);
            arrUserStudioWidget[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

            if (myUserStudio.getHrID() < 1 && myUserStudio.getFecID() < 1) {
                arrUserStudioWidget[i]->setHrSectionHidden();
            }
            if (myUserStudio.getPowerID() < 1 && myUserStudio.getPowerCurve().getId() < 1 && myUserStudio.getFecID() < 1) {
                arrUserStudioWidget[i]->setPowerSectionHidden();
            }

            // insert Widget
            QGridLayout *glayout = static_cast<QGridLayout*>( ui->widget_allSpeedo->layout()  );
            glayout->addWidget(arrUserStudioWidget[i], currentRowInserting, currentColInserting, 1, 1, Qt::AlignCenter);
            currentColInserting++;

        }
        //Set back theses widgets on top of GridStudio
        widgetLoading->raise();
        widgetLoading->activateWindow();

        widgetBattery->raise();
        widgetBattery->activateWindow();

    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showTestResult() {




    // ------------- FTP TEST -----------------
    if (workout.getWorkoutNameEnum() == Workout::FTP_TEST || workout.getWorkoutNameEnum() == Workout::FTP8min_TEST) {

        // GROUP
        if (account->enable_studio_mode) {

            for (int i=0; i<account->nb_user_studio; i++) {
                QString testResult = workout.getName() + tr(" Result:");
                int newFTP = arrDataWorkout[i]->getFTP();
                int newLTHR = arrDataWorkout[i]->getLTHR();
                arrUserStudioWidget[i]->setResultTest(testResult + QString::number(newFTP));

                UserStudio myUserStudio = vecUserStudio.at(i);
                if (newFTP > 0) { myUserStudio.setFTP(newFTP); }
                if (newLTHR >0) { myUserStudio.setLTHR(newLTHR); }
                vecUserStudio.replace(i, myUserStudio);
                emit ftpUserStudioChanged(vecUserStudio);
            }
        }
        //SOLO
        else {
            int newFTP = arrDataWorkout[0]->getFTP();
            int newLTHR = arrDataWorkout[0]->getLTHR();
            mainPlot->setAlertMessage(true, false, workout.getName() + tr(" Result")
                                      + "<div style='color:white;height:7px;'>------------------------------------</div><br/> "
                                      + tr("FTP: ") + QString::number(newFTP) + tr(" watts") + tr(" (Previous: ") +  QString::number(account->FTP) + tr(" watts)") + "<br/>"
                                      + tr("LTHR: ")  + QString::number(newLTHR) + tr(" bpm") + tr(" (Previous: ") +  QString::number(account->LTHR) + tr(" bpm)"), 500);
            if (newFTP > 0) { account->FTP = newFTP; }
            if (newLTHR > 0) { account->LTHR = newLTHR; }
            emit ftp_lthr_changed();
        }
    }

    // ------------- CP TEST -----------------
    else if (workout.getWorkoutNameEnum() ==  Workout::CP5min_TEST || workout.getWorkoutNameEnum() ==  Workout::CP20min_TEST) {

        // GROUP
        if (account->enable_studio_mode) {

            for (int i=0; i<account->nb_user_studio; i++) {
                QString testResult = workout.getName() + tr(" Result:");
                arrUserStudioWidget[i]->setResultTest(testResult + QString::number(arrDataWorkout[i]->getCP()));
            }
        }
        // SOLO
        else {
            int criticalPower = arrDataWorkout[0]->getCP();
            mainPlot->setAlertMessage(true, false, workout.getName() + tr(" Result")
                                      + "<div style='color:white;height:7px;'>------------------</div><br/> "
                                      + QString::number(criticalPower) + tr(" watts"), 500);
        }
    }
    else {
        mainPlot->setAlertMessage(true, false, workout.getName() + tr(" completed!")
                                  + "<div style='color:white;height:7px;'>------------------------------------</div><br/> "
                                  + tr("Nice work! "), 500);
    }




}


/////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::sendUserInfoToClock() {


    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            //            UserStudio myUserStudio = vecUserStudio.at(i);
            //            double cda = 30;
            //            double weight = 80;
            emit sendUserInfo(i+1, account->userCda, account->bike_weight_kg + account->weight_kg, account->nb_user_studio);
        }
    }
    else {
        emit sendUserInfo(1, account->userCda, account->bike_weight_kg + account->weight_kg, 1);
    }
}




/////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::connectDataWorkout() {

    qDebug() <<  "connectDataWorkout";

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            connect(arrDataWorkout[i], SIGNAL(maxPowerIntervalChanged(double)), arrUserStudioWidget[i], SLOT(setMaxIntervalPower(double)) );
            connect(arrDataWorkout[i], SIGNAL(maxPowerWorkoutChanged(double)), arrUserStudioWidget[i], SLOT(setMaxWorkoutPower(double)) );
            connect(arrDataWorkout[i], SIGNAL(avgPowerIntervalChanged(double)), arrUserStudioWidget[i], SLOT(setAvgIntervalPower(double)) );
            connect(arrDataWorkout[i], SIGNAL(avgPowerWorkoutChanged(double)), arrUserStudioWidget[i], SLOT(setAvgWorkoutPower(double)) );

            connect(arrDataWorkout[i], SIGNAL(maxCadenceIntervalChanged(double)), arrUserStudioWidget[i], SLOT(setMaxIntervalCad(double)) );
            connect(arrDataWorkout[i], SIGNAL(maxCadenceWorkoutChanged(double)), arrUserStudioWidget[i], SLOT(setMaxWorkoutCad(double)) );
            connect(arrDataWorkout[i], SIGNAL(avgCadenceIntervalChanged(double)), arrUserStudioWidget[i], SLOT(setAvgIntervalCad(double)) );
            connect(arrDataWorkout[i], SIGNAL(avgCadenceWorkoutChanged(double)), arrUserStudioWidget[i], SLOT(setAvgWorkoutCad(double)) );

            connect(arrDataWorkout[i], SIGNAL(maxHrIntervalChanged(double)), arrUserStudioWidget[i], SLOT(setMaxIntervalHr(double)) );
            connect(arrDataWorkout[i], SIGNAL(maxHrWorkoutChanged(double)), arrUserStudioWidget[i], SLOT(setMaxWorkoutHr(double)) );
            connect(arrDataWorkout[i], SIGNAL(avgHrIntervalChanged(double)), arrUserStudioWidget[i], SLOT(setAvgIntervalHr(double)) );
            connect(arrDataWorkout[i], SIGNAL(avgHrWorkoutChanged(double)), arrUserStudioWidget[i], SLOT(setAvgWorkoutHr(double)) );

            connect(arrDataWorkout[i], SIGNAL(normalizedPowerChanged(double)), arrUserStudioWidget[i], SLOT(setNormalizedPower(double)) );
            connect(arrDataWorkout[i], SIGNAL(intensityFactorChanged(double)), arrUserStudioWidget[i], SLOT(setIntensityFactor(double)) );
            connect(arrDataWorkout[i], SIGNAL(tssChanged(double)), arrUserStudioWidget[i], SLOT(setTrainingStressScore(double)) );
            connect(arrDataWorkout[i], SIGNAL(caloriesWorkoutChanged(double)), arrUserStudioWidget[i], SLOT(setCalories(double)) );

            //             connect(arrDataWorkout[0], SIGNAL(totalDistanceChanged(double)), ui->wid_5_infoWorkout, SLOT(distanceChanged(double)) );

        }
    }

    //    else {
    connect(arrDataWorkout[0], SIGNAL(maxHrIntervalChanged(double)), ui->wid_1_infoBoxHr, SLOT(maxIntervalChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(maxHrWorkoutChanged(double)), ui->wid_1_infoBoxHr, SLOT(maxWorkoutChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(avgHrIntervalChanged(double)), ui->wid_1_infoBoxHr, SLOT(avgIntervalChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(avgHrWorkoutChanged(double)), ui->wid_1_infoBoxHr, SLOT(avgWorkoutChanged(double)) );

    connect(arrDataWorkout[0], SIGNAL(maxCadenceIntervalChanged(double)), ui->wid_3_infoBoxCadence, SLOT(maxIntervalChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(maxCadenceWorkoutChanged(double)), ui->wid_3_infoBoxCadence, SLOT(maxWorkoutChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(avgCadenceIntervalChanged(double)), ui->wid_3_infoBoxCadence, SLOT(avgIntervalChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(avgCadenceWorkoutChanged(double)), ui->wid_3_infoBoxCadence, SLOT(avgWorkoutChanged(double)) );

    connect(arrDataWorkout[0], SIGNAL(maxSpeedIntervalChanged(double)), ui->wid_4_infoBoxSpeed, SLOT(maxIntervalChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(maxSpeedWorkoutChanged(double)), ui->wid_4_infoBoxSpeed, SLOT(maxWorkoutChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(avgSpeedIntervalChanged(double)), ui->wid_4_infoBoxSpeed, SLOT(avgIntervalChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(avgSpeedWorkoutChanged(double)), ui->wid_4_infoBoxSpeed, SLOT(avgWorkoutChanged(double)) );

    connect(arrDataWorkout[0], SIGNAL(maxPowerIntervalChanged(double)), ui->wid_2_infoBoxPower, SLOT(maxIntervalChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(maxPowerWorkoutChanged(double)), ui->wid_2_infoBoxPower, SLOT(maxWorkoutChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(avgPowerIntervalChanged(double)), ui->wid_2_infoBoxPower, SLOT(avgIntervalChanged(double)) );
    connect(arrDataWorkout[0], SIGNAL(avgPowerWorkoutChanged(double)), ui->wid_2_infoBoxPower, SLOT(avgWorkoutChanged(double)) );

    connect(arrDataWorkout[0], SIGNAL(normalizedPowerChanged(double)), ui->wid_5_infoWorkout, SLOT(NP_Changed(double)) );
    connect(arrDataWorkout[0], SIGNAL(intensityFactorChanged(double)), ui->wid_5_infoWorkout, SLOT(IF_Changed(double)) );
    connect(arrDataWorkout[0], SIGNAL(tssChanged(double)), ui->wid_5_infoWorkout, SLOT(TSS_Changed(double)) );
    connect(arrDataWorkout[0], SIGNAL(caloriesWorkoutChanged(double)), ui->wid_5_infoWorkout, SLOT(calories_Changed(double)) );
    connect(arrDataWorkout[0], SIGNAL(totalDistanceChanged(double)), ui->wid_5_infoWorkout, SLOT(distanceChanged(double)) );
    //    }


}

///////////////////////////////////////////////////////////////////////////////////////////////////////
bool WorkoutDialog::eventFilter(QObject *watched, QEvent *event) {

    Q_UNUSED(watched);

    //    qDebug() << "EventFilter " << watched << "Event:" << event;

    if(event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if(keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return ) {
            qDebug() << "ENTER PRESSED- STOP/START WORKOUT!" << watched;
            start_or_pause_workout();
            return true; // mark the event as handled
        }
        else if (keyEvent->key() == Qt::Key_Up) {
            emit increaseDifficulty();
            return true;
        }
        else if (keyEvent->key() == Qt::Key_Down) {
            emit decreaseDifficulty();
            return true;
        }
        else if (keyEvent->key() == Qt::Key_Backspace) {
            lapButtonPressed();
            return true;
        }
        // --- Calibration
        else if (keyEvent->key() == Qt::Key_F1) {
            qDebug() << "F1";
            if (usingFEC) {
                startCalibrateFEC();
            }
            else if (usingPower) {
                startCalibrationPM();
            }
            return true;
        }

        else if (keyEvent->key() == Qt::Key_F2) {
            qDebug() << "F2";
            return true;
        }
        // --- Fullscreen
        else if (keyEvent->key() == Qt::Key_F11) {
            expandWindow();
            return true;
        }



        // --- Radio Prev
        else if (keyEvent->key() == Qt::Key_F6) {
            emit F6previous();
            return true;
        }
        // --- Radio playPause
        else if (keyEvent->key() == Qt::Key_F7) {
            emit F7playPause();
            return true;
        }
        // --- Radio Next
        else if (keyEvent->key() == Qt::Key_F8) {
            emit F8next();
            return true;
        }


    }
    return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutDialog::showFullScreenWin() {

    qDebug() << "showFullScreenWin";
    this->setSizeGripEnabled(false);
    ui->widget_topMenu->setMinExpandExitVisible(true);
    // Re-apply the expand-button icon: when the dialog opens directly in
    // fullscreen the button's image stylesheet was set in the constructor while
    // the button was hidden and is not rendered (shows as a white box) until it
    // is reapplied. showNormalWin() already does this for the windowed path.
    ui->widget_topMenu->updateExpandIcon();

#ifdef Q_OS_MAC
    this->showMaximized();
#else
    this->showFullScreen();
#endif

    isFullScreenFlag = true;
}

//-----------------------------------------
void WorkoutDialog::showNormalWin() {

    qDebug() << "showNormalWin";


#ifdef Q_OS_MAC
    this->showNormal();
#else
    this->showNormal();
#endif

    this->setSizeGripEnabled(true);
    //    ui->widget_topMenu->setMinExpandExitVisible(false);
    ui->widget_topMenu->setMinExpandExitVisible(true);
    ui->widget_topMenu->updateExpandIcon();

    isFullScreenFlag = false;

}






