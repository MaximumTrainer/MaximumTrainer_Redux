#ifndef WORKOUTDIALOG_H
#define WORKOUTDIALOG_H

#include <QDialog>
#include <QTime>
#include <QTimer>

#include "virtual_gear.h"
#include <QHash>
#include <QKeyEvent>
#include <QNetworkReply>

#include "account.h"
#include "workout.h"
#include "sensor.h"
#include "workoutplot.h"
#include "settings.h"
#include "dataworkout.h"
#include "managerachievement.h"
#include "achievement.h"
#include "soundplayer.h"

#include "faderlabel.h"
#include "faderframe.h"
#include "radio.h"
#ifdef GC_HAVE_QTMULTIMEDIA
#include "qtmediaplayer.h"
#endif
#include "userstudiowidget.h"
#include "myconstants.h"
#include "userstudio.h"

#ifdef Q_OS_MAC
#include "macutils.h"
#endif


class DialogConfig;     // forward declaration
class WebBrowserView;   // forward declaration (lazily created web video player)
class QQuickWidget;
class RetroRaceController;   // retro ghost-race view
class ZwiftClickRelay;       // Zwift Click v2 input via the trainer's FC82 relay

namespace Ui {
class WorkoutDialog;
}

class WorkoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WorkoutDialog(Workout workout,
                           QList<Radio> lstRadio, QVector<UserStudio> vecUserStudio, QWidget *parent = 0);
    ~WorkoutDialog();

    void reject();

    /// Wire a Zwift Click v2 (read via the trainer's FC82 relay) to workout
    /// actions. The relay is owned by the trainer's BtleHub, not this dialog.
    /// No-op if relay is nullptr.
    void setClickRelay(ZwiftClickRelay *relay);

    /// Mark a controllable trainer (FTMS / simulator) as wired for this solo
    /// workout so sendLoads()/sendSlopes() emit ERG targets.  Historically
    /// the FE-C id came from the removed maximumtrainer.com sensor list,
    /// which left trainerControlUserId at -1 and silently disabled trainer control
    /// for every BLE workout.  Solo hubs ignore the id (1 = solo rider).
    void enableTrainerControl();   // sets the solo id and reveals the gear indicator

    void showVideoPlayer(int choice);

    void showHeartRateDisplayWidget(int display);
    void showPowerDisplayWidget(int display);
    void showCadenceDisplayWidget(int display);
    void showSpeedDisplayWidget();
    void useVirtualSpeedData(bool useIt);
    void showTrainerSpeed(bool show);
    void showCaloriesDisplayWidget();
    void showOxygenDisplayWidget();
    void showPowerBalanceWidget(int display);



    void setMessagePlot();
    void moveWidgetsPosition();

    ///timers
    void showTimerOnTop(bool);
    void showTimerIntervalRemaining(bool);
    void showTimerWorkoutRemaining(bool);
    void showTimerWorkoutElapsed(bool);
    void showTimerCurrentTarget(bool);
    void updateTimeWidgetVisibility();



    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
signals:
    ///Clock
    void startClock();
    void pauseClock();
    void resumeClock();
    void finishClock();
    void sendUserInfo(int userID, double cda, double weight, int nbUser);
    void sendPowerData(int userID, int power);
    void sendSlopeData(int userID, double slope);
    void startClockSpeed();


    //--Send to Hub
    void sendDataUserStudio(QVector<UserStudio>);
    void sendSoloData(int wheelCircMM, QList<Sensor> lstSensor, bool usePmForCadence, bool usePmForSpeed);

    void stopDecodingMsgHub();

    // Commands to FE-C
    void setLoad(int antID, double load);
    void setSlope(int antID, double slope);
    // Virtual-shifting resistance level (FTMS 0x04, 0.1 units) — instant gear feel.
    void setResistance(int antID, int levelTenths);

    void increaseDifficulty();
    void decreaseDifficulty();

    void permissionGrantedControl(int antID, int hubID);


    //    void targetPowerChanged(double percentageTarget, int target, int range);
    //    void targetCadenceChanged(double notUsed, int target, int range);
    //    void targetHrChanged(double percentageTarget, int target, int range);


    /// After data passing thru xDataReceived Slot
    //    void powerChangedAfterTreatment(int power);



    void ftpUserStudioChanged(QVector<UserStudio>);
    void ftp_lthr_changed();
    void fitFileReady(QString filename, QString nameOnly, QString description);

    void playPlayer();
    void pausePlayer();


    void insideConfig(bool inside);

    // Radio
    void F6previous();
    void F7playPause();
    void F8next();

    // Emitted when the user requests a reconnect after a BLE disconnection.
    // Connect to BtleHub::scanForDevice() (WASM) or BtleHub::connectToDevice()
    // (desktop) in MainWindow::executeWorkout().
    void reconnectDevice();



