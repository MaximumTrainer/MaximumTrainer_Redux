#include "workoutdialog.h"
#include "setting_keys.h"
#include "asyncdialogs.h"
#include "virtual_gear.h"
#include "ui_workoutdialog.h"

#ifndef Q_OS_WASM
#include "zwift_click_manager.h"
#endif

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
#include <QJsonDocument>
#include <QJsonObject>

#include "interval.h"
#include "workout.h"
#include "util.h"
#include "qwt_symbol.h"
#include "datacadence.h"
#include "datapower.h"
#include "dataheartrate.h"
#include "dataspeed.h"
#include "faderlabel.h"
#include "clock.h"
#include "workoututil.h"
#include "dialogconfig.h"
#include "webbrowserview.h"
#include <QQuickWidget>
#include "retroracecontroller.h"
#include "workoutraceview.h"
#include "dialogkeyboardshortcuts.h"
#include "logger.h"
#include "strava_service.h"
#include "intervalsicuservice.h"
#include "extrequest.h"
#include "intervalsummaryutil.h"
#include "postworkoutpanel.h"
#include "groupsession.h"
#include "grouppanel.h"
#include "groupridecoordinator.h"

namespace {
}


WorkoutDialog::~WorkoutDialog() {


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

}


WorkoutDialog::WorkoutDialog(Workout workout,  QList<Radio> lstRadio, QVector<UserStudio> vecUserStudio,
                             QWidget *parent) : QDialog(parent), ui(new Ui::WorkoutDialog) {

    ui->setupUi(this);
    this->setFocusPolicy(Qt::ClickFocus);

    // The embedded QWebEngineView needs a native window. Mark its container
    // native so Qt attaches it here rather than recreating the whole dialog's
    // native window (which flickers it closed/reopen and orphans child dialogs).
    // DontCreateNativeAncestors confines that to the container; showEvent() then
    // builds the view while the container is hidden, so it happens invisibly.
    ui->widget_webPlayer->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    ui->widget_webPlayer->setAttribute(Qt::WA_NativeWindow, true);
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
    usingOxygen = false;

    hrPairingDone = false;
    scPairingDone = false;
    cadencePairingDone = false;
    speedPairingDone = false;
    powerPairingDone = false;
    fecPairingDone = false;
    oxygenPairingDone = false;

    trainerControlUserId = -1;

    isUsingSlopeMode = false;



    //// ------------- Clock thread -------------------------
    thread = new QThread(this);
    Clock *clock1 = new Clock("clock1");
    clock1->moveToThread(thread);

    ///This --> Clock
    connect(this, &WorkoutDialog::startClock, clock1, &Clock::startClock);
    connect(this, &WorkoutDialog::pauseClock, clock1, &Clock::pauseClock);
    connect(this, &WorkoutDialog::resumeClock, clock1, &Clock::resumeClock);
    connect(this, &WorkoutDialog::finishClock, clock1, &Clock::finishClock);

    connect(this, &WorkoutDialog::sendUserInfo, clock1, &Clock::receiveUserInfo);
    connect(this, &WorkoutDialog::sendPowerData, clock1, &Clock::receivePowerData);
    //    connect(this, &WorkoutDialog::sendSlopeData, clock1, &Clock::receiveSlopeData);
    connect(this, &WorkoutDialog::startClockSpeed, clock1, &Clock::startClockSpeed);

    ///Clock --> This
    connect(clock1, &Clock::oneCyclePassed, this, &WorkoutDialog::updateRealTimeGraph);
    connect(clock1, &Clock::oneSecPassed, this, &WorkoutDialog::update1sec);
    connect(clock1, &Clock::updateTimePaused, this, &WorkoutDialog::updatePausedTime);
    connect(clock1, &Clock::virtualSpeed, this, &WorkoutDialog::VirtualSpeedDataReceived);

    connect(clock1, &Clock::finished, thread, &QThread::quit);
    connect(clock1, &Clock::finished, clock1, &Clock::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    thread->start();
    ///----------------------------------------------------
    timeElapsed_sec = 0;
    lastIntervalEndTime_msec = 0;

    lastIntervalTotalTimePausedWorkout_msec = 0;
    totalTimePausedWorkout_msec = 0;
    lastIntervalEndTime_sec = 0;


    sendUserInfoToClock();
    emit startClockSpeed();


    // Ignore click on workout plot while widget is loading
    timerIgnoreClickPlot = new QTimer(this);
    bignoreClickPlot = true;
    timerIgnoreClickPlot->start(1000);
    connect(timerIgnoreClickPlot, &QTimer::timeout, this, &WorkoutDialog::ignoreClickPlot);

    // Sensor dropout watchdog — fires every second while WorkoutDialog is open.
    m_dropoutWatchdog = new QTimer(this);
    m_dropoutWatchdog->setInterval(1000);
    connect(m_dropoutWatchdog, &QTimer::timeout, this, &WorkoutDialog::checkSensorDropout);
    m_dropoutWatchdog->start();


    /// Sounds timer
    timerCheckToActivateSound = new QTimer(this);
    timerCheckToActivateSound->setInterval(4000);
    connect(timerCheckToActivateSound, &QTimer::timeout, this, &WorkoutDialog::activateSoundBool);
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
    connect(timerCheckReactivateSoundPowerTooLow, &QTimer::timeout, this, &WorkoutDialog::activateSoundPowerTooLow);
    connect(timerCheckReactivateSoundPowerTooHigh, &QTimer::timeout, this, &WorkoutDialog::activateSoundPowerTooHigh);
    connect(timerCheckReactivateSoundCadenceTooLow, &QTimer::timeout, this, &WorkoutDialog::activateSoundCadenceTooLow);
    connect(timerCheckReactivateSoundCadenceTooHigh, &QTimer::timeout, this, &WorkoutDialog::activateSoundCadenceTooHigh);
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
    ///----------------------------- End Loading widgets ------------------------


    ///-------------------------- Battery widgets ----------------------
    widgetBattery = new QWidget(ui->widget_allSpeedo);
    widgetBattery->setAttribute(Qt::WA_TransparentForMouseEvents,true);
    widgetBattery->setFocusPolicy(Qt::NoFocus);
    QHBoxLayout *hLayout = new QHBoxLayout(widgetBattery);
    QVBoxLayout *vLayoutSub = new QVBoxLayout();
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
    connect(timerLastAnimationAchievementComplete, &QTimer::timeout, this, &WorkoutDialog::lastAchievementAnimationDone);
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
    // App-level filter so workout hotkeys (start/pause, gear ▲▼, lap, …) work
    // regardless of which child widget holds focus — otherwise the arrow keys
    // get eaten by button focus-navigation instead of reaching us.
    qApp->installEventFilter(this);
    loadInterface();


    connect(ui->widget_topMenu, &TopMenuWorkout::config, this, &WorkoutDialog::showConfig);
    connect(ui->widget_topMenu, &TopMenuWorkout::expand, this, &WorkoutDialog::expandWindow);
    connect(ui->widget_topMenu, &TopMenuWorkout::exit, this, &WorkoutDialog::closeWindow);
    connect(ui->widget_topMenu, &TopMenuWorkout::startOrPause, this, &WorkoutDialog::start_or_pause_workout);
    connect(ui->widget_topMenu, &TopMenuWorkout::lap, this, &WorkoutDialog::lapButtonPressed);
    // On-screen virtual-shift arrows mirror the Up/Down keys.
    connect(ui->widget_topMenu, &TopMenuWorkout::gearUp,   this, [this]{ shiftGear(+1); });
    connect(ui->widget_topMenu, &TopMenuWorkout::gearDown, this, [this]{ shiftGear(-1); });
    setupZwiftClick();   // opt-in Zwift Click v2 (right) controller, direct BLE
    connect(this, &WorkoutDialog::insideConfig, ui->widget_topMenu, &TopMenuWorkout::changeConfigIcon);

    // Make the splitter gutters easy to grab — the 1px default was nearly
    // impossible to hit. The handle is now 10px (see .ui) with a subtle fill
    // that highlights on hover so the drag zone is discoverable.
    ui->splitter->setStyleSheet(
        QStringLiteral("QSplitter::handle:vertical { background: #3a3f4b; }"
                       "QSplitter::handle:vertical:hover { background: #5a6473; }"));


    connect(this, &WorkoutDialog::increaseDifficulty, ui->widget_workoutPlot, &WorkoutPlot::increaseDifficulty);
    connect(this, &WorkoutDialog::decreaseDifficulty, ui->widget_workoutPlot, &WorkoutPlot::decreaseDifficulty);
    connect(ui->widget_workoutPlot, &WorkoutPlot::workoutDifficultyChanged, this, &WorkoutDialog::adjustWorkoutDifficulty);
    connect(ui->widget_workoutPlot, &WorkoutPlot::intervalClicked, this, &WorkoutDialog::moveToInterval);


    // Achievement
    connect(achievementManager, &ManagerAchievement::achievementCompleted, this, &WorkoutDialog::achievementReceived);
    // Video
    connect(this, &WorkoutDialog::playPlayer, ui->widgetVideo, &QtMediaPlayer::resume);
    connect(this, &WorkoutDialog::pausePlayer, ui->widgetVideo, &QtMediaPlayer::pause);


    // Initialise DataWorkout
    initDataWorkout();
    //createUserStudio Widget
    createUserStudioWidget();
    // Connect DataWorkout
    connectDataWorkout();


    // The embedded media player is now the only video option (the web player
    // was removed), so always show it. It displays its own "right-click to open
    // media" hint until a file/URL is loaded.
    ui->widgetVideo->setVisible(true);
    ui->wid_1_infoBoxHr->setTypeInfoBox(InfoWidget::HEART_RATE);
    ui->wid_2_infoBoxPower->setTypeInfoBox(InfoWidget::POWER);
    ui->wid_3_infoBoxCadence->setTypeInfoBox(InfoWidget::CADENCE);
    ui->wid_4_infoBoxSpeed->setTypeInfoBox(InfoWidget::SPEED);
    // Rowing (Beta): cadence box shows stroke rate, speed box shows pace /500m.
    ui->wid_3_infoBoxCadence->setRowingMode(account->rowing_mode);
    ui->wid_4_infoBoxSpeed->setRowingMode(account->rowing_mode);
    if (!account->distance_in_km) {
        ui->wid_4_infoBoxSpeed->setUseMiles(true);
        ui->wid_5_infoWorkout->setDistanceInMile(true);
    }

    //Set User Data in all widgets
    ui->widget_workoutPlot->setUserData(account->FTP, account->LTHR);
    ui->wid_1_workoutPlot_HeartrateZoom->setUserData(account->FTP, account->LTHR);
    ui->wid_2_workoutPlot_PowerZoom->setUserData(account->FTP, account->LTHR);
    ui->wid_3_workoutPlot_CadenceZoom->setUserData(account->FTP, account->LTHR);
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


    currentWorkoutDifficultyPercentage = 0;

    isAskingUserQuestion = false;
    isWorkoutStarted = false;
    isWorkoutPaused = true;
    isWorkoutOver = false;
    testResultShown = false;
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


    timeElapsedTotal = QTime(0,0,0,0);

    trainerControlUserId = -1;
    currentTargetPower = -1;
    currentTargetPowerRange = -1;
    currentTargetCadence = -1;
    currentTargetCadenceRange = -1;


    //Dialog config — shown non-modally so the workout session continues uninterrupted (#137)
    dconfig = new DialogConfig(lstRadio, this, this);
    dconfig->setModal(false);
    // When the workout window is forced on top it would otherwise float above
    // (and block interaction with) its own non-modal settings dialog. Give the
    // settings dialog the same stay-on-top hint so it remains reachable.
    if (account->force_workout_window_on_top)
        dconfig->setWindowFlag(Qt::WindowStaysOnTopHint, true);
    connect(dconfig, &QDialog::finished, this, [this](int) { emit insideConfig(false); });

    //Internet Radio Player (QtMultimedia: QMediaPlayer streaming a network URL)
#ifdef GC_HAVE_QTMULTIMEDIA
    radioPlayer = new QtMediaPlayer(this);
    radioPlayer->setVisible(false);
    radioPlayer->setRadio(true);

    connect(dconfig, &DialogConfig::signal_connectToRadioUrl, radioPlayer, &QtMediaPlayer::openUrlRadio);
    connect(dconfig, &DialogConfig::signal_volumeRadioChanged, radioPlayer, &QtMediaPlayer::changeVolume);
    connect(dconfig, &DialogConfig::signal_stopPlayingRadio, radioPlayer, &QtMediaPlayer::stop);

    connect(radioPlayer, &QtMediaPlayer::playing, dconfig, &DialogConfig::radioStartedPlaying);
    connect(radioPlayer, &QtMediaPlayer::paused, dconfig, &DialogConfig::radioStoppedPlaying);
    connect(radioPlayer, &QtMediaPlayer::stopped, dconfig, &DialogConfig::radioStoppedPlaying);
    connect(radioPlayer, &QtMediaPlayer::playing, ui->widget_topMenu, &TopMenuWorkout::radioStartedPlaying);
    connect(radioPlayer, &QtMediaPlayer::paused, ui->widget_topMenu, &TopMenuWorkout::radioStoppedPlaying);
    connect(radioPlayer, &QtMediaPlayer::stopped, ui->widget_topMenu, &TopMenuWorkout::radioStoppedPlaying);
#endif

    connect(dconfig, &DialogConfig::radioStatus, ui->widget_topMenu, &TopMenuWorkout::updateRadioStatus);

    //radio
    connect(this, &WorkoutDialog::F6previous, dconfig, &DialogConfig::on_pushButton_prevRadio_clicked);
    connect(this, &WorkoutDialog::F7playPause, dconfig, &DialogConfig::playPauseRadio);
    connect(this, &WorkoutDialog::F8next, dconfig, &DialogConfig::on_pushButton_nextRadio_clicked);
    connect(ui->widget_topMenu, &TopMenuWorkout::prevRadio, dconfig, &DialogConfig::on_pushButton_prevRadio_clicked);
    connect(ui->widget_topMenu, &TopMenuWorkout::playPauseRadio, dconfig, &DialogConfig::playPauseRadio);
    connect(ui->widget_topMenu, &TopMenuWorkout::nextRadio, dconfig, &DialogConfig::on_pushButton_nextRadio_clicked);

    // Radio volume indicator in the top bar — reflects the config slider, so it
    // updates whether the volume changes via the dialog or a keyboard shortcut.
    connect(dconfig, &DialogConfig::signal_volumeRadioChanged, ui->widget_topMenu, &TopMenuWorkout::updateRadioVolume);
    ui->widget_topMenu->updateRadioVolume(dconfig->radioVolume());

    // Flash the matching top-bar control on each radio action (any source: keys
    // or on-screen), mirroring the gear-shift feedback.
    // (Volume flashes from updateRadioVolume on an actual value change, so it
    // doesn't pulse when a station change merely re-applies the same volume.)
    connect(this, &WorkoutDialog::F6previous,  ui->widget_topMenu, &TopMenuWorkout::flashRadioPrev);
    connect(this, &WorkoutDialog::F8next,      ui->widget_topMenu, &TopMenuWorkout::flashRadioNext);
    connect(this, &WorkoutDialog::F7playPause, ui->widget_topMenu, &TopMenuWorkout::flashRadioPlayPause);


    //Disable button in Test mode
    if (workout.getWorkoutNameEnum() == Workout::FTP_TEST || workout.getWorkoutNameEnum() == Workout::FTP8min_TEST ||
            workout.getWorkoutNameEnum() == Workout::MAP_TEST ) {
        isTestWorkout = true;
    }
    else {
        isTestWorkout = false;
    }

    // The lap / "Interval" button only makes sense in free ride (manual laps);
    // structured workouts and tests advance intervals automatically, so hide it.
    ui->widget_topMenu->setButtonLapVisible(workout.getWorkoutNameEnum() == Workout::OPEN_RIDE);


    initUI();
    setWidgetsStopped(true);

    this->setFocus();

    // Remove loading Cursor
    QApplication::restoreOverrideCursor();
}


void WorkoutDialog::toggleTransparent() {

    if (isTransparent) {
        //revert back to normal QDialog, use saved stylesheet
        this->setStyleSheet(bakStylesheet);
    }
    else {

        //save current stylesheet
        bakStylesheet = this->styleSheet();

        setStyleSheet("background:transparent;");
        setAttribute(Qt::WA_TranslucentBackground);
        setWindowFlags(Qt::FramelessWindowHint); //this close the QDialog if not called in Constructor, why?.

    }
    isTransparent = !isTransparent;
}


void WorkoutDialog::addToControlList(int deviceId, int fromHubNumber) {


    if (!hashControlList.contains(deviceId)) {
        hashControlList.insert(deviceId, fromHubNumber);
        //emit signal back to Hub to let him add this ID
        emit permissionGrantedControl(deviceId, fromHubNumber);
    }
    /// DO NOT USE, because sending duplicate cause transfer to fail
    //    else if (hashControlList.size() >= nbTotalFecTrainer) {
    //        hashControlList.insert(deviceId, fromHubNumber);
    //        emit permissionGrantedControl(deviceId, fromHubNumber);
    //    }
    else {
        qInfo() << "Trainer already controlled by another hub:" << deviceId;
    }

}


void WorkoutDialog::batteryStatusReceived(QString sensorType, int percentage) {


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


void WorkoutDialog::slotFinishedGetPixmap() {


    QByteArray m_DownloadedData = replyGetBixmap->readAll();
    QPixmap pixmap; //48,48 icon

    pixmap.loadFromData(m_DownloadedData);
    labelIcon->setPixmap(pixmap);

    replyGetBixmap->deleteLater();
}


void WorkoutDialog::animateAchievement() {


    achievementCurrentlyPlaying = true;
    if (queueAchievement.size() == 0)
        return;

    /// Play sound
    if (account->enable_sound && account->sound_achievement)
        soundPlayer->playSoundAchievement();


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
    connect(replyGetBixmap, &QNetworkReply::finished, this, &WorkoutDialog::slotFinishedGetPixmap);
    widgetAchievement->setVisible(true);
    widgetAchievement->fadeInAndFadeOutAfterPause(600, 2000, 6500);
    timerLastAnimationAchievementComplete->start(9000); ///Check other achievement to play in the queue
}

void WorkoutDialog::lastAchievementAnimationDone() {

    timerLastAnimationAchievementComplete->stop();
    achievementCurrentlyPlaying = false;

    ///Remove queue item, check if queue is empty, if empty, put flag to not playing;
    queueAchievement.dequeue();
    if (!queueAchievement.isEmpty())
        animateAchievement();
}

void WorkoutDialog::initUI() {

    if (!account->enable_studio_mode) {
        layoutBottomWidgets();
    }

    ///Timers
    ui->widget_topMenu->setTimersVisible(true);
    showTimerIntervalRemaining(account->show_interval_remaining);
    showTimerWorkoutRemaining(account->show_workout_remaining);
    showTimerWorkoutElapsed(account->show_elapsed);
    showTimerCurrentTarget(account->show_current_target);

    /// Widgets
    showHeartRateDisplayWidget(account->display_hr);
    showPowerDisplayWidget(account->display_power);
    showCadenceDisplayWidget(account->display_cadence);
    showSpeedDisplayWidget();
    showTrainerSpeed(account->show_trainer_speed);
    // Speed is always virtual (derived from power), for solo and Studio riders.
    useVirtualSpeedData(true);
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
    /// VideoDisplay
    showVideoPlayer(account->display_video);


    ui->widget_topMenu->setWorkoutNameLabel(workout.getName());

    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {
        ui->widget_topMenu->setWorkoutNameLabel(tr("Free Ride"));
        ui->widget_topMenu->setFreeRideMode();
    }
    else if (workout.getWorkoutNameEnum() == Workout::MAP_TEST) {
        ui->widget_topMenu->setMAPMode();
    }
    if (workout.getWorkoutNameEnum() == Workout::MAP_TEST || workout.getWorkoutNameEnum() == Workout::OPEN_RIDE)
        ui->widget_workoutPlot->setSpinBoxDisabled();


    /// First Interval
    if (workout.getWorkoutNameEnum() != Workout::OPEN_RIDE) {

        timeWorkoutRemaining = workout.getDurationQTime();
        ui->widget_topMenu->setWorkoutRemainingTime(timeWorkoutRemaining);

        Interval firstInterval = workout.getInterval(0);
        timeInterval = firstInterval.getDurationQTime();
        ui->widget_topMenu->setIntervalTime(timeInterval);

        adjustTargets(firstInterval);
    }
    else {
        // open ride
        timeWorkoutRemaining = timeInterval = QTime(0,0,0,0);
        ui->widget_topMenu->setIntervalTime(timeInterval);

        targetPowerChanged_f(-1, 30);
        targetCadenceChanged_f(-1, 20);
        targetHrChanged_f(-1, 20);
    }
    ui->wid_1_workoutPlot_HeartrateZoom->setPosition(0);
    ui->wid_2_workoutPlot_PowerZoom->setPosition(0);
    ui->wid_3_workoutPlot_CadenceZoom->setPosition(0);


    if (account->enable_studio_mode) {
        ui->wid_1_infoBoxHr->setVisible(false);
        ui->wid_1_infoBoxHr->setVisible(false);
        ui->wid_1_workoutPlot_HeartrateZoom->setVisible(false);
        ui->wid_2_balancePower->setVisible(false);
        ui->wid_2_infoBoxPower->setVisible(false);
        ui->wid_2_workoutPlot_PowerZoom->setVisible(false);
        ui->wid_3_infoBoxCadence->setVisible(false);
        ui->wid_3_workoutPlot_CadenceZoom->setVisible(false);
        ui->wid_4_infoBoxSpeed->setVisible(false);
        ui->wid_5_infoWorkout->setVisible(false);
        ui->wid_oxygen->setVisible(false);
    }

    setMessagePlot();
}


void WorkoutDialog::layoutBottomWidgets() {


    QHBoxLayout *horizontalLayout = static_cast<QHBoxLayout*>(ui->horizontalLayout_Bottom->layout());

    horizontalLayout->removeWidget(ui->wid_1_infoBoxHr);
    horizontalLayout->removeWidget(ui->wid_1_workoutPlot_HeartrateZoom);

    horizontalLayout->removeWidget(ui->wid_2_balancePower);
    horizontalLayout->removeWidget(ui->wid_2_infoBoxPower);
    horizontalLayout->removeWidget(ui->wid_2_workoutPlot_PowerZoom);

    horizontalLayout->removeWidget(ui->wid_3_infoBoxCadence);
    horizontalLayout->removeWidget(ui->wid_3_workoutPlot_CadenceZoom);

    horizontalLayout->removeWidget(ui->wid_4_infoBoxSpeed);
    horizontalLayout->removeWidget(ui->wid_oxygen);
    horizontalLayout->removeWidget(ui->wid_5_infoWorkout);


    for (QWidget *widget : {static_cast<QWidget *>(ui->wid_2_infoBoxPower), static_cast<QWidget *>(ui->wid_2_workoutPlot_PowerZoom),
                            static_cast<QWidget *>(ui->wid_3_infoBoxCadence), static_cast<QWidget *>(ui->wid_3_workoutPlot_CadenceZoom),
                            static_cast<QWidget *>(ui->wid_2_balancePower),
                            static_cast<QWidget *>(ui->wid_1_infoBoxHr), static_cast<QWidget *>(ui->wid_1_workoutPlot_HeartrateZoom),
                            static_cast<QWidget *>(ui->wid_4_infoBoxSpeed),
                            static_cast<QWidget *>(ui->wid_5_infoWorkout),
                            static_cast<QWidget *>(ui->wid_oxygen)})
        horizontalLayout->addWidget(widget);
}


void WorkoutDialog::updatePausedTime(double totalTimePaused_msec) {

    this->totalTimePausedWorkout_msec = totalTimePaused_msec;
}

void WorkoutDialog::updateRealTimeGraph(double totalTimeElapsed_msec) {

    timeElapsed_sec = totalTimeElapsed_msec /1000;

    if (account->enable_studio_mode)
        return;

    // The clock ticks at 40Hz, but each replot() is a full QWT canvas repaint of
    // three plots — 120 repaints/sec for data that only changes a few times a
    // second. Coalesce to kMiniGraphRefreshMs (~20fps): smooth scrolling at half
    // the original work. Throttling on elapsed time (not a wall-clock read) keeps
    // this pause-aware, and the curve samples appended by the sensor callbacks are
    // picked up by the next replot regardless.
    if (totalTimeElapsed_msec - lastMiniGraphReplot_msec < kMiniGraphRefreshMs)
        return;
    lastMiniGraphReplot_msec = totalTimeElapsed_msec;

    ui->wid_1_workoutPlot_HeartrateZoom->moveIntervalTime(totalTimeElapsed_msec);
    ui->wid_2_workoutPlot_PowerZoom->moveIntervalTime(totalTimeElapsed_msec);
    ui->wid_3_workoutPlot_CadenceZoom->moveIntervalTime(totalTimeElapsed_msec);
}


// For MAP Test, check last second average, if it's under the target for more than 20secs total or 10 sec consecutive, MAP test over
void WorkoutDialog::checkMAPTestOver() {

    // The first and last seconds of an interval (sounds muted) are not scored.
    const bool testFailed = m_mapTest.recordSecond(soundsActive, m_secondSamples.rider(0).power,
                                                   currentTargetPower, currentTargetPowerRange);

    const QString redFontStart = "<font color=\"red\">";
    const QString fontEnd = "</font>";
    const auto remaining = [&](const QString &label, int left, bool warn) {
        const QString seconds = QString::number(left) + " sec.";
        return label + (warn ? redFontStart + seconds + fontEnd : seconds);
    };
    const QString textTotalOffTargetInterval =
        remaining(tr("Total remaining: "), MapTestTracker::kMaxSecondsOffTarget - m_mapTest.secondsOffTarget(),
                  m_mapTest.secondsOffTarget() >= 15);
    const QString textConsecutiveOffTarget =
        remaining(tr("Consecutive remaining: "), MapTestTracker::kMaxConsecutiveOffTarget - m_mapTest.consecutiveOffTarget(),
                  m_mapTest.consecutiveOffTarget() >= 5);

    //Update Time remaining off target (top Left of graph)
    mainPlot->setAlertMessage(m_mapTest.offTargetThisSecond(), false, tr("MAP Interval ") + "#" + QString::number(m_mapTest.step()) + " - " + QString::number(currentTargetPower) + " watts"
                              + "<div style='color:white;height:7px;'>---------------------------------</div><br/> " + textTotalOffTargetInterval +
                              + "<br/>" + textConsecutiveOffTarget, 500);
    if (testFailed) {
        const int secCompletedInInterval = MapTestTracker::kStepSeconds - Util::convertQTimeToSecD(timeInterval);
        const double mapResult = MapTestTracker::mapWatts(currentTargetPower, secCompletedInInterval);
        const int estimatedFTP = MapTestTracker::estimatedFtp(mapResult);
        mainPlot->setAlertMessage(true, false, workout.getName() + tr(" Result")
                                  + "<div style='color:white;height:7px;'>-------------------</div><br/> "  + QString::number(mapResult, 'f', 1)  + " watts" + "<br/>"
                                  + tr("FTP: ") + QString::number(estimatedFTP) + tr(" watts")
                                  + tr(" (75% of MAP; previous: ") + QString::number(account->FTP) + tr(" watts)"), 500);
        // Persist the estimated FTP into the athlete profile, like the FTP
        // tests do (LTHR and weight unchanged).
        if (estimatedFTP > 0) {
            account->saveProfileFields(estimatedFTP, account->LTHR, account->weight_kg);
            emit ftp_lthr_changed();
        }
        //Go to cooldown interval (last interval)
        moveToInterval(workout.getNbInterval()-1, -1, -1, false);
    }
}


void WorkoutDialog::update1sec(double totalTimeElapsed_sec) {

    // Diagnostic (temporary): the log timestamps on this line reveal whether the
    // Clock ticks faster than real time (the "workout / race finishes instantly"
    // report seen on one PC). Normally one line per real second with clockElapsed
    // incrementing by 1; a burst of lines, or clockElapsed jumping, points at the
    // Clock / QElapsedTimer.
    qDebug() << "[update1sec] clockElapsed=" << totalTimeElapsed_sec
             << "interval" << currentInterval << "/" << workout.getNbInterval();

    int totalTimeElapsed_sec_i = (int)totalTimeElapsed_sec;

    // Send last second data to DataWorkout, at second 2, send data received between [second 1 - second 2]
    sendLastSecondData(totalTimeElapsed_sec_i);

    // Check MAP Test stop condition
    if (!account->enable_studio_mode && workout.getWorkoutNameEnum() == Workout::MAP_TEST && currentIntervalObj.isTestInterval() && m_secondSamples.rider(0).power != -1) {
        checkMAPTestOver();
    }


    m_secondSamples.reset();

    // Update the graph 'dark' area
    ui->widget_workoutPlot->updateMarkerTimeNow(totalTimeElapsed_sec);

    // Update Timers
    timeElapsedTotal = timeElapsedTotal.addSecs(1);
    ui->widget_topMenu->setWorkoutTime(timeElapsedTotal);

    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {
        timeInterval = timeInterval.addSecs(1);
        ui->widget_topMenu->setIntervalTime(timeInterval);
    }
    else {
        timeInterval = timeInterval.addSecs(-1);
        ui->widget_topMenu->setIntervalTime(timeInterval);
        timeWorkoutRemaining = timeWorkoutRemaining.addSecs(-1);
        ui->widget_topMenu->setWorkoutRemainingTime(timeWorkoutRemaining);

    }

    // Early exit
    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {
        if (virtualShiftingActive())
            sendGearLoad();
        else
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


        timerCheckToActivateSound->start();
        currentInterval++;

        // Show the FTP test result as soon as the last test interval
        // completes, so riders who skip the cooldown still get it.
        const Workout::WORKOUT_NAME workoutType = workout.getWorkoutNameEnum();
        if (currentIntervalObj.isTestInterval()
                && (workoutType == Workout::FTP_TEST || workoutType == Workout::FTP8min_TEST)) {
            bool testIntervalRemaining = false;
            for (int i = currentInterval; i < workout.getNbInterval(); i++) {
                if (workout.getInterval(i).isTestInterval()) {
                    testIntervalRemaining = true;
                    break;
                }
            }
            if (!testIntervalRemaining)
                showTestResult();
        }

        if (workout.getWorkoutNameEnum() == Workout::MAP_TEST && currentIntervalObj.isTestInterval()) {
            achievementManager->checkMAPAchievement(m_mapTest.completeStep());
        }


        // Workout over?
        if ( currentInterval >= workout.getNbInterval() ) {
            emit pauseClock();
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

        if (raceController) {
            raceController->markIntervalBoundary();   // road line
            raceController->setIntervalMessage(currentIntervalObj.getDisplayMessage());
        }
    }


    ignoreCondition = false;
}


void WorkoutDialog::lapButtonPressed() {

    qInfo() << "Lap at" << timeElapsed_sec << "s";

    if (isTestWorkout) {
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
        ui->widget_topMenu->setIntervalTime(timeInterval);
    }
}

void WorkoutDialog::adjustWorkoutDifficulty(int percentageIncrease) {

    double diffFromActualDifficulty = percentageIncrease - currentWorkoutDifficultyPercentage;
    diffFromActualDifficulty = diffFromActualDifficulty/100.0;


    // Adjust difficulty live — rebuild the targets/plot without pausing. The
    // old code paused here (and never resumed), so every ±% nudge stopped the
    // workout; the sibling insertInterval() already drops the same pause as
    // "should be done in real time".

    // Compute new workout
    QList<Interval> lstIntervalAdjusted;

    for (Interval interval : workout.getLstInterval()) {

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
    this->workout = std::move(workoutEdited);

    // refresh view
    ui->widget_workoutPlot->setWorkoutData(workout, false);
    ui->wid_1_workoutPlot_HeartrateZoom->setWorkoutData(workout, WorkoutPlotZoomer::HEART_RATE, false);
    ui->wid_2_workoutPlot_PowerZoom->setWorkoutData(workout, WorkoutPlotZoomer::POWER, false);
    ui->wid_3_workoutPlot_CadenceZoom->setWorkoutData(workout, WorkoutPlotZoomer::CADENCE, false);

    // setWorkoutData() rebuilds the plot from scratch, dropping the "now" marker
    // and the done-zone shading — re-apply the current position so the big graph
    // still shows where we are instead of looking brand new.
    ui->widget_workoutPlot->updateMarkerTimeNow(timeElapsed_sec);

    //adjust mini-graph and widget to new target
    currentIntervalObj = workout.getInterval(currentInterval);
    adjustTargets(currentIntervalObj);

    // Pulse the on-screen +/− button green on any difficulty change (keys
    // or on-screen), like the gear and radio controls.
    if (percentageIncrease > currentWorkoutDifficultyPercentage)
        ui->widget_workoutPlot->flashDifficulty(true);
    else if (percentageIncrease < currentWorkoutDifficultyPercentage)
        ui->widget_workoutPlot->flashDifficulty(false);

    currentWorkoutDifficultyPercentage = percentageIncrease;
}


void WorkoutDialog::insertInterval() {


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
    this->workout = std::move(workoutEdited);

    // refresh view
    ui->widget_workoutPlot->setWorkoutData(workout, false);
    ui->wid_1_workoutPlot_HeartrateZoom->setWorkoutData(workout, WorkoutPlotZoomer::HEART_RATE, false);
    ui->wid_2_workoutPlot_PowerZoom->setWorkoutData(workout, WorkoutPlotZoomer::POWER, false);
    ui->wid_3_workoutPlot_CadenceZoom->setWorkoutData(workout, WorkoutPlotZoomer::CADENCE, false);

    ui->widget_workoutPlot->updateMarkerTimeNow(timeElapsed_sec);
}


void WorkoutDialog::moveToInterval(int nbInterval, double secWorkout, double startIntervalSec, bool showConfirmation) {
    Q_UNUSED(secWorkout);

    //clear focus on QwtPlot for hotkeys needs focus on this QDialog
    this->setFocus();

    // Not legal for test workout or Open Ride (except Manual = showConfirmation at false)
    if (showConfirmation && (bignoreClickPlot || isTestWorkout  || workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) )
        return;


    int nbIntervalToDelete = 0;
    if (currentInterval >= nbInterval)
        return;
    else {
        nbIntervalToDelete = nbInterval - currentInterval -1;
    }

    // pause workout if not paused and manually clicked
    if (!isWorkoutPaused && showConfirmation) {
        start_or_pause_workout();
    }

    QString timeStartInterval = Util::showQTimeAsString(Util::convertMinutesToQTime(startIntervalSec/60.0));

    //ask confirmation (async: exec() is fatal on WASM)
    if (showConfirmation) {
        isAskingUserQuestion = true;
        auto *msgBox = new QMessageBox(QMessageBox::Question, QString(),
                                       tr("Move to the interval starting at: ") + timeStartInterval + "?",
                                       QMessageBox::Yes | QMessageBox::No, this);
        msgBox->setDefaultButton(QMessageBox::No);
        msgBox->setAttribute(Qt::WA_DeleteOnClose);
        connect(msgBox, &QDialog::finished, this, [this](int) { isAskingUserQuestion = false; });
        connect(msgBox, &QMessageBox::buttonClicked, this,
                [this, msgBox, nbIntervalToDelete](QAbstractButton *button) {
            if (msgBox->standardButton(button) == QMessageBox::Yes)
                applyMoveToInterval(nbIntervalToDelete);
        });
        msgBox->open();
        return;
    }

    applyMoveToInterval(nbIntervalToDelete);
}


void WorkoutDialog::applyMoveToInterval(int nbIntervalToDelete) {

    //Calculate timeDone in currentInterval
    double currentIntervalTimeDone =  timeElapsed_sec - lastIntervalEndTime_sec;
    lastIntervalEndTime_sec += currentIntervalTimeDone;

    // Compute new workout and new interval
    QList<Interval> copyLstInterval = workout.getLstInterval();
    Interval intervalToModify = currentIntervalObj;

    // Update time interval
    QTime timeActuallyDone(0,0,0,0);
    timeActuallyDone = timeActuallyDone.addMSecs(currentIntervalTimeDone*1000);
    intervalToModify.setTime(timeActuallyDone);

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


    //Remove interval that we skipped (if clicked more than 1 interval ahead).
    // Guard the index: an abnormal jump (e.g. negative time-done) can over-count
    // nbIntervalToDelete; removeAt() past the end is undefined behaviour (crash).
    for (int i=0; i<nbIntervalToDelete && copyLstInterval.size() > currentInterval+1; i++) {
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

    //Go to selected interval (clamp the index — getInterval() uses at() with no
    //bounds check, so an out-of-range index would segfault).
    currentInterval++;
    if (currentInterval >= workout.getLstInterval().size())
        currentInterval = workout.getLstInterval().size() - 1;
    currentIntervalObj = workout.getInterval(currentInterval);
    adjustTargets(currentIntervalObj);

    // Keep the game's interval label in step with a manual jump (adjustTargets
    // already refreshes the power/cadence/HR targets, but not the message).
    if (raceController) {
        raceController->markIntervalBoundary();
        raceController->setIntervalMessage(currentIntervalObj.getDisplayMessage());
    }

    if (currentIntervalObj.getDisplayMessage() != "")
        ui->widget_workoutPlot->setDisplayIntervalMessage(false, currentIntervalObj.getDisplayMessage(), account->nb_sec_show_interval);


    //-- Update Timers
    timeInterval = currentIntervalObj.getDurationQTime();
    ui->widget_topMenu->setIntervalTime(timeInterval);

    //calculate workoutRemainingTime
    timeWorkoutRemaining = QTime(0,0,0,0);
    for (int i=currentInterval; i<workout.getLstInterval().size(); i++) {
        timeWorkoutRemaining = timeWorkoutRemaining.addSecs(Util::convertQTimeToSecD(workout.getInterval(i).getDurationQTime()));
    }
    ui->widget_topMenu->setWorkoutRemainingTime(timeWorkoutRemaining);

    //start timer ignore click (click on QDialog response trigger a new click event)
    bignoreClickPlot = true;
    timerIgnoreClickPlot->start(500);
}


void WorkoutDialog::moveToNextInterval() {


    int timeToAdd = Util::convertQTimeToSecD(currentIntervalObj.getDurationQTime());
    lastIntervalEndTime_sec += timeToAdd;

    changeIntervalDisplayNextSecond = false;
    ignoreCondition = true;

    if (currentInterval + 1 >= workout.getLstInterval().size())
        return;   // already on the last interval — no next to preview (at() would crash)
    Interval newInterval = workout.getLstInterval().at(currentInterval+1);
    if (newInterval.getDisplayMessage() != "")
        ui->widget_workoutPlot->setDisplayIntervalMessage(false, newInterval.getDisplayMessage(), account->nb_sec_show_interval);

    ui->widget_topMenu->setIntervalTime(newInterval.getDurationQTime());

    adjustTargets(newInterval);
}

void WorkoutDialog::startWorkout() {

    // Open each workout in an easy gear; the rider's shifts then persist across
    // intervals (only reset here, at workout start).
    m_virtualGear = VirtualGear::DefaultStartGear;
    updateGearIndicator();

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
        arrDataWorkout[0]->initFitFile(false, account->email_clean, workout.getName(), dateTimeStartedWorkout.toUTC(), account->rowing_mode );
    }

    timerCheckToActivateSound->start();

    if (raceController) raceController->beginRace();   // fire the race gun
}


void WorkoutDialog::workoutOver() {

    isWorkoutOver = true;
    stopErgSmoothing();

    // Release virtual-shifting resistance so the trainer doesn't hold a load
    // after the workout ends (it would otherwise persist until disconnect).
    if (virtualShiftingActive())
        emit setLoad(trainerControlUserId, 0);

    if (raceController) raceController->finishRace();   // finish line + celebration

    qInfo() << "Workout completed";
    if (account->enable_sound && account->sound_end_workout)
        soundPlayer->playSoundEndWorkout();

    ui->widget_workoutPlot->setDisplayIntervalMessage(true, tr("Workout Completed!"), 20000);

    isWorkoutPaused = true;
    ui->widget_topMenu->setButtonStartReady(false);
    ui->widget_topMenu->setButtonLapVisible(false);
    ui->widget_workoutPlot->setSpinBoxDisabled();


    setWidgetsStopped(true);

    achievementManager->workoutCompleted(workout);


    //Close FIT FILE
    closeFitFiles(timeElapsed_sec);


    // Show Test Result?
    showTestResult();


    // Set workout to done
    account->hashWorkoutDone.insert(workout.getName());
}


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


    if (isWorkoutOver) {
        return;
    }

    // In a group workout nobody starts alone: the host's Enter/Start schedules
    // the shared start, and pedalling triggers wait for it.
    if (!isWorkoutStarted && m_groupRide && m_groupRide->awaitingStart()) {
        if (m_groupRide->isHost())
            m_groupRide->requestStart();
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
        if (webPlayer) webPlayer->playVideo();

    }
    // If workout paused, we resume it
    else if (isWorkoutStarted && isWorkoutPaused) {
        qInfo() << "Workout resumed";
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
        if (webPlayer) webPlayer->playVideo();

    }
    // If not paused, we pause it
    else if (isWorkoutStarted && !isWorkoutPaused) {
        qInfo() << "Workout paused";
        if (account->enable_sound  && account->sound_pause_resume_workout)
            soundPlayer->playSoundPauseWorkout();
        ui->widget_topMenu->setButtonStartPaused(false);
        isWorkoutPaused = true;
        stopErgSmoothing();
        setWidgetsStopped(true);
        setMessagePlot();
        emit pauseClock();
        emit pausePlayer();
        if (webPlayer) webPlayer->pauseVideo();
    }

    // Keep the race in sync with workout pause/resume.
    if (raceController && isWorkoutStarted && !isWorkoutOver)
        raceController->setRacePaused(isWorkoutPaused);
}


void WorkoutDialog::recordRiderSecond(int riderIndex, int seconds) {
    const RiderSecond &second = m_secondSamples.rider(riderIndex);
    arrDataWorkout[riderIndex]->updateData(account->enable_studio_mode, seconds, second.heartRate, second.cadence,
                                           second.speed, second.power, second.rightPedalPercent,
                                           second.saturatedHemoglobinPercent, second.totalHemoglobinConcentration);
}


void WorkoutDialog::sendLastSecondData(int seconds) {

    if (m_raceView)
        m_raceView->onWorkoutSecond(workout, currentInterval, seconds,
                                    Util::convertQTimeToSecD(timeInterval), account->FTP);

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            recordRiderSecond(i, seconds);
        }
    }
    else {
        recordRiderSecond(0, seconds);
    }

    // Re-send the gear's load each second so faster/slower pedaling changes
    // resistance like a real gear (ERG-fallback trainers); harmless dedup for
    // resistance-level trainers.
    if (gearsDriveNow())
        sendGearLoad();
}


void WorkoutDialog::markSensorPaired(bool &pairingDone, QLabel *pairingLabel) {
    if (account->enable_studio_mode || pairingDone)
        return;
    pairingDone = true;
    pairingLabel->setText(pairingLabel->text() + msgPairingDone);
    pairingLabel->setStyleSheet("background-color : rgba(1,1,1,220); color : green;");
    checkPairingCompleted();
}


void WorkoutDialog::HrDataReceived(int userID, int value) {


    // invalid value, show "-" to the user
    if (value == -1) {
        ui->wid_1_infoBoxHr->setValue(value);
        ui->wid_1_workoutPlot_HeartrateZoom->updateTextLabelValue(value);
        if (account->enable_studio_mode) {
            arrUserStudioWidget[userID-1]->setHrValue(value);
        }
        return;
    }
    if (value < 0)
        return;

    if (raceController && userID == 1)
        raceController->setLiveHr(value);

    // Track for sensor dropout detection
    if (!account->enable_studio_mode && isWorkoutStarted && !isWorkoutOver)
        m_lastHrMs = QDateTime::currentMSecsSinceEpoch();


    if (userID == 1 && m_groupRide) m_groupRide->recordHeartRate(value);

    // Mark pairing as done
    markSensorPaired(hrPairingDone, labelPairHr);


    if (!isWorkoutPaused && !isWorkoutOver) {

        arrDataWorkout[userID-1]->checkUpdateMaxHr(value);

        m_secondSamples.addSample(userID-1, SensorSecondAccumulator::Metric::HeartRate, value);


        //Update Graph (zoomer)
        ui->wid_1_workoutPlot_HeartrateZoom->updateCurve(timeElapsed_sec, value);
    }

    // Show data to the display
    ui->wid_1_infoBoxHr->setValue(value);
    ui->wid_1_workoutPlot_HeartrateZoom->updateTextLabelValue(value);
    if (account->enable_studio_mode) {
        arrUserStudioWidget[userID-1]->setHrValue(value);
    }
}


void WorkoutDialog::CadenceDataReceived(int userID, int value) {

    // Animate the race player's legs at the rider's real cadence.
    if (raceController && userID == 1 && value >= 0 && value <= 250)
        raceController->setLiveCadenceRpm(value);

    // invalid value, show "-" to the user
    if (value == -1 || value > 250) {
        ui->wid_3_infoBoxCadence->setValue(value);
        ui->wid_3_workoutPlot_CadenceZoom->updateTextLabelValue(value);
        if (account->enable_studio_mode) {
            arrUserStudioWidget[userID-1]->setCadenceValue(value);
        }
        return;
    }
    if (value < 0)
        return;


    if (userID == 1 && m_groupRide) m_groupRide->recordCadence(value);

    // Mark pairing as done
    markSensorPaired(cadencePairingDone, labelCadence);
    markSensorPaired(scPairingDone, labelSpeedCadence);


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
        m_secondSamples.addSample(userID-1, SensorSecondAccumulator::Metric::Cadence, value);


        //Update Graph (zoomer)
        ui->wid_3_workoutPlot_CadenceZoom->updateCurve(timeElapsed_sec, value);

    }
    ///Resume Workout?
    if (!account->enable_studio_mode && !isAskingUserQuestion && (account->start_trigger == 0) && isWorkoutPaused && !isWorkoutOver && (value > account->value_cadence_start) ) {
        start_or_pause_workout();
    }


    // Show raw data to the display
    ui->wid_3_infoBoxCadence->setValue(value);
    ui->wid_3_workoutPlot_CadenceZoom->updateTextLabelValue(value);
    // UserStudio
    if (account->enable_studio_mode) {
        arrUserStudioWidget[userID-1]->setCadenceValue(value);
    }
}


