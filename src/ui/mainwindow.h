#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTranslator>
#include <QNetworkReply>
#include <QThread>
#ifndef GC_WASM_BUILD
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtWebEngineCore/QWebEngineDownloadRequest>
#else
#include <QWebEngineDownloadItem>
#endif
#endif

#include "zoneobject.h"
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
#ifdef GC_WASM_BUILD
#include "btle_hub_wasm.h"
#else
#include "btle_hub.h"
#endif


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

    //Coming from Studio QWebView ----------
    void enableStudioMode(bool enable);
    void setNumberUserStudio(int numberUser);
    void loadConfigStudio();
    void saveConfigStudio();
    void updateVecStudio(QVector<UserStudio>);
    void fillStudioPage();
    void fillSettingPage();
    void companyLoadedForUser(int riderID);
    void setPowerCurveForUser(int riderID, int company_id, int trainer_id, QString companyName, QString trainerName,
                              double coef0, double coef1, double coef2, double coef3, int formulaInCode);
    void disablePowerCurveForUser(int riderID);
    void updateFieldForUser(int riderID, int fieldNumber, QVariant value);
    void updateTrainerCurve(int trainer_id, QString companyName, QString trainerName,
                            double coef0, double coef1, double coef2, double coef3, int formulaInCode);



    void updateZoneInterface();
    void leftMenuChanged(int tabSelected);

    void goToWorkoutPlanFilter(const QString& plan);
    void goToWorkoutNameFilterFromIntervals(const QString &workoutName);
    void exportWorkoutToPdf(const Workout& workout);


    void showWorkoutList();
    void showWorkoutCreator();

    void setFlagFtpChanged();



    void setPmForCadence(bool usedFor);
    void setPmForSpeed(bool usedFor);



    void workoutExecuting();
    void workoutOver();

    void checkToUploadFile(const QString& filename, const QString& nameOnly, const QString& description);

    //tempo
    void postDataAccountFinished();



private slots:
    void slotFinishedGetRadio();


    void on_actionAbout_MT_triggered();
    void on_actionAbout_Qt_triggered();
    void on_actionRequest_Help_triggered();
    void on_actionCheck_for_Updates_triggered();
    void slotVersionCheckFinished();
    void on_actionPreferences_triggered();
    void on_actionWorkout_triggered();
    void on_actionOpen_Course_Folder_triggered();
    void on_actionHistory_triggered();


    void on_actionCreate_New_triggered();

    void on_actionOpen_Ride_triggered();
    void on_actionExit_triggered();
    void on_actionLog_off_Exit_triggered();


    void on_actionSingle_Workout_triggered();
    void on_actionMultiple_Workouts_triggered();

    //-Strava
    void slotStravaUploadFinished();
    void slotStravaCheckUploadStatus();
    void slotStravaUploadStatusFinished();
    //- TrainingPeaks
    void slotTrainingPeaksRefreshFinished();
    void slotTrainingPeaksUploadFinished();
    //-SelfLoops
    void slotSelfLoopsUploadFinished();
    //-Intervals.icu
    void slotIntervalsIcuUploadFinished();

    void onNetworkOnlineChanged(bool isOnline);

    void on_actionImportCourse_triggered();

    void createWebChannelPlan();
    void createWebChannelZone();
    void createWebChannelSettings();
    void createWebChannelStudio();

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

private:
    void loadSettings();
    void saveSettings();

    void savePathImport(const QString& filepath);
    QString loadPathImport() const;

    void saveAndNavigateToWorkout(const Workout &workout, const QString &subFolder);


    void checkToEnableWindow();
    void sendDataToSettingsOrStudioPage(int deviceType, int numberDeviceFound, QList<int> lstDevicePairedr, QList<int> lstTypeDevicePairedr, bool fromStudioPage);

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

    bool replyRadioDone;
    QNetworkReply *replyRadio;
    QList<Radio> lstRadio;

    ZoneObject *zoneObject;
    PlanObject *planObject;

    QEventLoop loop;
    QNetworkReply *replySaveAccount;
    int saveAccountTry;
    SavingWindow savingWindow;

    //Strava
    QNetworkReply *replyStravaUpload;
    QNetworkReply *replyStravaUploadStatus;
    int stravaUploadID;
    QTimer *timerCheckUploadStatus;
    //TrainingPeaks
    QString nameWorkout;
    QString descriptionWorkout;
    QString filepathWorkout;
    QNetworkReply *replyTrainingPeaksRefreshStatus;
    QNetworkReply *replyTrainingPeaksPostFile;
    //SelfLoops
    QNetworkReply *replySelfLoopsUpload;
    //Intervals.icu
    QNetworkReply *replyIntervalsIcuUpload;
    QNetworkReply *replyVersionCheck = nullptr;


    Settings *settings;
    Account *account;
    FancyTabBar *ftb;

    int currentIndexLeftMenu;
    bool ftpChanged;
    bool isInsideWorkout;

    QString lastWorkoutNameDownloaded;

    QNetworkReply *replyIntervalsIcuZwo;
    QString        m_pendingIntervalsWorkoutId;

    QString        m_ssOutputDir;
    int            m_ssStep       = 0;
    WorkoutDialog *m_ssWorkoutDlg = nullptr;
    SimulatorHub  *m_ssSimHub     = nullptr;

};

#endif // MAINWINDOW_H