public slots:
    // control list master of QThreads hub
    void addToControlList(int antID, int fromHubNumber);

    /// Whether the connected trainer supports FTMS Set Target Resistance Level
    /// (0x04). When true, virtual shifting uses resistance level (instant, gear-
    /// like); otherwise it falls back to cadence-aware ERG.
    void setResistanceLevelSupported(bool supported) { m_trainerSupportsResistanceLevel = supported; }

    ///Connected to Clock
    void update1sec(double totalTimeElapsed_sec);
    void updateRealTimeGraph(double totalTimeElapsed_msec);
    void updatePausedTime(double totalTimePaused_msec);

    void lapButtonPressed();


    void batteryStatusReceived(QString sensorType, int percentage);

    void startCalibrateFEC();
    void startCalibrationPM();


    void HrDataReceived(int userID, int value);
    void CadenceDataReceived(int userID, int value);
    void PowerDataReceived(int userID, int value);
    void PowerBalanceDataReceived(int userID, int rightPedalPercentage);
    void pedalMetricReceived(int userID, double,double,double,double,double);
    void TrainerSpeedDataReceived(int userID, double value);
    void VirtualSpeedDataReceived(int userID, double value, double timeAtThisSpeedSec);
    void OxygenValueChanged(int userID, double, double);

    void start_or_pause_workout();

    // Invoked when the BLE hub emits connectionError() — on WASM the DOM overlay
    // already shows the Reconnect button; this slot logs and pauses the workout.
    void onBleConnectionError(const QString &errorString);

    void checkSensorDropout();   ///< 1-Hz watchdog for sensor dropout auto-pause

    void ignoreClickPlot();

    void activateSoundBool();
    void activateSoundPowerTooLow();
    void activateSoundPowerTooHigh();
    void activateSoundCadenceTooLow();
    void activateSoundCadenceTooHigh();

    // QNetwork reply
    void slotFinishedGetPixmap();


    void achievementReceived(Achievement achievement);
    void lastAchievementAnimationDone();







private slots:

    void speedDataChosen(int userID, double value);

    void closeWindow();
    void expandWindow();
    void showConfig();
    void toggleTransparent();

    void insertInterval();
    void moveToInterval(int nb, double secWorkout, double startIntervalSec, bool showConfirmation);
    void adjustWorkoutDifficulty(int percentage);

    // Post-workout upload slots
    void uploadToStrava();
    void doStravaUpload();   ///< the actual upload, after any token refresh
    void setStravaPostStatus(const QString &text, bool retryable);
    void uploadToIntervalsIcu();
    void setIntervalsIcuPostStatus(const QString &text, bool retryable);
    void slotPostIntervalsIcuUploadDone();
    void slotPostStravaUploadDone();
    void slotPostStravaCheckStatus();
    void slotPostStravaStatusDone();