void WorkoutDialog::TrainerSpeedDataReceived(int userID, double value) {
    Q_UNUSED(userID);


    // invalid value, show "-" to the user
    if (value == -1) {
        ui->wid_4_infoBoxSpeed->setTrainerSpeed(value);
        return;
    }
    if (value < 0)
        return;

    // Mark pairing as done
    markSensorPaired(speedPairingDone, labelSpeed);
    markSensorPaired(scPairingDone, labelSpeedCadence);

    // The displayed speed is always virtual (see VirtualSpeedDataReceived);
    // a paired sensor's reading is only shown as the optional "trainer speed".
    ui->wid_4_infoBoxSpeed->setTrainerSpeed(value);

}


/// VIRTUAL SPEED, COMING FROM POWER DATA CALCULATED TO SPEED
/// VALUE IS M/S
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


    speedDataChosen(userID, valueKMH);

    //Update Distance Counter
    if (!isWorkoutPaused && !isWorkoutOver) {
        arrDataWorkout[userID-1]->updateDistance(valueMS*timeAtThisSpeedSec);
    }

}


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
        m_secondSamples.addSample(userID-1, SensorSecondAccumulator::Metric::Speed, value);
    }
    ///Resume Workout?
    if (!account->enable_studio_mode && !isAskingUserQuestion && (account->start_trigger == 2) && isWorkoutPaused && !isWorkoutOver && (valueUnit > account->value_speed_start) ) {
        start_or_pause_workout();
    }


    ui->wid_4_infoBoxSpeed->setValue(value);

}


