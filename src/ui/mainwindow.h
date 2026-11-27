#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QPointer>
#include "racebot.h"
#include "racecourse.h"

class CoursesPage;
#include <optional>
#include <QMainWindow>
#include <QNetworkReply>
#include <QThread>
#ifndef GC_WASM_BUILD
#endif

#include "fancytabbar.h"
#include "radio.h"
#include "workout.h"
#include "settings.h"
#include "settingspage.h"
#include "userstudio.h"
#include "myconstants.h"
#include "tab_intervals_icu.h"
#include "historywidget.h"
#ifdef GC_WASM_BUILD
#include "btle_hub_wasm.h"
#else
#include "btle_hub.h"
#include "btle_sensor_config.h"
#endif




namespace Ui {
class MainWindow;
}

class GroupSession;

class WorkoutDialog;
class ZwiftClickManager;
class AchievementsWidget;
class SimulatorHub;
class DeviceSession;
class SensorStatusStrip;
class QLabel;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void closeEvent(QCloseEvent *event) override;
    /// Connects the saved sensors and Zwift Click for the whole session (after login).
    void startDevices();


signals :
    void isExpanded(bool isExpanded);
    void ftpAndTabProfileChanged();
    /// Emitted from closeEvent when the user chose Log Out, so the app can
    /// return to the login screen in-process instead of quitting.
    void logoutRequested();


public slots:

    void executeWorkout(Workout workout);

    void startScreenshotMode(const QString &outputDir);

    /// Headless BLE-sensor validation: drives a demo workout and injects each
    /// sensor type one at a time (HR/power/cadence/speed/oxygen/balance/pedal),
    /// grabbing a screenshot per type, and captures the trainer-control output
    /// (setLoad/setSlope) to prove ERG actually drives the trainer. Writes a
    /// report then quits. Entry: --sensor-check [dir].
    void runSensorCheck(const QString &outDir);
    /// --device-session-check: real hardware. Connects the saved sensors at
    /// startup, opens a free ride, closes it, opens a second one, and reports
    /// whether the trainer stayed connected and how fast power reached each ride.
    void runDeviceSessionCheck(const QString &outDir);
    /// --bench-workout: run the simulated demo workout headless for @p seconds,
    /// then print the process CPU usage over that window and quit.
    void runWorkoutBenchmark(int seconds);

    /// Launch the demo workout dialog (simulator-driven, non-modal) used by
    /// screenshot mode and, on WASM, by the `window.mt_startDemoWorkout` test
    /// hook to drive the app into the metric-dashboard view for render checks.
    void launchDemoWorkout();

#ifndef GC_WASM_BUILD
private:
    /// Open the workout window straight away on the app's live connections
    /// (DeviceSession): a chip per sensor shows its state, and each hub is wired
    /// to the workout as soon as it connects. Closing the workout keeps them.
    void startWorkoutOnLiveDevices(const Workout &workout);

    /// Wire one rider's role→hub map to the dialog; distinct hubs accumulate in
    /// \a allHubs for cleanup. Shared by the solo and Studio launch paths. With
    /// \a releaseOnClose the hubs disconnect when the workout closes.
    void wireHubsToDialog(WorkoutDialog *w,
                          const QMap<BtleSensorRole, BtleHub*> &hubsByRole,
                          QSet<BtleHub*> &allHubs,
                          int riderIndex = 0,
                          bool releaseOnClose = true);

    void rebuildDeviceBar();
    /// Right-click on the device bar; \a key is the chip clicked (-1: none).
    void showDeviceMenu(int key, const QPoint &globalPos);

    DeviceSession     *m_devices = nullptr;
    SensorStatusStrip *m_deviceStrip = nullptr;
    QLabel            *m_deviceNote = nullptr;

    /// Studio: open the workout window straight away and connect every
    /// rider's saved sensors inside it, all at once.
    void startStudioWorkoutConnectingSensors(const Workout &workout);
public slots:
#endif

private:
    AchievementsWidget *m_achievementsPage = nullptr;
    CoursesPage *m_coursesPage = nullptr;
    /// Left-bar positions, set when the bar is built. Use these, never numbers.
    struct LeftTabs {
        int workout = 0;
        int courses = 1;
        int intervals = 2;
        int history = 3;
        int achievements = 4;
        int devices = 5;
        int studio = 6;
        int settings = 7;
    } m_tabs;
    /// The old menu bar's actions, as buttons on the pages they belong to.
    void setupPageActions();
    QAction *m_hostGroupAction = nullptr;
    QAction *m_joinGroupAction = nullptr;
    QVector<int> m_tabToPage;   // left-bar tab -> stackedWidget_menu page
    /// Simulator-fed workout launch (the "Simulation" connection method).
    /// Shared by the desktop synchronous flow and the WASM async chain.
    void connectSimulatorHub(class SimulatorHub *hub, WorkoutDialog *dialog);
    void startSimulatedWorkout(const Workout &workout);

    /// Wire a single already-connected hub to a new WorkoutDialog and show it
    /// (the legacy single-device tail of executeWorkout, shared by both flows).
    void startWorkoutWithConnectedHub(const Workout &workout, BtleHub *btleHub);