private:

    void keyPressEvent(QKeyEvent *event) override;
    void showEvent(QShowEvent *event) override;

    void checkFitFileCreated();
    void showPostWorkoutPanel();
    void closeFitFiles(double timeElapsed_sec);
    void closeAndDeleteFitFile();
    void changeIntervalsDataWorkout(double, double, int, bool, bool);
    void initDataWorkout();
    void connectDataWorkout();
    void createUserStudioWidget();
    void showIntervalSummaryOverlay(double avgPower, double avgHr, double avgCad,
                                    int durationSec, double targetPowerFraction);

    void sendUserInfoToClock();

    void showTestResult();



    void sendTargetsPower(double percentageTarget, int range);
    void sendTargetsCadence(int target, int range);
    void sendTargetsHr(double percentageTarget, int range);

    void sendLoads(double percentageFTP);
    void sendSlopes(double slope);


    void showFullScreenWin();
    void showNormalWin();
    void loadInterface();
    void saveInterface();

    void animateAchievement();
    void checkMAPTestOver();


    bool eventFilter(QObject *watched, QEvent *event);

    void initUI();
    double calculateNewTargetPower();
    int calculateNewTargetCadence();
    double calculateNewTargetHr();
    void startWorkout();
    void workoutOver();
    void moveToNextInterval();

    void targetPowerChanged_f(double percentageTarget, int range);
    void targetCadenceChanged_f(int target, int range);
    void targetHrChanged_f(double percentageTarget, int range);
    void setWidgetsStopped(bool b);


    void sendLastSecondData(int seconds);
    void adjustTargets(Interval interval);

    void sureYouWantToQuit();




    //-----------------------------------------
private:
    bool isUsingSlopeMode;
    QTimer *timerAlertCalibrateCt;

    // Virtual shifting (#293): on single-cog trainers, slope/test intervals
    // otherwise send resistance 0 and the rider spins out. Up/Down shift a
    // virtual gear that drives the trainer via the (working) FTMS ERG path,
    // cadence-aware so it feels like a gear rather than a flat ERG clamp.
    static constexpr int kVirtualGearCount = VirtualGear::Count;
    int  m_virtualGear = (VirtualGear::Count + 1) / 2;   // start mid-gear
    bool m_trainerSupportsResistanceLevel = false;       // FTMS 0x04 available?
    bool virtualShiftingActive() const;           // a trainer is under our control
    bool ergOwnsThisInterval() const;             // ERG enabled + active power target
    bool gearsDriveNow() const;                   // gears are the active resistance source
    int  gearTargetWatts(int gear, double cadence) const;
    void sendGearLoad();                          // emit setLoad for current gear
    void shiftGear(int delta);                    // +1 up / -1 down, re-sends
    void updateGearIndicator();                   // refresh the top-bar gear UI


    //Internet radio player
#ifdef GC_HAVE_QTMULTIMEDIA
    QtMediaPlayer *radioPlayer;
#endif

    // Zwift Click v2 controller input (native BLE). Created when virtual
    // shifting is on and controllers are paired; maps buttons to shift /
    // difficulty / start-pause / lap / radio actions.
#ifndef Q_OS_WASM
    ZwiftClickRelay *m_clickRelay = nullptr;   // owned by the trainer's BtleHub