void WorkoutDialog::PowerDataReceived(int userID, int value) {

    // invalid value, show "-" to the user
    if (value == -1) {
        ui->wid_2_infoBoxPower->setValue(value);
        ui->wid_2_workoutPlot_PowerZoom->updateTextLabelValue(value);
        if (account->enable_studio_mode) {
            arrUserStudioWidget[userID-1]->setPowerValue(value);
        }
        return;
    }


    if (value < 0)
        return;

    if (userID == 1 && m_groupRide) m_groupRide->recordPower(value);

    // Drive the player in the retro race with live (solo) power.
    if (raceController && userID == 1)
        raceController->setLivePowerWatts(value);

    // Track for sensor dropout detection (non-studio, user 1 only)
    if (!account->enable_studio_mode && isWorkoutStarted && !isWorkoutOver)
        m_lastPowerMs = QDateTime::currentMSecsSinceEpoch();

    // Send power To Clock
    emit sendPowerData(userID, value);


    // Mark pairing as done
    markSensorPaired(powerPairingDone, labelPower);
    markSensorPaired(fecPairingDone, labelFEC);


    const int powerValue = value;

    if (!isWorkoutPaused && !isWorkoutOver) {

        arrDataWorkout[userID-1]->checkUpdateMaxPower(powerValue);

        ///Below Pause treshold?
        if (!account->enable_studio_mode && (account->start_trigger == 1) && (powerValue < account->value_power_start) ) {
            start_or_pause_workout();
        }

        ///Check to play sound, 10sec cooldown between sound and not first/last 3 second of an interval
        if (!account->enable_studio_mode && currentTargetPower != -1 && soundsActive && account->enable_sound) {
            /// TOO LOW
            if (account->sound_alert_power_under_target && (powerValue < currentTargetPower - currentTargetPowerRange) && soundPowerTooLowActive) {
                soundPlayer->playSoundPowerTooLow();
                soundPowerTooLowActive = false;
                timerCheckReactivateSoundPowerTooLow->start();
            }
            /// TOO HIGH
            else if (account->sound_alert_power_above_target && (powerValue > currentTargetPower + currentTargetPowerRange) && soundPowerTooHighActive) {
                soundPlayer->playSoundPowerTooHigh();
                soundPowerTooHighActive = false;
                timerCheckReactivateSoundPowerTooHigh->start();
            }
        }


        //averaging 1sec
        m_secondSamples.addSample(userID-1, SensorSecondAccumulator::Metric::Power, value);

        //Update Graph (zoomer)
        ui->wid_2_workoutPlot_PowerZoom->updateCurve(timeElapsed_sec, powerValue);


    }
    ///Resume Workout?
    if (!account->enable_studio_mode && !isAskingUserQuestion && (account->start_trigger == 1) && isWorkoutPaused && !isWorkoutOver && (value >= account->value_power_start) ) {
        start_or_pause_workout();
    }


    // Show raw data to the display
    ui->wid_2_infoBoxPower->setValue(powerValue);
    ui->wid_2_workoutPlot_PowerZoom->updateTextLabelValue(powerValue);
    if (account->enable_studio_mode) {
        arrUserStudioWidget[userID-1]->setPowerValue(powerValue);
    }
}