#ifdef GC_WASM_BUILD
    /// Browser BLE launch: scanner prompt via open(), then a signal-driven
    /// wait for the connection (no nested event loop — fatal on WASM).
    void startWasmBleWorkout(const Workout &workout);
#endif
public slots:

    //Coming from Studio QWebView ----------
    void enableStudioMode(bool enable);
    void setNumberUserStudio(int numberUser);
    void updateVecStudio(QVector<UserStudio>);
    void updateFieldForUser(int riderID, int fieldNumber, QVariant value);

    /// Build the per-rider UserStudio vector to hand to a Studio-mode workout:
    /// fills FTP/LTHR from the account when a rider left them unset, and sets the
    /// metric-section visibility flags (hrID/powerID/…). \a simulator shows every
    /// metric; otherwise the flags reflect that rider's saved BLE sensors.
    QVector<UserStudio> prepareStudioRiders(bool simulator);
    void leftMenuChanged(int tabSelected);

    void goToWorkoutNameFilterFromIntervals(const QString &workoutName);


    void showWorkoutList();
    void showWorkoutCreator();


    void setPmForCadence(bool usedFor);
    void setPmForSpeed(bool usedFor);


    void workoutExecuting();
    void workoutOver();

    // Group workouts (desktop only for now).
    void hostGroupWorkout(const Workout &workout);
    void showJoinGroupDialog();



private slots:
    void showUpdateAvailable(const QString &latestVersion);
    void on_actionAbout_MT_triggered();
    void on_actionRequest_Help_triggered();
    void on_actionKeyboard_Shortcuts_triggered();
    void on_actionPreferences_triggered();
    void on_actionWorkout_triggered();
    void on_actionHistory_triggered();


    void on_actionCreate_New_triggered();

    void on_actionOpen_Ride_triggered();
    void on_actionExit_triggered();
    void on_actionLogout_triggered();
    // The confirmed half of Log Out (clear credentials + close). Split out of
    // on_actionLogout_triggered so the WASM test hook can drive it directly.
    void performLogout();


    void on_actionSingle_Workout_triggered();
    void on_actionMultiple_Workouts_triggered();

    void onNetworkOnlineChanged(bool isOnline);




    // Screenshot mode — captures a sequence of PNG files then quits the app.
    void screenshotNextStep();

    // Delete embedded QWebEngineViews before quit (see definition for why).
    // Shared by closeEvent and the screenshot-mode quit, which bypasses it.
    void releaseWebEngineViews();
    void scheduleScreenshotQuit();

    // BLE sensor-check mode — one async step per sensor type (see runSensorCheck).
    void sensorCheckNextStep();

    void slotSystemThemeChanged();

private:
    void loadSettings();
    void saveSettings();

    void savePathImport(const QString& filepath);
    QString loadPathImport() const;



    // Screenshot mode helpers
    Workout makeDemoWorkout() const;

private:
    QVector<UserStudio> vecUserStudio;

    //pairing
    QList<int> lstDevicePaired;
    QList<int> lstTypeDevicePaired;
    int pairingResponseNumber;
    bool pairingResponseAlreadySent;


    Ui::MainWindow *ui;
    SettingsPage *dconfig;

    QList<Radio> lstRadio;



    Settings *settings;
    Account *account;
    FancyTabBar *ftb;
    bool m_loggingOut = false;   // set by Log Out so closeEvent re-shows login

    int currentIndexLeftMenu;
    bool isInsideWorkout;
    // Guards executeWorkout() against re-entry while a launch is still in flight
    // (connection/sensor dialogs, BLE connect wait) so a second double-click can
    // never spawn a second WorkoutDialog. isInsideWorkout takes over once the
    // workout is actually running.
    bool m_launchingWorkout = false;

    void startGroupWorkout(const Workout &workout, GroupSession *session);
    void attachPendingGroupSession(WorkoutDialog *dialog);
    void rideCourse(const RaceCourse &course, const RaceBot::FieldSettings &bots);
    std::optional<RaceCourse> m_pendingRaceCourse;
    RaceBot::FieldSettings m_pendingRaceBots;
    bool canRideInGroup(const Workout &workout);
    QString groupDisplayName() const;
    QPointer<GroupSession> m_pendingGroupSession;

    QString lastWorkoutNameDownloaded;


    QString        m_ssOutputDir;
    int            m_ssStep       = 0;

    // BLE sensor-check mode state
    QString        m_scOutDir;
    int            m_scStep    = 0;
    QStringList    m_scReport;     // human-readable per-sensor results
    QStringList    m_scLoadLog;    // captured setLoad/setSlope (trainer control)
    WorkoutDialog *m_ssWorkoutDlg = nullptr;
    SimulatorHub  *m_ssSimHub     = nullptr;

    QList<SimulatorHub*> m_ssStudioHubs;   // per-rider sim hubs for the studio-workout shot
    // Saved so screenshot mode (which toggles studio mode + rider count for its
    // captures) can restore the user's real settings before quitting.
    bool m_ssSavedStudioEnabled = false;
    int  m_ssSavedRiderCount    = 1;

};

#endif // MAINWINDOW_H