#endif

    // Embedded web video player (QWebEngine-based), created and loaded in the
    // background when the dialog opens (see showEvent).
    WebBrowserView *webPlayer = nullptr;
    /// Returns the web player, creating it (inside widget_webPlayer) on first call.
    WebBrowserView *ensureWebPlayer();
    /// Guards the one-time background load of the web player done in showEvent().
    bool webPlayerLoaded = false;

    // Retro ghost-race view, the third content option (display_video == 2). Sits
    // in the same slot as the video player. Opponent is this workout's most
    // recent past ride, or a computer pacer when there is no history yet; the
    // player is driven by live trainer power.
    QQuickWidget        *raceView       = nullptr;
    RetroRaceController *raceController = nullptr;
    QQuickWidget *ensureRaceView();
    QVariantList buildWorkoutProfile() const;   // normalised target-power samples
    void onToggleGameFullscreen();              // collapse/restore the data panes
    bool m_gameFullscreen = false;
    QList<int> m_savedSplitterSizes;

    DialogConfig *dconfig;
    QList<Radio> lstRadio;

    ///Clock thread
    QThread *thread;

    /// Pairing window
    QWidget *widgetLoading;
    FaderLabel *labelPairHr;
    FaderLabel *labelSpeedCadence;
    FaderLabel *labelCadence;
    FaderLabel *labelSpeed;
    FaderLabel *labelFEC;
    FaderLabel *labelOxygen;
    FaderLabel *labelPower;
    FaderLabel *labelCtPower;

    /// Calibrate window + battery status
    QWidget *widgetBattery;
    FaderLabel *labelBattery;
    FaderLabel *labelBatteryStatus;

    bool isAskingUserQuestion;
    bool isCalibrating;



    /// Achievement window
    FaderFrame *widgetAchievement;
    QLabel *labelIcon;
    QLabel *labelAchievementName;
    QTimer *timerLastAnimationAchievementComplete;
    QQueue<Achievement> queueAchievement;
    bool achievementCurrentlyPlaying;

    // Post-workout summary: achievements earned this session
    QList<Achievement> earnedAchievements;

    // Post-workout upload panel and stored FIT file info
    QWidget *widgetPostWorkout = nullptr;
    QString fitFilePath;
    QString fitFileName;
    QString fitFileDescription;
    bool    m_quitAfterSave = false;   // suppress panel when saving-and-quitting
    QNetworkReply *replyPostStravaUpload   = nullptr;
    QNetworkReply *replyPostStravaStatus   = nullptr;
    QTimer        *timerPostStravaStatus   = nullptr;
    qint64         stravaUploadID_post     = 0;
    QNetworkReply *replyPostIntervalsIcuUpload = nullptr;



    QString msgPairingDone;
    bool hrPairingDone;
    bool scPairingDone;
    bool cadencePairingDone;
    bool speedPairingDone;
    bool powerPairingDone;
    bool fecPairingDone;
    bool oxygenPairingDone;


    /// Identify sensors (solo)
    Sensor sensorHr;
    Sensor sensorSpeedCadence;
    Sensor sensorCadence;
    Sensor sensorSpeed;
    Sensor sensorPower;
    Sensor sensorFEC;
    Sensor sensorOxygen;


    bool usingHr;
    bool usingSpeedCadence;
    bool usingCadence;
    bool usingSpeed;
    bool usingPower;
    bool usingPowerSensorForCadence;
    bool usingPowerSensorForSpeed;
    bool usingFEC;
    bool usingOxygen;

    int trainerControlUserId;


    void checkPairingCompleted();
    /////////////////////////


    QString bakStylesheet;
    bool isTransparent;
    bool wasFullscreenOnOpen;
    bool isFullScreenFlag;
    QSize lastNormalSize;
    Ui::WorkoutDialog *ui;


    QHash<int,int> hashControlList; //addId, hubId
    //    int nbTotalFecTrainer;


    Workout workout;
    bool isTestWorkout;
    QVector<UserStudio> vecUserStudio;
    //DataWorkout *dataWorkout; //Pointer because is a QObject (needs Signal) - deletion is automatic
    DataWorkout *arrDataWorkout[constants::nbMaxUserStudio];
    UserStudioWidget *arrUserStudioWidget[constants::nbMaxUserStudio];
    SoundPlayer *soundPlayer;

    QTimer *timer_sec;
    QNetworkReply *replyGetBixmap;

    Account *account;
    Settings *settings;
    ManagerAchievement *achievementManager;


    // For Rolling Averaging Power
    //    QQueue<double> queuePower;
    //    int lastSecondPower;
    //    int nbPointsPower;

    QQueue<double> arrQueuePower[constants::nbMaxUserStudio];
    int arrLastSecondPower[constants::nbMaxUserStudio];
    int arrNbPointPower[constants::nbMaxUserStudio];

    //to calculate mean every second ,[0] = Rider#1
    QVector<int> nbPointHr1sec;
    QVector<double> averageHr1sec;
    QVector<int> nbPointCadence1sec;
    QVector<double> averageCadence1sec;
    QVector<int> nbPointSpeed1sec;
    QVector<double> averageSpeed1sec;
    QVector<int> nbPointPower1sec;
    QVector<double> averagePower1sec;

    //not doing average for now
    QVector<double> avgRightPedal1sec;
    QVector<double> avgLeftTorqueEff;
    QVector<double> avgRightTorqueEff;
    QVector<double> avgLeftPedalSmooth;
    QVector<double> avgRightPedalSmooth;
    QVector<double> avgCombinedPedalSmooth;
    QVector<double> avgSaturatedHemoglobinPercent1sec;
    QVector<double> avgTotalHemoglobinConc1sec;




    double lastIntervalEndTime_sec;

    //to determine pausedTime
    int lastIntervalTotalTimePausedWorkout_msec;
    int totalTimePausedWorkout_msec;

    int lastIntervalEndTime_msec;
    double timeElapsed_sec;
    //    double skippedTime_sec;

    // The clock thread ticks every 25ms (40Hz) so virtual-speed physics stays
    // smooth, but the three scrolling mini-graphs only need a fraction of that
    // to look fluid (their data changes at the ~1-4Hz sensor rate). Coalesce
    // their replot() calls to this cadence instead of replotting 40x/sec.
    static constexpr double kMiniGraphRefreshMs = 50.0; // ~20 fps
    double lastMiniGraphReplot_msec = -kMiniGraphRefreshMs;

    int nbUpdate1Sec;
    QTime timeWorkoutRemaining;
    QTime timeElapsedTotal;
    QTime timeInterval;



    bool isWorkoutStarted;
    bool isWorkoutOver;
    bool isWorkoutPaused;


    /// disable screen saver