void WorkoutDialog::PowerBalanceDataReceived(int userID, int rightPedalPercentage) {

    if (!powerBalanceDataSeen) {
        powerBalanceDataSeen = true;
        if (!account->enable_studio_mode)
            showPowerBalanceWidget(account->display_power_balance);
    }
    ui->wid_2_balancePower->setValue(rightPedalPercentage);
    m_secondSamples.setRightPedalPercent(userID-1, rightPedalPercentage);
}


void WorkoutDialog::OxygenValueChanged(int userID, double percentageSaturatedHemoglobin, double totalHemoglobinConcentration) { // %, g/d;


    // Mark pairing as done
    markSensorPaired(oxygenPairingDone, labelOxygen);
    ui->wid_oxygen->oxygenValueChanged(percentageSaturatedHemoglobin, totalHemoglobinConcentration);

    m_secondSamples.setHemoglobin(userID-1, percentageSaturatedHemoglobin, totalHemoglobinConcentration);
}


void WorkoutDialog::sendTargetsPower(double percentageTarget, int range) {


    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            arrUserStudioWidget[i]->setTargetPower(percentageTarget, range);
        }
    }
    else {
        ui->wid_2_workoutPlot_PowerZoom->targetChanged(percentageTarget, range);
        ui->wid_2_infoBoxPower->targetChanged(percentageTarget, range);
    }
}


