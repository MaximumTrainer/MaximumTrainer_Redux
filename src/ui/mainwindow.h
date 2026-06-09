#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkReply>
#include <QThread>
#include <QDockWidget>
#ifndef GC_WASM_BUILD
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtWebEngineCore/QWebEngineDownloadRequest>
#else
#include <QWebEngineDownloadItem>
#endif
#endif

#include "planobject.h"
#include "fancytabbar.h"
#include "radio.h"
#include "workout.h"
#include "settings.h"
#include "dialogmainwindowconfig.h"
#include "savingwindow.h"
#include "userstudio.h"
#include "myconstants.h"
#include "tab_intervals_icu.h"
#include "historywidget.h"
#include "planadherencestore.h"
#ifdef GC_WASM_BUILD
#include "btle_hub_wasm.h"
#else
#include "btle_hub.h"
#include "btle_sensor_config.h"
#endif


#include "workoutqueue.h"
#include "queuepanelwidget.h"

class WorkoutCountdownDialog; // forward

namespace Ui {
class MainWindow;
}

class WorkoutDialog;
class SimulatorHub;



class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void closeEvent(QCloseEvent *event);



signals :
    void isExpanded(bool isExpanded);
    void ftpAndTabProfileChanged();


public slots:

    void executeWorkout(Workout workout);

    void startScreenshotMode(const QString &outputDir);

#ifndef GC_WASM_BUILD
private:
    /// Launch the workout dialog wired to multiple already-connected BLE hubs,
    /// one per saved sensor role. Used by the multi-device pairing flow.
    void startWorkoutWithHubs(const Workout &workout,
                              const QMap<BtleSensorRole, BtleHub*> &hubsByRole);

    /// Wire one rider's role→hub map to the dialog; distinct hubs accumulate in
    /// \a allHubs for cleanup. Shared by the solo and Studio launch paths.
    void wireHubsToDialog(WorkoutDialog *w,
                          const QMap<BtleSensorRole, BtleHub*> &hubsByRole,
                          QSet<BtleHub*> &allHubs);

    /// Launch a Studio-mode workout with each rider's already-connected hubs
    /// (tagged via BtleHub::setUserID). Pairs are (riderIndex, role→hub map).
    void startWorkoutWithStudioHubs(const Workout &workout,
                                    const QList<QPair<int, QMap<BtleSensorRole, BtleHub*>>> &riderHubs);
public slots:
#endif

    //Coming from Studio QWebView ----------
    void enableStudioMode(bool enable);
    void setNumberUserStudio(int numberUser);
    void loadConfigStudio();
    void saveConfigStudio();
    void updateVecStudio(QVector<UserStudio>);
    void updateFieldForUser(int riderID, int fieldNumber, QVariant value);

    /// Build the per-rider UserStudio vector to hand to a Studio-mode workout:
    /// fills FTP/LTHR from the account when a rider left them unset, and sets the
    /// metric-section visibility flags (hrID/powerID/…). \a simulator shows every
    /// metric; otherwise the flags reflect that rider's saved BLE sensors.
    QVector<UserStudio> prepareStudioRiders(bool simulator);
    void leftMenuChanged(int tabSelected);

    void goToWorkoutPlanFilter(const QString& plan);
    void goToWorkoutNameFilterFromIntervals(const QString &workoutName);
    void exportWorkoutToPdf(const Workout& workout);


    void showWorkoutList();
    void showWorkoutCreator();



    void setPmForCadence(bool usedFor);
    void setPmForSpeed(bool usedFor);



    void workoutExecuting();
    void workoutOver();

    void checkToUploadFile(const QString& filename, const QString& nameOnly, const QString& description);

    //tempo
    void postDataAccountFinished();



private slots:
    void on_actionAbout_MT_triggered();
    void on_actionAbout_Qt_triggered();
    void on_actionRequest_Help_triggered();
    void on_actionKeyboard_Shortcuts_triggered();
    void on_actionCheck_for_Updates_triggered();
    void slotVersionCheckFinished();
    void on_actionPreferences_triggered();
    void on_actionWorkout_triggered();
    void on_actionHistory_triggered();
    void addWorkoutToQueue(const Workout &workout);


    void on_actionCreate_New_triggered();

    void on_actionOpen_Ride_triggered();
    void on_actionExit_triggered();


    void on_actionSingle_Workout_triggered();
    void on_actionMultiple_Workouts_triggered();

    //-Intervals.icu
    void slotIntervalsIcuUploadFinished();

    void onNetworkOnlineChanged(bool isOnline);

    void createWebChannelPlan();

    void reloadPlanWebView();
    void onIntervalsIcuWorkoutDownloaded();
    void showPlanContextMenu(const QPoint &pos);

#ifndef GC_WASM_BUILD
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    void onTrainerwebDownloadRequested(QWebEngineDownloadRequest *download);
#else
    void onTrainerwebDownloadRequested(QWebEngineDownloadItem *download);
#endif
#endif

    // Screenshot mode — captures a sequence of PNG files then quits the app.
    void screenshotNextStep();

    void slotSystemThemeChanged();

private:
    void loadSettings();
    void saveSettings();

    void savePathImport(const QString& filepath);
    QString loadPathImport() const;

    void saveAndNavigateToWorkout(const Workout &workout, const QString &subFolder);

    void tryAdvanceWorkoutQueue();

    // Screenshot mode helpers
    Workout makeDemoWorkout() const;

private:
    QVector<UserStudio> vecUserStudio;

    //pairing
    QList<int> lstDevicePaired;
    QList<int> lstTypeDevicePaired;
    int pairingResponseNumber;
    bool pairingResponseAlreadySent;
    //-----------------------



    Ui::MainWindow *ui;
    DialogMainWindowConfig *dconfig;

    QList<Radio> lstRadio;

    PlanObject *planObject;

    QEventLoop loop;
    QNetworkReply *replySaveAccount;
    int saveAccountTry;
    SavingWindow savingWindow;

    //Intervals.icu
    QNetworkReply *replyIntervalsIcuUpload;
    QNetworkReply *replyVersionCheck = nullptr;


    Settings *settings;
    Account *account;
    FancyTabBar *ftb;

    int currentIndexLeftMenu;
    bool isInsideWorkout;

    QString lastWorkoutNameDownloaded;

    QNetworkReply *replyIntervalsIcuZwo;
    QString        m_pendingIntervalsWorkoutId;

    QString        m_ssOutputDir;
    int            m_ssStep       = 0;
    WorkoutDialog *m_ssWorkoutDlg = nullptr;
    SimulatorHub  *m_ssSimHub     = nullptr;
    QList<SimulatorHub*> m_ssStudioHubs;   // per-rider sim hubs for the studio-workout shot
    // Saved so screenshot mode (which toggles studio mode + rider count for its
    // captures) can restore the user's real settings before quitting.
    bool m_ssSavedStudioEnabled = false;
    int  m_ssSavedRiderCount    = 1;

    // Plan Adherence (#157)
    PlanAdherenceStore *m_adherenceStore = nullptr;

    // Workout queue (#152)
    WorkoutQueue     *m_workoutQueue   = nullptr;
    QueuePanelWidget *m_queuePanel     = nullptr;
    QDockWidget      *m_queueDock      = nullptr;
};

#endif // MAINWINDOW_H