#ifdef Q_OS_MAC
    MacUtils macUtil;
#endif

    bool bignoreClickPlot;
    QTimer *timerIgnoreClickPlot;

    ///------ Sounds ----------------------
    QTimer *timerCheckToActivateSound;
    bool soundsActive;

    int durationReactivateSameSoundMsec; //to edit if 10sec is too long
    QTimer *timerCheckReactivateSoundPowerTooLow;
    QTimer *timerCheckReactivateSoundPowerTooHigh;
    QTimer *timerCheckReactivateSoundCadenceTooLow;
    QTimer *timerCheckReactivateSoundCadenceTooHigh;
    bool soundPowerTooLowActive;
    bool soundPowerTooHighActive;
    bool soundCadenceTooLowActive;
    bool soundCadenceTooHighActive;
    ///---------------------------------------

    int currMAPInterval;
    int totalSecOffTargetInInterval;
    int totalConsecutiveOffTarget;

    bool m_timerOnTop = false;   // timers shown on the top bar (vs bottom-left)

    int currentTargetPower;
    int currentTargetPowerRange;
    int currentTargetCadence;
    int currentTargetCadenceRange;

    Interval currentIntervalObj;
    int currentInterval;
    bool changeIntervalDisplayNextSecond;
    bool ignoreCondition;

    int currentWorkoutDifficultyPercentage; //0 = Normal

    ///------ Sensor dropout auto-pause ----------------------
    qint64 m_lastPowerMs       = 0;  ///< Timestamp (ms) of last valid power reading
    qint64 m_lastHrMs          = 0;  ///< Timestamp (ms) of last valid HR reading
    bool   m_dropoutPaused     = false; ///< True when workout is paused due to dropout
    int    m_recoveryCountdown = 0;  ///< Countdown (s) before auto-resume after recovery
    QTimer *m_dropoutWatchdog  = nullptr;
    ///-------------------------------------------------------

    /// Battery warning tracking (issue #156): sensorType → last warned percentage.
    /// A warning is suppressed for a sensor unless the level drops ≥ 5 % below
    /// the level at which the last warning was shown.
    QHash<QString, int> m_warnedBatteryLevels;

    WorkoutPlot *mainPlot;
    friend class DialogConfig;

    // ERG mode resistance smoothing
    QTimer    *m_ergSmoothTimer   = nullptr;
    double     m_ergSmoothFrom    = 0.0;    ///< Starting watts when ramp began
    double     m_ergSmoothTo      = 0.0;    ///< Target watts for the ramp
    double     m_ergSmoothLast    = 0.0;    ///< Most-recently emitted watts (for mid-ramp retargeting)
    int        m_ergSmoothStep    = 0;      ///< Steps elapsed so far
    int        m_ergSmoothSteps   = 0;      ///< Total steps in current ramp
    int        m_ergSmoothAntID   = -1;     ///< ANT ID to target

    void startErgSmoothing(double fromWatts, double toWatts);
    void stopErgSmoothing();
    void ergSmoothStep();
};

#endif // WORKOUTDIALOG_H