void WorkoutDialog::sendTargetsCadence(int target, int range) {

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            arrUserStudioWidget[i]->setTargetCadence(target, range);
        }
    }
    else {
        ui->wid_3_workoutPlot_CadenceZoom->targetChanged(target, range);
        ui->wid_3_infoBoxCadence->targetChanged(target, range);
    }
}


void WorkoutDialog::sendTargetsHr(double percentageTarget, int range) {

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            arrUserStudioWidget[i]->setTargetHr(percentageTarget, range);
        }
    }
    else {
        ui->wid_1_workoutPlot_HeartrateZoom->targetChanged(percentageTarget, range);
        ui->wid_1_infoBoxHr->targetChanged(percentageTarget, range);
    }
}


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
        if (trainerControlUserId != -1) {
            emit setSlope(trainerControlUserId, slope);
        }
    }

}

// Virtual shifting (#293) — drive a single-cog trainer's resistance from a
// rider-controlled gear instead of sending slope 0 (which spins out).
void WorkoutDialog::enableTrainerControl() {
    trainerControlUserId = 1;       // solo rider; hubs ignore the id
    updateGearIndicator();          // a trainer is now wired → reveal the gear UI
}

// A trainer is under our control (solo, non-studio). Virtual shifting is offered
// here regardless of the ERG ("control trainer resistance") checkbox: with ERG
// off the rider controls resistance *through* the gears.
bool WorkoutDialog::virtualShiftingActive() const {
    return trainerControlUserId != -1 && account
        && account->virtual_shifting          // opt-in; off => real gears / free ride
        && !account->enable_studio_mode;
}

// ERG only owns an interval when the checkbox is on AND there's a positive power
// target (a structured power interval). Then gears are inactive ("ERG").
bool WorkoutDialog::ergOwnsThisInterval() const {
    return account && account->control_trainer_resistance
        && !isUsingSlopeMode && currentTargetPower > 0;
}

// Gears are the active resistance source whenever a trainer is controllable, the
// workout isn't over, and ERG isn't currently owning the interval.
bool WorkoutDialog::gearsDriveNow() const {
    return virtualShiftingActive() && !isWorkoutOver && !ergOwnsThisInterval();
}

int WorkoutDialog::gearTargetWatts(int gear, double cadence) const {
    return VirtualGear::targetWatts(gear, cadence,
                                    (account && account->FTP > 0) ? account->FTP : 0.0);
}

void WorkoutDialog::sendGearLoad() {
    if (!gearsDriveNow())
        return;

    if (m_trainerSupportsResistanceLevel) {
        // Instant, real-gear feel: a fixed brake the rider's power works against.
        emit setResistance(trainerControlUserId, VirtualGear::resistanceLevel(m_virtualGear));
    } else {
        // Fallback for trainers without 0x04: cadence-aware ERG.
        const double cad = m_secondSamples.rider(0).cadence;
        emit setLoad(trainerControlUserId, gearTargetWatts(m_virtualGear, cad));
    }
}

void WorkoutDialog::setupZwiftClick()
{
#ifndef Q_OS_WASM
    if (!account || !account->use_zwift_click)
        return;
    // The right Click v2 is read over its own BLE connection — independent of the
    // trainer, so ERG/FTMS is unaffected. Right-side mapping (left unsupported):
    //   Y → gear up   B → gear down   A → radio next   Z → radio prev
    //   right paddle (+) → start/pause workout
    m_clickManager = new ZwiftClickManager(this);   // freed with the dialog (BLE released)
    connect(m_clickManager, &ZwiftClickManager::shiftUp,   this, [this]{ shiftGear(+1); });
    connect(m_clickManager, &ZwiftClickManager::shiftDown, this, [this]{ shiftGear(-1); });
    connect(m_clickManager, &ZwiftClickManager::radioNext, this, &WorkoutDialog::F8next);
    connect(m_clickManager, &ZwiftClickManager::radioPrev, this, &WorkoutDialog::F6previous);
    connect(m_clickManager, &ZwiftClickManager::startPauseWorkout,
            this, [this]{ start_or_pause_workout(); });
    m_clickManager->start();   // scan + connect; only the right controller's buttons are mapped
#endif
}

void WorkoutDialog::shiftGear(int delta) {
    // During an ERG-owned interval a virtual gear has no effect, so the gear-up/
    // gear-down controls (Zwift Click buttons, the toolbar ▲/▼, or the Up/Down
    // keys) instead nudge the whole-workout difficulty - the same action as the
    // +/- graph buttons. In slope/free-ride they keep changing the virtual gear.
    if (ergOwnsThisInterval()) {
        if (delta > 0)      emit increaseDifficulty();
        else if (delta < 0) emit decreaseDifficulty();
        return;
    }
    // Ignore shifts unless gears are actually driving resistance — with virtual
    // shifting off / workout over the gear is not in control, so changing the
    // number would just be misleading.
    if (!gearsDriveNow())
        return;
    const int g = qBound(1, m_virtualGear + delta, kVirtualGearCount);
    if (g == m_virtualGear)
        return;
    m_virtualGear = g;
    sendGearLoad();
    updateGearIndicator();
    ui->widget_topMenu->flashShift(delta);   // confirm the shift on the toolbar
}

// Refresh the top-bar gear indicator. It's a virtual-gear concept, so it's shown
// only when gears actually drive resistance: a trainer is controllable (not in
// studio) AND we're not in an ERG-owned interval. In ERG the gear means nothing,
// so the whole indicator is hidden (the gear up/down controls re-task to
// difficulty there - see shiftGear()).
void WorkoutDialog::updateGearIndicator() {
    const bool show = virtualShiftingActive() && !ergOwnsThisInterval();
    ui->widget_topMenu->setGearVisible(show);
    if (show)
        ui->widget_topMenu->updateGear(m_virtualGear, kVirtualGearCount);
}

void WorkoutDialog::sendLoads(double percentageFTP) {

    // Studio mode: no smoothing — each rider has a different FTP, broadcast directly.
    if (account->enable_studio_mode) {
        stopErgSmoothing();
        for (int i = 0; i < account->nb_user_studio; i++) {
            UserStudio myUserStudio = vecUserStudio.at(i);
            if (myUserStudio.getFecID() > 0 && myUserStudio.getFTP() > 0) {
                int userTarget = qRound(percentageFTP * myUserStudio.getFTP());
                emit setLoad(myUserStudio.getFecID(), userTarget);
            }
        }
        return;
    }

    if (trainerControlUserId == -1) {
        stopErgSmoothing();
        return;
    }

    // Compute the new target in watts from the percentage and current FTP.
    const double targetWatts = (account->FTP > 0) ? qRound(percentageFTP * account->FTP) : 0.0;

    // If smoothing is disabled or target is zero, send immediately.
    if (account->erg_smoothing_duration_s <= 0 || targetWatts <= 0) {
        stopErgSmoothing();
        m_ergSmoothLast = targetWatts;
        emit setLoad(trainerControlUserId, qRound(targetWatts));
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
    m_ergSmoothDeviceId = trainerControlUserId;

    if (!m_ergSmoothTimer) {
        m_ergSmoothTimer = new QTimer(this);
        m_ergSmoothTimer->setInterval(1000);
        connect(m_ergSmoothTimer, &QTimer::timeout, this, &WorkoutDialog::ergSmoothStep);
    }

    // Emit step 0 immediately (the "from" value to start the transition).
    const int startWatts = qRound(fromWatts);
    m_ergSmoothLast = startWatts;
    emit setLoad(m_ergSmoothDeviceId, startWatts);

    m_ergSmoothTimer->start();
}

void WorkoutDialog::stopErgSmoothing()
{
    if (m_ergSmoothTimer)
        m_ergSmoothTimer->stop();
    m_ergSmoothStep  = 0;
    m_ergSmoothSteps = 0;
    // Do NOT clear m_ergSmoothLast — it is the "current position" for mid-ramp retargeting.
    m_ergSmoothDeviceId = -1;
}

void WorkoutDialog::ergSmoothStep()
{
    if (m_ergSmoothDeviceId == -1 || m_ergSmoothSteps <= 0) {
        stopErgSmoothing();
        return;
    }

    m_ergSmoothStep++;
    const double progress = static_cast<double>(m_ergSmoothStep) / static_cast<double>(m_ergSmoothSteps);
    const double watts    = m_ergSmoothFrom + (m_ergSmoothTo - m_ergSmoothFrom) * qMin(progress, 1.0);
    const int rounded     = qRound(watts);

    m_ergSmoothLast = rounded;
    emit setLoad(m_ergSmoothDeviceId, rounded);

    if (m_ergSmoothStep >= m_ergSmoothSteps)
        stopErgSmoothing();
}


void WorkoutDialog::targetPowerChanged_f(double percentageTarget, int range) {


    if (percentageTarget <= 0 || isUsingSlopeMode || !account->control_trainer_resistance) {
        // Single-cog trainers spin out at slope 0 (#293) — drive the virtual
        // gear's resistance instead when trainer control is available.
        if (virtualShiftingActive())
            sendGearLoad();
        else
            sendSlopes(0);
    }
    else if(account->control_trainer_resistance) {
        sendLoads(percentageTarget);
    }


    currentTargetPower = qRound(percentageTarget * account->FTP);
    currentTargetPowerRange =  range;
    // Keep the workout pacer on the current interval target (rest → soft-pedal
    // at ~45% FTP so it keeps rolling rather than stopping dead).
    if (raceController) {
        raceController->setPacerTargetWatts(currentTargetPower > 0
                                            ? currentTargetPower
                                            : qRound(0.45 * account->FTP));
        // Drive the game's power target indicator (same threshold as the alerts).
        raceController->setTargetPower(currentTargetPower, currentTargetPowerRange);
        // Keep the team-race (draft/leash) in step with the current ERG/slope mode.
        raceController->setErgMode(!isUsingSlopeMode && account->control_trainer_resistance);
    }
    ui->widget_topMenu->setTargetPower(percentageTarget, range);
    sendTargetsPower(percentageTarget, range);

    // Reflect the new interval in the gear indicator (active vs dimmed "ERG").
    updateGearIndicator();
}


void WorkoutDialog::targetCadenceChanged_f(int target, int range) {


    currentTargetCadence = target;
    currentTargetCadenceRange = range;
    ui->widget_topMenu->setTargetCadence(target, range);

    sendTargetsCadence(target, range);
    if (raceController) raceController->setTargetCadence(target, range);
}

void WorkoutDialog::targetHrChanged_f(double percentageTarget, int range) {

    ui->widget_topMenu->setTargetHeartRate(percentageTarget, range);

    sendTargetsHr(percentageTarget, range);
    // Convert the %LTHR target to bpm for the game's HR indicator.
    if (raceController) {
        const double bpm = (percentageTarget > 0 && account->LTHR > 0)
                         ? qRound(percentageTarget * account->LTHR) : -1.0;
        raceController->setTargetHr(bpm, range);
    }
}


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

void WorkoutDialog::showTimerIntervalRemaining(bool show) {

    ui->widget_topMenu->showIntervalRemaining(show);
}

void WorkoutDialog::showTimerWorkoutRemaining(bool show) {

    ui->widget_topMenu->showWorkoutRemaining(show);
}

void WorkoutDialog::showTimerWorkoutElapsed(bool show) {

    ui->widget_topMenu->showWorkoutElapsed(show);
}

void WorkoutDialog::showTimerCurrentTarget(bool show) {

    ui->widget_topMenu->showCurrentTarget(show);
}

// Hide the bottom-left time widget entirely when it's on the top bar or when no
// timer element is enabled — otherwise it reserves an empty box bottom-left.

WebBrowserView *WorkoutDialog::ensureWebPlayer() {
    if (!webPlayer) {
        webPlayer = new WebBrowserView(ui->widget_webPlayer);
        ui->widget_webPlayer->layout()->addWidget(webPlayer);
    }
    return webPlayer;
}


WorkoutRaceView *WorkoutDialog::ensureRaceView() {
    if (!m_raceView) {
        WorkoutRaceView::Setup setup;
        setup.riderMassKg = account->bike_weight_kg + account->weight_kg;
        setup.cdA = account->userCda;
        setup.ergMode = !isUsingSlopeMode && account->control_trainer_resistance;
        setup.intervalMessage = currentIntervalObj.getDisplayMessage();
        m_raceView = new WorkoutRaceView(workout, setup, ui->widgetVideo->parentWidget()->layout(),
                                         {isWorkoutStarted, isWorkoutPaused, isWorkoutOver}, this);
        raceController = m_raceView->controller();
        raceView = m_raceView->view();
        // Seed the workout pacer's target the moment the user picks it.
        connect(raceController, &RetroRaceController::opponentChanged, this, [this]() {
            if (currentTargetPower > 0)
                raceController->setPacerTargetWatts(currentTargetPower);
        });
        // QML fullscreen button → collapse/restore the graph + widget panes.
        connect(raceController, &RetroRaceController::fullscreenToggleRequested,
                this, &WorkoutDialog::onToggleGameFullscreen);
    }
    return m_raceView;
}

void WorkoutDialog::onToggleGameFullscreen() {
    QList<int> sizes = ui->splitter->sizes();   // [0]=toolbar [1]=content [2]=graph [3]=widgets
    if (sizes.size() < 4)
        return;
    if (!m_gameFullscreen) {
        m_savedSplitterSizes = sizes;
        int total = 0;
        for (int s : sizes) total += s;
        sizes[2] = 0;                            // collapse interval graph
        sizes[3] = 0;                            // collapse bottom widgets
        sizes[1] = total - sizes[0];             // content (game) takes the rest
        ui->splitter->setSizes(sizes);
        m_gameFullscreen = true;
    } else {
        if (!m_savedSplitterSizes.isEmpty())
            ui->splitter->setSizes(m_savedSplitterSizes);
        m_gameFullscreen = false;
    }
    if (raceController)
        raceController->setGameFullscreen(m_gameFullscreen);
}

/// 0 = Standard video, 1 = Web Browser, 2 = Game (retro race).
// The web player is built in the background (see showEvent); its page loads the
// first time the Web Browser view is shown.
void WorkoutDialog::showVideoPlayer(int choice) {

    // The game (retro race) is unavailable in multi-rider studio mode (no single
    // "player") and in free ride (no targets/finish — it doesn't make sense yet):
    // fall back to video in both cases.
    if ((account->enable_studio_mode
         || workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) && choice == 2)
        choice = 0;

    if (raceView) raceView->setVisible(false);
    if (raceController) raceController->setVisualActive(false);

    /// Game (retro race) — occupies the video slot.
    if (choice == 2) {
        if (webPlayer) webPlayer->pauseVideo();
        ui->widget_webPlayer->setVisible(false);
        ui->widgetVideo->setVisible(false);
        WorkoutRaceView *race = ensureRaceView();
        race->view()->setVisible(true);
        race->controller()->setVisualActive(true);
        return;
    }

    /// Standard
    if (choice == 0) {
        // Hiding the view stops rendering but not playback, so pause it too or
        // its audio keeps playing behind the VLC player.
        if (webPlayer) webPlayer->pauseVideo();
        ui->widget_webPlayer->setVisible(false);
        ui->widgetVideo->setVisible(true);
    }
    /// WebView
    else {
        ui->widgetVideo->setVisible(false);
        ui->widget_webPlayer->setVisible(true);
        if (webPlayer) webPlayer->loadHomePageIfNeeded();
    }
}


///1 = Detailed
///2 = Graph
///3 = Graph & Detailed (power only)
void WorkoutDialog::showHeartRateDisplayWidget(int display) {
    if (account->show_hr_widget) {
        if (display == 1) { /// Detailed
            ui->wid_1_workoutPlot_HeartrateZoom->setVisible(false);
            ui->wid_1_infoBoxHr->setVisible(true);
        }
        else if (display == 2) { /// Graph
            ui->wid_1_workoutPlot_HeartrateZoom->setVisible(true);
            ui->wid_1_infoBoxHr->setVisible(false);
        }
    }
    else { /// Hide
        ui->wid_1_workoutPlot_HeartrateZoom->setVisible(false);
        ui->wid_1_infoBoxHr->setVisible(false);
    }
}
void WorkoutDialog::showPowerDisplayWidget(int display) {
    if (account->show_power_widget) {
        if (display == 1) { /// Detailed
            ui->wid_2_workoutPlot_PowerZoom->setVisible(false);
            ui->wid_2_infoBoxPower->setVisible(true);
        }
        else if (display == 2) { /// Graph
            ui->wid_2_workoutPlot_PowerZoom->setVisible(true);
            ui->wid_2_infoBoxPower->setVisible(false);
        }
        else if (display == 3) { ///Graph and Detailed
            ui->wid_2_workoutPlot_PowerZoom->setVisible(true);
            ui->wid_2_infoBoxPower->setVisible(true);
        }
    }
    else { /// Hide
        ui->wid_2_workoutPlot_PowerZoom->setVisible(false);
        ui->wid_2_infoBoxPower->setVisible(false);
    }
}
void WorkoutDialog::showCadenceDisplayWidget(int display) {
    if (account->show_cadence_widget) {
        if (display == 1) { /// Detailed
            ui->wid_3_workoutPlot_CadenceZoom->setVisible(false);
            ui->wid_3_infoBoxCadence->setVisible(true);
        }
        else if (display == 2) { /// Graph
            ui->wid_3_workoutPlot_CadenceZoom->setVisible(true);
            ui->wid_3_infoBoxCadence->setVisible(false);
        }
    }
    else { /// Hide
        ui->wid_3_workoutPlot_CadenceZoom->setVisible(false);
        ui->wid_3_infoBoxCadence->setVisible(false);
    }
}
void WorkoutDialog::showSpeedDisplayWidget() {
    if (account->show_speed_widget) {
        ui->wid_4_infoBoxSpeed->setVisible(true);
    }
    else { /// Hide
        ui->wid_4_infoBoxSpeed->setVisible(false);
    }
}


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

void WorkoutDialog::showTrainerSpeed(bool show) {

    ui->wid_4_infoBoxSpeed->setTrainerSpeedVisible(show);
}

void WorkoutDialog::showCaloriesDisplayWidget() {

    if (account->show_calories_widget) {
        ui->wid_5_infoWorkout->setVisible(true);
    }
    else { /// Hide
        ui->wid_5_infoWorkout->setVisible(false);
    }
}

void WorkoutDialog::showOxygenDisplayWidget() {

    if (account->show_oxygen_widget) {
        ui->wid_oxygen->setVisible(true);
    }
    else { /// Hide
        ui->wid_oxygen->setVisible(false);
    }
}


void WorkoutDialog::showPowerBalanceWidget(int display) {

    if (account->show_power_balance_widget) {
        if (display == 0) {  /// Show once a balance-capable meter reports, never as an empty widget
            ui->wid_2_balancePower->setVisible(powerBalanceDataSeen);
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


void WorkoutDialog::setMessagePlot() {

    if (!isWorkoutStarted && m_groupRide && m_groupRide->awaitingStart()) {
        QString groupMessage;
        if (m_groupRide->scheduledStartLocalMs() > 0) {
            const qint64 remainingMs = m_groupRide->scheduledStartLocalMs() - QDateTime::currentMSecsSinceEpoch();
            groupMessage = tr("Group ride starts in %1 s").arg((qMax<qint64>(0, remainingMs) + 999) / 1000);
        } else if (m_groupRide->isHost()) {
            groupMessage = tr("When everyone is ready, press Enter or Start group ride");
        } else {
            groupMessage = tr("Waiting for the host to start the group ride");
        }
        ui->widget_workoutPlot->updateMessageText(groupMessage);
        return;
    }

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


void WorkoutDialog::setWidgetsStopped(bool b) {


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
    }
}


void WorkoutDialog::achievementReceived(Achievement achievement) {


    //// Add achievement to Queue (for in-workout animated overlay)
    queueAchievement.enqueue(achievement);

    // Also collect for the post-workout summary panel
    earnedAchievements.append(achievement);

    if (!achievementCurrentlyPlaying) {
        animateAchievement();
    }
}

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


void WorkoutDialog::closeWindow() {

    sureYouWantToQuit();
}

void WorkoutDialog::expandWindow() {


    if (this->isFullScreen()) {
        showNormalWin();
    }
    else {
        showFullScreenWin();
    }
}


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
        ui->widget_workoutPlot->adjustDifficultyBy(5);
        return;
    case Qt::Key_Minus:
        ui->widget_workoutPlot->adjustDifficultyBy(-5);
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
            auto *dlg = new DialogKeyboardShortcuts(this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->open();
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

void WorkoutDialog::showEvent(QShowEvent *event) {

    QDialog::showEvent(event);

    // Build and load the web player once, in the background, just after the
    // dialog is mapped — while its container is still hidden, so the native
    // window is realized invisibly and switching to it later never flickers the
    // dialog closed/reopen. Deferred to the next iteration so the toplevel is
    // fully mapped first.
    if (!webPlayerLoaded) {
        webPlayerLoaded = true;
        QTimer::singleShot(0, this, [this]() {
            // Build the web player now, but load its page only once the Web
            // Browser view is picked: an idle view starts no Chromium renderer.
            ensureWebPlayer();
            // Build the retro-race view now too, for the same reason: its native
            // OpenGL surface is realized once here while it can stay hidden, so
            // switching to it mid-workout never tears down and rebuilds the dialog
            // window (which looked like the workout reopening). Skip where the race
            // isn't offered (studio / free ride) — there's no single "player".
            if (!(account->enable_studio_mode
                  || workout.getWorkoutNameEnum() == Workout::OPEN_RIDE))
                ensureRaceView();
            // Re-assert the selected content view now that the dialog is mapped.
            // The native QVideoWidget (Standard video) does not paint when first
            // shown pre-map during init, leaving a black pane until the dropdown
            // is toggled; re-applying here fixes the first-load black screen.
            showVideoPlayer(account->display_video);
        });
    }
}

void WorkoutDialog::reject() {

    sureYouWantToQuit();
}


void WorkoutDialog::showConfig() {
    emit insideConfig(true);
    dconfig->show();
    dconfig->raise();
    dconfig->activateWindow();
    // insideConfig(false) is emitted via the QDialog::finished connection set up in setupUi
}


void WorkoutDialog::sureYouWantToQuit() {

    saveInterface();

    if (!isWorkoutPaused) {
        start_or_pause_workout();
    }

    if (isWorkoutStarted && !isWorkoutOver) {

        isAskingUserQuestion = true;
        // Async 3-way confirm (exec() is fatal on WASM); accept() runs from
        // the button callback instead of after a blocking return.
        auto *msgBox = new QMessageBox(QMessageBox::Question, QString(),
                                       tr("Workout is not completed."),
                                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, this);
        msgBox->setInformativeText(tr("Save your progress?"));
        msgBox->setAttribute(Qt::WA_DeleteOnClose);
        connect(msgBox, &QDialog::finished, this, [this](int) { isAskingUserQuestion = false; });
        connect(msgBox, &QMessageBox::buttonClicked, this,
                [this, msgBox](QAbstractButton *button) {
            const QMessageBox::StandardButton reply = msgBox->standardButton(button);
            if (reply == QMessageBox::Yes) {

                // Save FIT FILE
                if (Util::getSystemPathHistory() != "invalid_writable_path") {
                    int intervalPausedTime_msec = totalTimePausedWorkout_msec - lastIntervalTotalTimePausedWorkout_msec;
                    changeIntervalsDataWorkout(lastIntervalEndTime_msec, timeElapsed_sec, intervalPausedTime_msec, true, false);
                    m_quitAfterSave = true;  // skip post-workout panel — dialog closes immediately after
                    closeFitFiles(timeElapsed_sec);
                }

                QDialog::accept();
            }
            else if (reply == QMessageBox::No) {
                closeAndDeleteFitFile();
                QDialog::accept();
            }
        });
        msgBox->open();
    }
    /// Not started or workout completed, no warning to show
    else {
        QDialog::accept();
    }

}


void WorkoutDialog::loadInterface() {

    namespace Keys = SettingKeys::WorkoutDialog;
    const QSettings settings;

    resize(AppSettings::read(settings, Keys::Size));
    move(AppSettings::read(settings, Keys::Pos));

    ui->splitter->setSizes({AppSettings::read(settings, Keys::Splitter0),
                            AppSettings::read(settings, Keys::Splitter1),
                            AppSettings::read(settings, Keys::Splitter2),
                            AppSettings::read(settings, Keys::Splitter3)});

    const bool wasFullscreen = AppSettings::read(settings, Keys::Fullscreen);

    if (wasFullscreen) {
        wasFullscreenOnOpen = true;
        showFullScreenWin();
    }
    else {
        wasFullscreenOnOpen = false;
        isFullScreenFlag = false;
        this->setSizeGripEnabled(true);
        ui->widget_topMenu->updateExpandIcon();
    }

}
void WorkoutDialog::saveInterface() {

    namespace Keys = SettingKeys::WorkoutDialog;
    QSettings settings;
    AppSettings::write(settings, Keys::Size, size());
    AppSettings::write(settings, Keys::Pos, pos());

    const QList<int> sizeNow = ui->splitter->sizes();
    AppSettings::write(settings, Keys::Splitter0, sizeNow.at(0));
    AppSettings::write(settings, Keys::Splitter1, sizeNow.at(1));
    AppSettings::write(settings, Keys::Splitter2, sizeNow.at(2));
    AppSettings::write(settings, Keys::Splitter3, sizeNow.at(3));
    AppSettings::write(settings, Keys::Fullscreen, isFullScreen());
}


void WorkoutDialog::ignoreClickPlot() {
    bignoreClickPlot = false;
    timerIgnoreClickPlot->stop();
}


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


void WorkoutDialog::checkFitFileCreated() {

    QString fitFilename = arrDataWorkout[0]->getFitFilename();
    qInfo() << "FIT file:" << fitFilename;
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

void WorkoutDialog::showPostWorkoutPanel()
{
    if (widgetPostWorkout) return;  // already shown

    // Parent to the whole dialog (not the bottom widget pane) so the panel is
    // visible even when the data panes are collapsed (e.g. fullscreen game).
    widgetPostWorkout = new PostWorkoutPanel(account, earnedAchievements,
                                             fitFilePath, fitFileName, fitFileDescription, this);
    const QSize ps = size();   // centre over the whole workout dialog
    const QSize ws = widgetPostWorkout->size();
    widgetPostWorkout->move((ps.width()  - ws.width())  / 2,
                            (ps.height() - ws.height()) / 2);
    widgetPostWorkout->show();
    widgetPostWorkout->raise();
}

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
            arrDataWorkout[i]->changeInterval(timeStarted, timeNow, timePaused_msec, workoutOver, testInterval);
        }
    }
    else {
        arrDataWorkout[0]->changeInterval(timeStarted, timeNow, timePaused_msec, workoutOver, testInterval);
    }

    if (showSummary)
        showIntervalSummaryOverlay(summaryAvgPower, summaryAvgHr, summaryAvgCad,
                                   static_cast<int>(intervalDuration), targetPowerFraction);

    lastIntervalEndTime_msec = timeElapsed_sec;
    lastIntervalTotalTimePausedWorkout_msec = totalTimePausedWorkout_msec;
}


void WorkoutDialog::initDataWorkout() {

    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            arrDataWorkout[i] = new DataWorkout(this->workout, account->FTP, this);
        }
    }
    else {
        arrDataWorkout[0] = new DataWorkout(this->workout, account->FTP, this);
        // Forward whole-ride metrics to the race game.
        connect(arrDataWorkout[0], &DataWorkout::normalizedPowerChanged, this,
                [this](double v) { if (raceController) raceController->setNp(v); });
        connect(arrDataWorkout[0], &DataWorkout::intensityFactorChanged, this,
                [this](double v) { if (raceController) raceController->setIf(v); });
        connect(arrDataWorkout[0], &DataWorkout::tssChanged, this,
                [this](double v) { if (raceController) raceController->setTss(v); });
    }
}


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


void WorkoutDialog::createUserStudioWidget() {


    // Rider boxes are laid out in a balanced, screen-adaptive grid. Each box
    // stretches to fill its column (growing on wide displays, shrinking toward
    // minBoxWidth on small ones) while keeping a fixed height so rows align.
    // Columns are capped at maxPerRow and balanced across the rows needed, so
    // e.g. 15 riders → 5+5+5 and 10 → 5+5 rather than a full row plus a stub.
    const int minBoxWidth = 290;
    const int boxHeight    = 190;
    const int maxPerRow    = 5;

    const int nbRiders = account->nb_user_studio;

    if (account->enable_studio_mode && nbRiders > 0) {
        const int availWidth   = QGuiApplication::primaryScreen()->availableGeometry().width();
        const int colsThatFit  = qMax(1, availWidth / minBoxWidth);
        const int cap          = qMin(maxPerRow, colsThatFit);
        const int rowsNeeded   = (nbRiders + cap - 1) / cap;
        const int columns      = qMax(1, (nbRiders + rowsNeeded - 1) / rowsNeeded);

        QGridLayout *glayout = static_cast<QGridLayout*>(ui->widget_allSpeedo->layout());
        // Gap between boxes so each rider's outline reads as a separate card.
        glayout->setHorizontalSpacing(10);
        glayout->setVerticalSpacing(10);

        for (int i = 0; i < nbRiders; i++) {
            UserStudio myUserStudio = vecUserStudio.at(i);
            // Fall back to "RiderN" when the rider left their name blank in the
            // Studio tab, so every box is still labelled in the workout view.
            QString riderName = myUserStudio.getDisplayName().trimmed();
            if (riderName.isEmpty())
                riderName = tr("Rider%1").arg(i+1);

            arrUserStudioWidget[i] = new UserStudioWidget(i+1, riderName, myUserStudio.getFTP(), myUserStudio.getLTHR(), this);
            arrUserStudioWidget[i]->setMinimumSize(minBoxWidth, boxHeight);
            arrUserStudioWidget[i]->setMaximumHeight(boxHeight);
            arrUserStudioWidget[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

            if (myUserStudio.getHrID() < 1 && myUserStudio.getFecID() < 1)
                arrUserStudioWidget[i]->setHrSectionHidden();
            if (myUserStudio.getPowerID() < 1 && myUserStudio.getFecID() < 1)
                arrUserStudioWidget[i]->setPowerSectionHidden();

            glayout->addWidget(arrUserStudioWidget[i], i / columns, i % columns);
        }
        for (int c = 0; c < columns; c++)
            glayout->setColumnStretch(c, 1);

        //Set back theses widgets on top of GridStudio
        widgetLoading->raise();
        widgetLoading->activateWindow();

        widgetBattery->raise();
        widgetBattery->activateWindow();
    }
}


void WorkoutDialog::showTestResult() {

    // May already have been shown when the last test interval finished.
    if (testResultShown)
        return;
    testResultShown = true;

    // ------------- FTP TEST -----------------
    if (workout.getWorkoutNameEnum() == Workout::FTP_TEST || workout.getWorkoutNameEnum() == Workout::FTP8min_TEST) {

        // GROUP — update every rider's FTP/LTHR from their own test result.
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
            }
            // Emit once with the fully-updated vector; MainWindow persists it so
            // the new values show in the Studio tab and survive a restart.
            emit ftpUserStudioChanged(vecUserStudio);
        }
        //SOLO
        else {
            int newFTP = arrDataWorkout[0]->getFTP();
            int newLTHR = arrDataWorkout[0]->getLTHR();
            mainPlot->setAlertMessage(true, false, workout.getName() + tr(" Result")
                                      + "<div style='color:white;height:7px;'>------------------------------------</div><br/> "
                                      + tr("FTP: ") + QString::number(newFTP) + tr(" watts") + tr(" (Previous: ") +  QString::number(account->FTP) + tr(" watts)") + "<br/>"
                                      + tr("LTHR: ")  + QString::number(newLTHR) + tr(" bpm") + tr(" (Previous: ") +  QString::number(account->LTHR) + tr(" bpm)"), 500);
            // Persist the new FTP/LTHR into the athlete profile so they survive
            // a restart and show up in Preferences. Keep the previous value for
            // any metric the test could not compute (<= 0).
            const int ftpToSave  = (newFTP  > 0) ? newFTP  : account->FTP;
            const int lthrToSave = (newLTHR > 0) ? newLTHR : account->LTHR;
            account->saveProfileFields(ftpToSave, lthrToSave, account->weight_kg);
            emit ftp_lthr_changed();
        }
    }

    else {
        mainPlot->setAlertMessage(true, false, workout.getName() + tr(" completed!")
                                  + "<div style='color:white;height:7px;'>------------------------------------</div><br/> "
                                  + tr("Nice work! "), 500);
    }


}


void WorkoutDialog::sendUserInfoToClock() {


    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            emit sendUserInfo(i+1, account->userCda, account->bike_weight_kg + account->weight_kg, account->nb_user_studio);
        }
    }
    else {
        emit sendUserInfo(1, account->userCda, account->bike_weight_kg + account->weight_kg, 1);
    }
}


void WorkoutDialog::connectDataWorkout() {


    if (account->enable_studio_mode) {
        for (int i=0; i<account->nb_user_studio; i++) {
            connect(arrDataWorkout[i], &DataWorkout::maxPowerIntervalChanged, arrUserStudioWidget[i], &UserStudioWidget::setMaxIntervalPower);
            connect(arrDataWorkout[i], &DataWorkout::maxPowerWorkoutChanged, arrUserStudioWidget[i], &UserStudioWidget::setMaxWorkoutPower);
            connect(arrDataWorkout[i], &DataWorkout::avgPowerIntervalChanged, arrUserStudioWidget[i], &UserStudioWidget::setAvgIntervalPower);
            connect(arrDataWorkout[i], &DataWorkout::avgPowerWorkoutChanged, arrUserStudioWidget[i], &UserStudioWidget::setAvgWorkoutPower);

            connect(arrDataWorkout[i], &DataWorkout::maxCadenceIntervalChanged, arrUserStudioWidget[i], &UserStudioWidget::setMaxIntervalCad);
            connect(arrDataWorkout[i], &DataWorkout::maxCadenceWorkoutChanged, arrUserStudioWidget[i], &UserStudioWidget::setMaxWorkoutCad);
            connect(arrDataWorkout[i], &DataWorkout::avgCadenceIntervalChanged, arrUserStudioWidget[i], &UserStudioWidget::setAvgIntervalCad);
            connect(arrDataWorkout[i], &DataWorkout::avgCadenceWorkoutChanged, arrUserStudioWidget[i], &UserStudioWidget::setAvgWorkoutCad);

            connect(arrDataWorkout[i], &DataWorkout::maxHrIntervalChanged, arrUserStudioWidget[i], &UserStudioWidget::setMaxIntervalHr);
            connect(arrDataWorkout[i], &DataWorkout::maxHrWorkoutChanged, arrUserStudioWidget[i], &UserStudioWidget::setMaxWorkoutHr);
            connect(arrDataWorkout[i], &DataWorkout::avgHrIntervalChanged, arrUserStudioWidget[i], &UserStudioWidget::setAvgIntervalHr);
            connect(arrDataWorkout[i], &DataWorkout::avgHrWorkoutChanged, arrUserStudioWidget[i], &UserStudioWidget::setAvgWorkoutHr);

            connect(arrDataWorkout[i], &DataWorkout::normalizedPowerChanged, arrUserStudioWidget[i], &UserStudioWidget::setNormalizedPower);
            connect(arrDataWorkout[i], &DataWorkout::intensityFactorChanged, arrUserStudioWidget[i], &UserStudioWidget::setIntensityFactor);
            connect(arrDataWorkout[i], &DataWorkout::tssChanged, arrUserStudioWidget[i], &UserStudioWidget::setTrainingStressScore);
            connect(arrDataWorkout[i], &DataWorkout::caloriesWorkoutChanged, arrUserStudioWidget[i], &UserStudioWidget::setCalories);


        }
    }

    connect(arrDataWorkout[0], &DataWorkout::maxHrIntervalChanged, ui->wid_1_infoBoxHr, &InfoWidget::maxIntervalChanged);
    connect(arrDataWorkout[0], &DataWorkout::maxHrWorkoutChanged, ui->wid_1_infoBoxHr, &InfoWidget::maxWorkoutChanged);
    connect(arrDataWorkout[0], &DataWorkout::avgHrIntervalChanged, ui->wid_1_infoBoxHr, &InfoWidget::avgIntervalChanged);
    connect(arrDataWorkout[0], &DataWorkout::avgHrWorkoutChanged, ui->wid_1_infoBoxHr, &InfoWidget::avgWorkoutChanged);

    connect(arrDataWorkout[0], &DataWorkout::maxCadenceIntervalChanged, ui->wid_3_infoBoxCadence, &InfoWidget::maxIntervalChanged);
    connect(arrDataWorkout[0], &DataWorkout::maxCadenceWorkoutChanged, ui->wid_3_infoBoxCadence, &InfoWidget::maxWorkoutChanged);
    connect(arrDataWorkout[0], &DataWorkout::avgCadenceIntervalChanged, ui->wid_3_infoBoxCadence, &InfoWidget::avgIntervalChanged);
    connect(arrDataWorkout[0], &DataWorkout::avgCadenceWorkoutChanged, ui->wid_3_infoBoxCadence, &InfoWidget::avgWorkoutChanged);

    connect(arrDataWorkout[0], &DataWorkout::maxSpeedIntervalChanged, ui->wid_4_infoBoxSpeed, &InfoWidget::maxIntervalChanged);
    connect(arrDataWorkout[0], &DataWorkout::maxSpeedWorkoutChanged, ui->wid_4_infoBoxSpeed, &InfoWidget::maxWorkoutChanged);
    connect(arrDataWorkout[0], &DataWorkout::avgSpeedIntervalChanged, ui->wid_4_infoBoxSpeed, &InfoWidget::avgIntervalChanged);
    connect(arrDataWorkout[0], &DataWorkout::avgSpeedWorkoutChanged, ui->wid_4_infoBoxSpeed, &InfoWidget::avgWorkoutChanged);

    connect(arrDataWorkout[0], &DataWorkout::maxPowerIntervalChanged, ui->wid_2_infoBoxPower, &InfoWidget::maxIntervalChanged);
    connect(arrDataWorkout[0], &DataWorkout::maxPowerWorkoutChanged, ui->wid_2_infoBoxPower, &InfoWidget::maxWorkoutChanged);
    connect(arrDataWorkout[0], &DataWorkout::avgPowerIntervalChanged, ui->wid_2_infoBoxPower, &InfoWidget::avgIntervalChanged);
    connect(arrDataWorkout[0], &DataWorkout::avgPowerWorkoutChanged, ui->wid_2_infoBoxPower, &InfoWidget::avgWorkoutChanged);

    connect(arrDataWorkout[0], &DataWorkout::normalizedPowerChanged, ui->wid_5_infoWorkout, &infosWorkout::NP_Changed);
    connect(arrDataWorkout[0], &DataWorkout::intensityFactorChanged, ui->wid_5_infoWorkout, &infosWorkout::IF_Changed);
    connect(arrDataWorkout[0], &DataWorkout::tssChanged, ui->wid_5_infoWorkout, &infosWorkout::TSS_Changed);
    connect(arrDataWorkout[0], &DataWorkout::caloriesWorkoutChanged, ui->wid_5_infoWorkout, &infosWorkout::calories_Changed);
    connect(arrDataWorkout[0], &DataWorkout::totalDistanceChanged, ui->wid_5_infoWorkout, &infosWorkout::distanceChanged);
    // Mirror the workout's real ride distance into the race HUD so the game's
    // DIST readout matches the session widget (the race keeps its own internal
    // power-model distance only for the player-vs-opponent gap).
    connect(arrDataWorkout[0], &DataWorkout::totalDistanceChanged, this,
            [this](double meters) { if (raceController) raceController->setDisplayDistanceM(meters); });


}

bool WorkoutDialog::eventFilter(QObject *watched, QEvent *event) {

    Q_UNUSED(watched);


    // Only act while this workout window is the active one (an app-level filter
    // otherwise sees every key for every window). isActiveWindow() stays true
    // even when a child button holds focus, but goes false for modal dialogs.
    if(event->type() == QEvent::KeyPress && isActiveWindow()) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if(keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return ) {
            start_or_pause_workout();
            return true; // mark the event as handled
        }
        else if (keyEvent->key() == Qt::Key_Up) {
            // Virtual shifting takes the arrows when enabled; otherwise they keep
            // their legacy role of nudging workout difficulty.
            if (virtualShiftingActive()) shiftGear(+1);
            else emit increaseDifficulty();
            return true;
        }
        else if (keyEvent->key() == Qt::Key_Down) {
            if (virtualShiftingActive()) shiftGear(-1);
            else emit decreaseDifficulty();
            return true;
        }
        else if (keyEvent->key() == Qt::Key_Backspace) {
            lapButtonPressed();
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


void WorkoutDialog::showFullScreenWin() {

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

void WorkoutDialog::showNormalWin() {


#ifdef Q_OS_MAC
    this->showNormal();
#else
    this->showNormal();
#endif

    this->setSizeGripEnabled(true);
    ui->widget_topMenu->setMinExpandExitVisible(true);
    ui->widget_topMenu->updateExpandIcon();

    isFullScreenFlag = false;

}



void WorkoutDialog::setGroupSession(GroupSession *session) {
    if (!session) return;

    m_groupPanel = new GroupPanel(session, this);
    ui->gridLayout->addWidget(m_groupPanel, 0, 1);

    m_groupRide = new GroupRideCoordinator(session, [this]() {
        GroupRideCoordinator::RideSnapshot ride;
        ride.started = isWorkoutStarted;
        ride.paused = isWorkoutPaused;
        ride.over = isWorkoutOver;
        ride.elapsedSeconds = timeElapsed_sec;
        ride.intervalIndex = currentInterval;
        ride.targetPowerWatts = currentTargetPower;
        ride.ftpWatts = account->FTP;
        return ride;
    }, this);
    connect(m_groupPanel, &GroupPanel::startRequested, m_groupRide, &GroupRideCoordinator::requestStart);
    connect(m_groupRide, &GroupRideCoordinator::startScheduled, m_groupPanel, &GroupPanel::setScheduledStart);
    connect(m_groupRide, &GroupRideCoordinator::rideStarted, this, [this]() {
        m_groupPanel->setRideStarted();
        if (!isWorkoutStarted) start_or_pause_workout();
    });
    connect(m_groupRide, &GroupRideCoordinator::telemetrySent, m_groupPanel, &GroupPanel::setOwnTelemetry);
    connect(m_groupRide, &GroupRideCoordinator::statusChanged, this, &WorkoutDialog::setMessagePlot);
    m_groupRide->begin();
    setMessagePlot();
}
