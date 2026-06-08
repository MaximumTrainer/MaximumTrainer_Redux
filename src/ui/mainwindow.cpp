#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QSettings>
#include <QSet>
#include <QDateTime>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QApplication>
#include <QFileDialog>
#include <QTimer>
#include <QDir>
#include <QGuiApplication>
#include <QStyleHints>

#include "util.h"
#include "logger.h"
#include "environnement.h"
#include "userdao.h"
#include "savingwindow.h"
#include "soundplayer.h"
#include "dialogmainwindowconfig.h"
#include "savingwindow.h"
#include "workoutdialog.h"
#include "workout.h"
#include "radiodao.h"
#include "mycreatorplot.h"
#include "reportutil.h"
#include "importerworkout.h"
#include "importerworkoutzwo.h"
#include "intervalsicudao.h"
#include "intervalsicuservice.h"
#include "xmlutil.h"
#include "managerachievement.h"
#include "simulator_hub.h"
#include "dialog_connection_method.h"
#include "networkmonitor.h"
#include "updatedialog.h"
#include "versiondao.h"
#include "dialogkeyboardshortcuts.h"
#include "workoutcountdowndialog.h"
#include "apptheme.h"

#include <QDir>
#include <QMenu>
#include <QRegularExpression>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEnginePage>
#include <QWebEngineScriptCollection>
#include <QWebChannel>
#ifndef GC_WASM_BUILD
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtWebEngineCore/QWebEngineDownloadRequest>
#else
#include <QWebEngineDownloadItem>
#endif
#endif
#include "myqwebenginepage.h"

#include "extrequest.h"
#include "strava_service.h"
#ifdef GC_WASM_BUILD
#include "btle_scanner_dialog_wasm.h"
#else
#include "btle_scanner_dialog.h"
#include "btle_sensor_store.h"
#include "sensor_connect_dialog.h"
#endif
#include "sensorswidget.h"
#include "studiowidget.h"





MainWindow::~MainWindow() {
    if (replyIntervalsIcuZwo) {
        replyIntervalsIcuZwo->abort();
        replyIntervalsIcuZwo->deleteLater();
        replyIntervalsIcuZwo = nullptr;
    }
    delete ui;

    qDebug() << "Desctructor MainWindow";
    qDebug() << "Desctructor Over MainWindow";
}





MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {

    ui->setupUi(this);
    this->setEnabled(false);
    ui->widget_bottomMenu->setGeneralMessage(tr("Retrieving Data...")); //will be removed when response is received
    ui->actionAbout_Qt->setIcon(QIcon(":/qt-project.org/qmessagebox/images/qtlogo-64.png"));
    ui->actionOpen_Ride->setEnabled(true);

    this->settings = qApp->property("User_Settings").value<Settings*>();
    this->account = qApp->property("Account").value<Account*>();

    zoneObject = new ZoneObject(this);         /// Used with QWebView zone page
    planObject = new PlanObject(this);         ///Used with QWebView Plan page

    replyIntervalsIcuZwo    = nullptr;
    replyIntervalsIcuUpload = nullptr;


    createWebChannelPlan();
    createWebChannelZone();
    createWebChannelSettings();
    createWebChannelStudio();

    // Right-click context menu on the Plan (Intervals.icu calendar) view
    ui->webView_plan->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->webView_plan, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(showPlanContextMenu(QPoint)));

    ui->webView_zones->setUrl(QUrl(Environnement::getUrlZones()));
    ui->webView_achiev->setUrl(QUrl(Environnement::getUrlAchievement()));
    ui->webView_settings->setUrl(QUrl(Environnement::getUrlSettings()));

    // Load Intervals.icu athlete calendar, or show a setup prompt if no credentials configured
    if (!account->intervals_icu_athlete_id.isEmpty()) {
        ui->webView_plan->setUrl(QUrl(urlIntervalsIcuCalendar.arg(account->intervals_icu_athlete_id)));
    } else {
        ui->webView_plan->setHtml(
            QStringLiteral(
                "<html><body style='font-family:sans-serif;text-align:center;padding-top:60px;'>"
                "<h2>Intervals.icu Calendar</h2>"
                "<p>No Intervals.icu credentials configured.</p>"
                "<p>Open <b>Preferences</b> and enter your athlete ID and API key "
                "in the Connectivity section.</p>"
                "</body></html>"
            )
        );
    }

    // ── Trainerweb integration ────────────────────────────────────────────────
    ui->webView_trainerweb_plans->setUrl(
        QUrl(QStringLiteral("https://trainerdb-84bdb.firebaseapp.com/listplan")));
    ui->webView_trainerweb_creator->setUrl(
        QUrl(QStringLiteral("https://trainerdb-84bdb.firebaseapp.com/listworkoutcreator")));
#ifndef GC_WASM_BUILD
    {
        // Intercept workout file downloads from both Trainerweb sub-views.
        // Both views share the same default profile so we only connect once.
        auto *plansProfile   = ui->webView_trainerweb_plans->page()->profile();
        auto *creatorProfile = ui->webView_trainerweb_creator->page()->profile();
        connect(plansProfile, &QWebEngineProfile::downloadRequested,
                this, &MainWindow::onTrainerwebDownloadRequested);
        if (creatorProfile != plansProfile)
            connect(creatorProfile, &QWebEngineProfile::downloadRequested,
                    this, &MainWindow::onTrainerwebDownloadRequested);
    }
#else
    // In WASM, QWebEngineProfile::downloadRequested is not available, so
    // workout imports from Trainerweb cannot be intercepted. Hide the tab.
    ui->tabWidget->setTabVisible(1, false);
#endif

    // Initialise the Intervals.icu tab with current credentials
    ui->tab_intervals_icu->refreshCredentials();



    // ------------------------------------- BTLE ready ---------------------------------


    stravaUploadID = -1;
    saveAccountTry = 0;

    ftb = new FancyTabBar(FancyTabBar::TabBarPosition::Left, ui->widget_fancyMenu);
    ftb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Tab indices must stay in sync with the pages in stackedWidget_menu
    // (see leftMenuChanged): 0 Workout, 1 Intervals.icu, 2 Plan, 3 Studio,
    // 4 Sensors, 5 History. The former Profile and Settings web-view tabs were
    // removed — FTP/LTHR/weight now live in the Preferences dialog, and the
    // server-hosted settings page is superseded by it.
    ftb->insertTab(0, QIcon(":/image/icon/workoutMan"), tr("Workout"));
    ftb->insertTab(1, QIcon(":/image/icon/intervals"),   tr("Intervals.icu"));
    ftb->insertTab(2, QIcon(":/image/icon/calendar"),  tr("Plan"));
    ftb->insertTab(3, QIcon(":/image/icon/studio"), tr("Studio"));
    ftb->insertTab(4, QIcon(":/image/icon/bluetooth"), tr("Sensors"));
    ftb->insertTab(5, QIcon(":/image/icon/chart"), tr("History"));

    ftb->setTabEnabled(0, true);
    ftb->setTabEnabled(1, true);
    ftb->setTabEnabled(2, true);
    ftb->setTabEnabled(3, true);
    ftb->setTabEnabled(4, true);
    ftb->setTabEnabled(5, true);



    ftb->setCurrentIndex(0);
    connect(ftb, SIGNAL(currentChanged(int)), this, SLOT(leftMenuChanged(int)) );


    ui->tabWidget_workout->tabBar()->setObjectName("tabBarWorkout");
    setStyleSheet(qApp->styleSheet());

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Track OS colour-scheme changes for System theme mode.
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            &MainWindow::slotSystemThemeChanged);
#endif


    //Load userStudio xml file to VecUserStudio
    XmlUtil *xmlUtil = new XmlUtil(this);
    vecUserStudio = xmlUtil->parseUserStudioFile("");


    ManagerAchievement *achievementManager = new ManagerAchievement(this);
    qApp->setProperty("ManagerAchievement", QVariant::fromValue<ManagerAchievement*>(achievementManager));



    //Parse Workouts
    ui->tab_workout1->parseIncludedWorkouts();
    ui->tab_workout1->parseMapWorkout(account->FTP);
    ui->tab_workout1->parseUserWorkouts();


    currentIndexLeftMenu = 0;
    ftpChanged = false;


    // connect Workout Dialog done with refresh Training Data page
    connect (ui->tab_workout1, SIGNAL(executeWorkout(Workout)), this, SLOT(executeWorkout(Workout)) );


    /// Connect Workout Creator --> WorkoutList Page
    connect(ui->tab_create, SIGNAL(workoutCreated(Workout)), ui->tab_workout1, SLOT(addWorkout(Workout)));
    connect(ui->tab_create, SIGNAL(workoutCreated(Workout)), this, SLOT(showWorkoutList()));
    connect(ui->tab_create, SIGNAL(workoutOverwrited(Workout)), ui->tab_workout1, SLOT(overwriteWorkout(Workout)));
    connect(ui->tab_create, SIGNAL(workoutOverwrited(Workout)), this, SLOT(showWorkoutList()));

    connect(ui->tab_workout1, SIGNAL(editWorkout(Workout)), this, SLOT(showWorkoutCreator()));
    connect(ui->tab_workout1, SIGNAL(editWorkout(Workout)), ui->tab_create, SLOT(editWorkout(Workout)));



    connect(ui->tab_create, SIGNAL(showStatusBarMessage(QString, int)), ui->widget_bottomMenu, SLOT(setGeneralMessage(QString,int)));
    connect(ui->tab_workout1, SIGNAL(signal_exportWorkoutToPdf(Workout)), this, SLOT(exportWorkoutToPdf(Workout)) );


    /// Update create workout graph on FTP and LTHR change
    connect(zoneObject, SIGNAL(signal_updateFTP()), this, SLOT(setFlagFtpChanged()) );
    connect(zoneObject, SIGNAL(signal_updateLTHR()), this, SLOT(setFlagFtpChanged()) );
    connect(this, SIGNAL(ftpAndTabProfileChanged()), ui->tab_create, SLOT(computeWorkout()) );
    ///Also update workout metrics based on FTP
    connect(this, SIGNAL(ftpAndTabProfileChanged()), ui->tab_workout1, SLOT(updateTableViewMetrics()) );
    connect(this, SIGNAL(ftpAndTabProfileChanged()), ui->tab_workout1, SLOT(refreshMapWorkout()) );



    /// Load Settings
    loadSettings();
    isInsideWorkout = false;


    //DialogConfig
    dconfig = new DialogMainWindowConfig(this);

    //    dconfig->setModal(true);
    connect(dconfig, SIGNAL(folderWorkoutChanged()), ui->tab_workout1, SLOT(refreshUserWorkout()) );
    connect(dconfig, &DialogMainWindowConfig::intervalsIcuCredentialsChanged, this, &MainWindow::reloadPlanWebView);
    connect(dconfig, &DialogMainWindowConfig::intervalsIcuCredentialsChanged,
            ui->tab_intervals_icu, &TabIntervalsIcu::refreshCredentials);
    // Athlete profile (FTP/LTHR/weight) edited in Preferences → recompute zones
    // and workout targets, same as the old profile page's FTP-change path.
    connect(dconfig, &DialogMainWindowConfig::profileChanged,
            this, &MainWindow::ftpAndTabProfileChanged);


    leftMenuChanged(0);
    enableStudioMode(account->enable_studio_mode);


    // ── Network connectivity monitoring ─────────────────────────────────────
    // Connect the NetworkMonitor singleton so we can hide all Intervals.icu
    // UI surfaces (sidebar tab, calendar widget, preferences section) when the
    // application goes offline, and restore them on reconnect.
    NetworkMonitor *netmon = NetworkMonitor::instance();
    connect(netmon, &NetworkMonitor::onlineChanged,
            this, &MainWindow::onNetworkOnlineChanged);
    connect(netmon, &NetworkMonitor::onlineChanged,
            ui->tab_intervals_icu, &TabIntervalsIcu::setOnlineMode);
    connect(netmon, &NetworkMonitor::onlineChanged,
            dconfig, &DialogMainWindowConfig::setOnlineMode);

    // Apply the current (initial) online state immediately so the UI matches
    // before the first timed probe fires.
    onNetworkOnlineChanged(netmon->isOnline());



    /// Load the radio list from the user's local radios.json file. The
    /// legacy maximumtrainer.com REST endpoint is gone; Util seeds the file
    /// with a few bundled defaults on first run, and the user manages it
    /// from the workout-dialog Settings → Radio tab.
    lstRadio = Util::loadLocalRadioList();
    this->setEnabled(true);
    ui->widget_bottomMenu->removeGeneralMessage();

    qDebug() << "Test1";

    connect(planObject, SIGNAL(signal_goToWorkout(QString)), this, SLOT(goToWorkoutPlanFilter(QString)) );


    connect(ui->webView_settings, SIGNAL(loadFinished(bool)), this, SLOT(fillSettingPage()));

    // Native Studio page: enabling studio mode / changing rider count routes
    // through the same MainWindow logic the old web page used.
    connect(ui->studioWidget, &StudioWidget::studioModeChanged, this, &MainWindow::enableStudioMode);
    connect(ui->studioWidget, &StudioWidget::riderCountChanged, this, &MainWindow::setNumberUserStudio);

    // Wire Intervals.icu workout downloaded → refresh workout list and filter
    connect(ui->tab_intervals_icu, &TabIntervalsIcu::workoutDownloaded,
            this, &MainWindow::goToWorkoutNameFilterFromIntervals);

    // Wire batch sync feedback
    connect(ui->tab_intervals_icu, &TabIntervalsIcu::syncFinished,
            this, [this](int count) {
                QSettings().setValue(QStringLiteral("intervalsIcu/lastSync"),
                                     QDateTime::currentDateTime().toString(Qt::ISODate));
                ui->tab_workout1->refreshUserWorkout();
                ui->widget_bottomMenu->setGeneralMessage(
                    count > 0
                        ? tr("Intervals.icu sync complete — %1 workout(s) imported.").arg(count)
                        : tr("Intervals.icu sync complete — no new workouts found."),
                    6000);
            });
    connect(ui->tab_intervals_icu, &TabIntervalsIcu::syncFailed,
            this, [this](const QString &err) {
                QMessageBox::warning(this, tr("Intervals.icu Sync Failed"), err);
            });

    // ── Plan Adherence (#157) ─────────────────────────────────────────────────
    m_adherenceStore = new PlanAdherenceStore(this);
    if (auto *hw = qobject_cast<HistoryWidget*>(ui->historyWidget))
        hw->setAdherenceStore(m_adherenceStore);

    // ── Workout Queue (#152) ─────────────────────────────────────────────────
    m_workoutQueue = new WorkoutQueue(this);
    m_queuePanel   = new QueuePanelWidget(m_workoutQueue, this);

    m_queueDock = new QDockWidget(tr("Workout Queue"), this);
    m_queueDock->setObjectName(QStringLiteral("workoutQueueDock"));
    m_queueDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_queueDock->setWidget(m_queuePanel);
    m_queueDock->setMinimumWidth(220);
    addDockWidget(Qt::RightDockWidgetArea, m_queueDock);
    m_queueDock->hide();

    connect(ui->tab_workout1, &Main_WorkoutPage::addWorkoutToQueue,
            this, &MainWindow::addWorkoutToQueue);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//---------------------------------------------------------------------------------
void MainWindow::exportWorkoutToPdf(const Workout& workout) {


    qDebug() << "Export to PDF!";

    myCreatorPlot myPlot(this);
    myPlot.updateWorkout(workout);


    myPlot.setAxisTitle(2, tr("Time"));


    QString fileNameToShow = myPlot.getSavePathExport() + QDir::separator() +  workout.getName();
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Workout"), fileNameToShow, tr("PDF Documents(*.pdf)"));
    if (fileName.isEmpty())
        return;

    //Save path for future uses
    QFileInfo fileInfo(fileName);
    myPlot.savePathExport(fileInfo.absolutePath());


    ReportUtil::printWorkoutToPdf(workout, &myPlot, fileName);
    ui->widget_bottomMenu->setGeneralMessage(tr("Saved to: ") + fileName, 7000);
}

//---------------------------------------------------------------------------------
void MainWindow::goToWorkoutPlanFilter(const QString& workoutId) {

    qDebug() << "mainWindow Plan workout ID" << workoutId;

    if (account->intervals_icu_athlete_id.isEmpty() || account->intervals_icu_api_key.isEmpty()) {
        ui->widget_bottomMenu->setGeneralMessage(
            tr("Intervals.icu credentials not configured. Open Preferences to set them."), 5000);
        return;
    }

    if (replyIntervalsIcuZwo) {
        replyIntervalsIcuZwo->abort();
        replyIntervalsIcuZwo->deleteLater();
        replyIntervalsIcuZwo = nullptr;
    }

    replyIntervalsIcuZwo = IntervalsIcuDAO::downloadWorkoutZwo(
        account->intervals_icu_athlete_id,
        workoutId,
        account->intervals_icu_api_key);

    if (!replyIntervalsIcuZwo) {
        ui->widget_bottomMenu->setGeneralMessage(
            tr("Failed to start Intervals.icu workout download."), 5000);
        return;
    }

    m_pendingIntervalsWorkoutId = workoutId;
    ui->widget_bottomMenu->setGeneralMessage(tr("Downloading workout from Intervals.icu..."));
    connect(replyIntervalsIcuZwo, SIGNAL(finished()), this, SLOT(onIntervalsIcuWorkoutDownloaded()));
}


//---------------------------------------------------------------------------------
void MainWindow::onIntervalsIcuWorkoutDownloaded() {

    if (!replyIntervalsIcuZwo) return;

    const QNetworkReply::NetworkError err = replyIntervalsIcuZwo->error();
    if (err != QNetworkReply::NoError) {
        LOG_WARN("MainWindow", QStringLiteral("Intervals.icu ZWO download error: ") + replyIntervalsIcuZwo->errorString());
        ui->widget_bottomMenu->setGeneralMessage(
            tr("Could not download workout from Intervals.icu: %1")
                .arg(replyIntervalsIcuZwo->errorString()), 5000);
        replyIntervalsIcuZwo->deleteLater();
        replyIntervalsIcuZwo = nullptr;
        return;
    }

    const QByteArray data = replyIntervalsIcuZwo->readAll();
    replyIntervalsIcuZwo->deleteLater();
    replyIntervalsIcuZwo = nullptr;

    Workout workout = ImporterWorkoutZwo::importFromByteArray(data, m_pendingIntervalsWorkoutId);
    if (workout.getLstInterval().isEmpty()) {
        ui->widget_bottomMenu->setGeneralMessage(
            tr("Workout import failed: no intervals found in the downloaded file."), 5000);
        return;
    }

    // Sanitize the workout name so it is safe to use as a filename
    QString safeName = workout.getName();
    safeName.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"));
    if (safeName.isEmpty())
        safeName = QStringLiteral("intervals_") + m_pendingIntervalsWorkoutId;

    // Save the parsed workout to the user's Intervals folder so it appears in the list
    const QString intervalsFolder = Util::getSystemPathWorkout() + QDir::separator() + QStringLiteral("intervals");
    if (!QDir().mkpath(intervalsFolder)) {
        ui->widget_bottomMenu->setGeneralMessage(
            tr("Could not create intervals folder: %1").arg(intervalsFolder), 5000);
        return;
    }

    // Loop until we find a filename that does not already exist
    QString uniqueSafeName = safeName;
    for (int n = 1; QFile::exists(intervalsFolder + QDir::separator() + uniqueSafeName + QStringLiteral(".workout")); ++n)
        uniqueSafeName = safeName + QStringLiteral("_") + QString::number(n);
    const QString filePath = intervalsFolder + QDir::separator() + uniqueSafeName + QStringLiteral(".workout");

    if (!XmlUtil::createWorkoutXml(workout, filePath)) {
        ui->widget_bottomMenu->setGeneralMessage(
            tr("Could not save workout to disk: %1").arg(filePath), 5000);
        return;
    }

    // Refresh the workout list and navigate to the imported workout
    ui->tab_workout1->refreshUserWorkout();
    ui->tabWidget_workout->setCurrentIndex(0);
    ui->tab_workout1->setFilterWorkoutName(workout.getName());
    ftb->setCurrentIndex(0);

    ui->widget_bottomMenu->setGeneralMessage(
        tr("Workout '%1' imported from Intervals.icu.").arg(workout.getName()), 5000);
}


//---------------------------------------------------------------------------------
void MainWindow::reloadPlanWebView() {

    if (!account->intervals_icu_athlete_id.isEmpty()) {
        ui->webView_plan->setUrl(QUrl(urlIntervalsIcuCalendar.arg(account->intervals_icu_athlete_id)));
    } else {
        ui->webView_plan->setHtml(
            QStringLiteral(
                "<html><body style='font-family:sans-serif;text-align:center;padding-top:60px;'>"
                "<h2>Intervals.icu Calendar</h2>"
                "<p>No Intervals.icu credentials configured.</p>"
                "<p>Open <b>Preferences</b> and enter your athlete ID and API key "
                "in the Connectivity section.</p>"
                "</body></html>"
            )
        );
    }
}


//---------------------------------------------------------------------------------
void MainWindow::showPlanContextMenu(const QPoint &pos) {

    QMenu *menu = new QMenu(ui->webView_plan);
    menu->addAction(tr("Refresh"), this, SLOT(reloadPlanWebView()));
    menu->exec(ui->webView_plan->mapToGlobal(pos));
    menu->deleteLater();
}

// trigger after a .zwo file downloaded from Intervals.icu is saved
//---------------------------------------------------------------------------------
void MainWindow::goToWorkoutNameFilterFromIntervals(const QString &workoutName) {

    qDebug() << "Intervals.icu workout downloaded:" << workoutName;

    // Parse the raw .zwo file saved by TabIntervalsIcu and convert it to .workout format
    const QString zwoDir  = Util::getSystemPathWorkout() + QDir::separator() + QStringLiteral("intervals_icu");
    const QString zwoPath = zwoDir + QDir::separator() + workoutName + QStringLiteral(".zwo");

    QFile zwoFile(zwoPath);
    if (zwoFile.open(QIODevice::ReadOnly)) {
        const QByteArray zwoData = zwoFile.readAll();
        zwoFile.close();

        Workout imported = ImporterWorkoutZwo::importFromByteArray(zwoData, workoutName);
        if (!imported.getLstInterval().isEmpty()) {
            // Sanitise name for filesystem use
            QString safeName = imported.getName();
            safeName.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"));
            if (safeName.isEmpty())
                safeName = workoutName;

            const QString workoutDir = Util::getSystemPathWorkout() + QDir::separator() + QStringLiteral("intervals");
            QDir().mkpath(workoutDir);

            // Find a unique filename
            QString uniqueName = safeName;
            for (int n = 1; QFile::exists(workoutDir + QDir::separator() + uniqueName + QStringLiteral(".workout")); ++n)
                uniqueName = safeName + QStringLiteral("_") + QString::number(n);

            const QString destPath = workoutDir + QDir::separator() + uniqueName + QStringLiteral(".workout");
            if (XmlUtil::createWorkoutXml(imported, destPath)) {
                ui->tab_workout1->refreshUserWorkout();
                ui->tabWidget_workout->setCurrentIndex(0);
                ui->tab_workout1->setFilterWorkoutName(imported.getName());
                ftb->setCurrentIndex(0);
                ui->widget_bottomMenu->setGeneralMessage(
                    tr("Workout '%1' imported from Intervals.icu.").arg(imported.getName()), 5000);
                return;
            }
        }
        LOG_WARN("MainWindow", QStringLiteral("goToWorkoutNameFilterFromIntervals: ZWO parse/save failed for ") + zwoPath);
    } else {
        LOG_WARN("MainWindow", QStringLiteral("goToWorkoutNameFilterFromIntervals: could not open ZWO file ") + zwoPath);
    }

    // Fallback: refresh list even if parsing failed
    ui->tab_workout1->refreshUserWorkout();
    ui->tabWidget_workout->setCurrentIndex(0);
    ui->tab_workout1->setFilterWorkoutName(workoutName);
    ftb->setCurrentIndex(0);
}


// ─────────────────────────────────────────────────────────────────────────────
// Trainerweb integration (#121)
// ─────────────────────────────────────────────────────────────────────────────

#ifndef GC_WASM_BUILD
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void MainWindow::onTrainerwebDownloadRequested(QWebEngineDownloadRequest *download)
{
    const QUrl url        = download->url();
    const QString host    = url.host();
    const QString fileName = QFileInfo(download->downloadFileName()).fileName();
    const QString suffix  = QFileInfo(fileName).suffix().toLower();

    // Only intercept downloads from the Trainerweb Firebase domain.
    // Let other pages' downloads proceed untouched.
    if (!host.contains(QStringLiteral("trainerdb-84bdb"), Qt::CaseInsensitive)) {
        download->accept();
        return;
    }

    // Only handle recognised workout formats.
    if (suffix != QLatin1String("zwo") &&
        suffix != QLatin1String("erg") &&
        suffix != QLatin1String("mrc")) {
        download->accept();
        return;
    }

    // Redirect the download to a temporary directory and accept it.
    download->setDownloadDirectory(QDir::tempPath());
    download->setDownloadFileName(fileName);
    download->accept();

    // Import the file once the download is complete.
    connect(download, &QWebEngineDownloadRequest::isFinishedChanged, this,
            [this, download, suffix]() {
        if (!download->isFinished())
            return;
        if (download->state() != QWebEngineDownloadRequest::DownloadCompleted) {
            ui->widget_bottomMenu->setGeneralMessage(
                tr("Trainerweb download failed."), 4000);
            return;
        }
        const QString filePath = download->downloadDirectory()
                                 + QDir::separator()
                                 + download->downloadFileName();
        if (suffix == QLatin1String("zwo")) {
            QFile f(filePath);
            if (f.open(QIODevice::ReadOnly)) {
                const QByteArray data = f.readAll();
                f.close();
                Workout w = ImporterWorkoutZwo::importFromByteArray(
                    data, QFileInfo(filePath).baseName());
                if (!w.getLstInterval().isEmpty()) {
                    saveAndNavigateToWorkout(w, QStringLiteral("trainerweb"));
                    return;
                }
            }
        } else {
            // .erg / .mrc — use the existing file-based importer.
            Workout w = ImporterWorkout::importWorkoutFromFile(filePath, account->FTP);
            if (!w.getLstInterval().isEmpty()) {
                saveAndNavigateToWorkout(w, QStringLiteral("trainerweb"));
                return;
            }
        }
        ui->widget_bottomMenu->setGeneralMessage(
            tr("Could not import Trainerweb workout: no intervals found."), 5000);
    });
}
#else  // Qt 5 — Trainerweb download interception uses Qt 6-only APIs
void MainWindow::onTrainerwebDownloadRequested(QWebEngineDownloadItem *download)
{
    download->accept();
}
#endif // QT_VERSION
#endif // GC_WASM_BUILD

void MainWindow::saveAndNavigateToWorkout(const Workout &workout, const QString &subFolder)
{
    QString safeName = workout.getName();
    safeName.replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")),
                     QStringLiteral("_"));
    if (safeName.isEmpty())
        safeName = subFolder + QStringLiteral("_workout");

    const QString destDir = Util::getSystemPathWorkout() + QDir::separator() + subFolder;
    QDir().mkpath(destDir);

    // Find a unique filename so existing workouts are not overwritten.
    QString uniqueName = safeName;
    for (int n = 1;
         QFile::exists(destDir + QDir::separator() + uniqueName + QStringLiteral(".workout"));
         ++n)
        uniqueName = safeName + QStringLiteral("_") + QString::number(n);

    const QString destPath = destDir + QDir::separator() + uniqueName + QStringLiteral(".workout");
    if (XmlUtil::createWorkoutXml(workout, destPath)) {
        ui->tab_workout1->refreshUserWorkout();
        ui->tabWidget_workout->setCurrentIndex(0);
        ui->tab_workout1->setFilterWorkoutName(workout.getName());
        ftb->setCurrentIndex(0);
        ui->widget_bottomMenu->setGeneralMessage(
            tr("Workout '%1' imported from Trainerweb.").arg(workout.getName()), 5000);
    } else {
        ui->widget_bottomMenu->setGeneralMessage(
            tr("Could not save Trainerweb workout to disk."), 5000);
    }
}

/////////////////////////////////////////////////////////////////////////////////
void MainWindow::tryAdvanceWorkoutQueue()
{
    if (m_workoutQueue->isEmpty())
        return;

    const QString nextName = m_workoutQueue->name(0);
    const QString nextPath = m_workoutQueue->filePath(0);
    WorkoutCountdownDialog countdown(nextName, 60, this);
    if (countdown.exec() != QDialog::Accepted) {
        // User chose "Cancel Queue" — clear the remaining queue so it doesn't
        // auto-advance after subsequent workouts.
        if (countdown.dialogResult() == WorkoutCountdownDialog::Result::Cancelled)
            m_workoutQueue->clear();
        return;
    }

    // Defer via singleShot so this stack frame fully unwinds before
    // the next workout begins — avoids unbounded recursion with long queues.
    QTimer::singleShot(0, this, [this, nextPath]() {
        XmlUtil xmlNext;
        Workout next = xmlNext.parseSingleWorkoutXml(nextPath);
        m_workoutQueue->dequeueFilePath();
        if (!next.getName().isEmpty()) {
            executeWorkout(next);
        } else {
            QMessageBox::warning(this,
                                 tr("Queue Error"),
                                 tr("Could not load the next queued workout:\n%1").arg(nextPath));
        }
    });
}

/////////////////////////////////////////////////////////////////////////////////
void MainWindow::workoutExecuting() {

    this->setDisabled(true);
    isInsideWorkout = true;
    qDebug() << "WORKOUT EXECUTING************";


    for (int i=0; i<ftb->getNumberOfTabs(); i++)
        ftb->setTabEnabled(i, false);

}
/////////////////////////////////////////////////////////////////////
void MainWindow::workoutOver() {

    this->setDisabled(false);
    isInsideWorkout = false;
    qDebug() << "WORKOUT DONE************";

    for (int i=0; i<ftb->getNumberOfTabs(); i++)
        ftb->setTabEnabled(i, true);

    if (account->enable_studio_mode) {
        enableStudioMode(true);
    }

}



/////////////////////////////////////////////////////////////////////////////////
void MainWindow::setFlagFtpChanged() {

    ftpChanged = true;

}

/////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::leftMenuChanged(int tabSelected) {

    //    qDebug() << "leftMenuChanged" << tabSelected;

    //    if (tabSelected == 0) {
    //        ui->label_headerMain->setText(tr("Workout"));
    //        ui->label_iconMenu->setStyleSheet("QLabel#label_iconMenu{image: url(:/image/icon/road);border-radius: 1px;}");
    //    }
    //    else if (tabSelected == 1) {
    //        ui->label_headerMain->setText(tr("Plan"));
    //        ui->label_iconMenu->setStyleSheet("QLabel#label_iconMenu{image: url(:/image/icon/calendar);border-radius: 1px;}");
    //    }
    //    else if (tabSelected == 2) {
    //        ui->label_headerMain->setText(tr("Profile"));
    //        ui->label_iconMenu->setStyleSheet("QLabel#label_iconMenu{image: url(:/image/icon/user);border-radius: 1px;}");
    //    }
    //    else if (tabSelected == 3) {
    //        ui->label_headerMain->setText(tr("Gear"));
    //        ui->label_iconMenu->setStyleSheet("QLabel#label_iconMenu{image: url(:/image/icon/gear);border-radius: 1px;}");
    //    }



    if (currentIndexLeftMenu == 3 && ftpChanged) { ///Also check that FTP has changed..
        qDebug() << "-*-*-*-OK FTP CHANGED, RECALCULATE!";
        emit ftpAndTabProfileChanged();
        ftpChanged = false;
    }

    // The Profile (page 4) and Settings (page 5) tabs were removed from the tab
    // bar, but their stacked-widget pages remain so the existing web views keep
    // working for code that still references them. Map the visible tabs to their
    // pages: 0 Workout, 1 Intervals.icu, 2 Plan, 3 Studio, 4 Sensors(=page 7),
    // 5 History(=page 6).
    static const int tabToPage[] = {0, 1, 2, 3, 7, 6};
    const int pageIndex = (tabSelected >= 0 && tabSelected < 6)
                              ? tabToPage[tabSelected]
                              : tabSelected;
    ui->stackedWidget_menu->setCurrentIndex(pageIndex);
    currentIndexLeftMenu = tabSelected;

    // Refresh saved sensors each time the Sensors tab is opened.
    if (tabSelected == 4) {
        if (auto *sw = qobject_cast<SensorsWidget*>(ui->sensorsWidget))
            sw->reload();
    }
    // Refresh studio settings each time the Studio tab is opened.
    if (tabSelected == 3) {
        if (auto *sw = qobject_cast<StudioWidget*>(ui->studioWidget))
            sw->reload();
    }
}



//------------------------------------------------------------
void MainWindow::createWebChannelZone() {

    qDebug() << "createWebChannelZone";

    QFile webChannelJsFile(":/qtwebchannel/qwebchannel.js");
    if(  !webChannelJsFile.open(QIODevice::ReadOnly) ) {
        LOG_WARN("MainWindow", QStringLiteral("Could not open qwebchannel.js: ") + webChannelJsFile.errorString());
    }
    else {
        qDebug() << "OK webEngineProfile";
        QByteArray webChannelJs = webChannelJsFile.readAll();
        webChannelJs.append(
                    "\n"
                    "var zoneObject"
                    "\n"
                    "new QWebChannel(qt.webChannelTransport, function(channel) {"
                    "     zoneObject = channel.objects.zoneObject;"
                    "});"
                    "\n"
                    );

        QWebChannel *channel = new QWebChannel(ui->webView_zones);
        QWebEngineScript script;
        script.setSourceCode(webChannelJs);
        script.setName("qwebchannel.js");
        script.setWorldId(QWebEngineScript::MainWorld);
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setRunsOnSubFrames(false);

        ui->webView_zones->page()->scripts().insert(script);
        ui->webView_zones->page()->setWebChannel(channel);
        channel->registerObject("zoneObject", zoneObject);
    }
}


//------------------------------------------------------------
void MainWindow::createWebChannelPlan() {

    qDebug() << "createWebChannelPlan";

    QFile webChannelJsFile(":/qtwebchannel/qwebchannel.js");
    if(  !webChannelJsFile.open(QIODevice::ReadOnly) ) {
        LOG_WARN("MainWindow", QStringLiteral("Could not open qwebchannel.js: ") + webChannelJsFile.errorString());
    }
    else {
        qDebug() << "OK webEngineProfile";
        QByteArray webChannelJs = webChannelJsFile.readAll();
        webChannelJs.append(
                    "\n"
                    "var planObject"
                    "\n"
                    "new QWebChannel(qt.webChannelTransport, function(channel) {"
                    "     planObject = channel.objects.planObject;"
                    "});"
                    "\n"
                    );

        QWebChannel *channel = new QWebChannel(ui->webView_plan);
        QWebEngineScript script;
        script.setSourceCode(webChannelJs);
        script.setName("qwebchannel.js");
        script.setWorldId(QWebEngineScript::MainWorld);
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setRunsOnSubFrames(false);

        // Navigation override: links containing "forum" or "cms" are opened in the system browser
        QStringList lstExternal = {"forum", "cms" };
        MyQWebEnginePage *myPage = new MyQWebEnginePage(ui->webView_plan);
        myPage->setExternalList(lstExternal);
        ui->webView_plan->setPage(myPage);

        ui->webView_plan->page()->scripts().insert(script);
        ui->webView_plan->page()->setWebChannel(channel);
        channel->registerObject("planObject", planObject);
    }
}


//------------------------------------------------------------
void MainWindow::createWebChannelSettings() {

    qDebug() << "createWebChannelSettings";

    QFile webChannelJsFile(":/qtwebchannel/qwebchannel.js");
    if(  !webChannelJsFile.open(QIODevice::ReadOnly) ) {
        LOG_WARN("MainWindow", QStringLiteral("Could not open qwebchannel.js: ") + webChannelJsFile.errorString());
    }
    else {
        qDebug() << "OK webEngineProfile";
        QByteArray webChannelJs = webChannelJsFile.readAll();
        webChannelJs.append(
                    "\n"
                    "var MainWindow"
                    "\n"
                    "new QWebChannel(qt.webChannelTransport, function(channel) {"
                    "     MainWindow = channel.objects.MainWindow;"
                    "});"
                    "\n"
                    );

        QWebChannel *channel = new QWebChannel(ui->webView_settings);
        QWebEngineScript script;
        script.setSourceCode(webChannelJs);
        script.setName("qwebchannel.js");
        script.setWorldId(QWebEngineScript::MainWorld);
        script.setInjectionPoint(QWebEngineScript::DocumentCreation);
        script.setRunsOnSubFrames(false);

        ui->webView_settings->page()->scripts().insert(script);
        ui->webView_settings->page()->setWebChannel(channel);
        channel->registerObject("MainWindow", this);
    }
}


//------------------------------------------------------------
void MainWindow::createWebChannelStudio() {

    qDebug() << "createWebChannelStudio";

    QFile webChannelJsFile(":/qtwebchannel/qwebchannel.js");
    if(  !webChannelJsFile.open(QIODevice::ReadOnly) ) {
        LOG_WARN("MainWindow", QStringLiteral("Could not open qwebchannel.js: ") + webChannelJsFile.errorString());
    }
    else {
        qDebug() << "OK webEngineProfile";
        QByteArray webChannelJs = webChannelJsFile.readAll();
        webChannelJs.append(
                    "\n"
                    "var MainWindow"
                    "\n"
                    "new QWebChannel(qt.webChannelTransport, function(channel) {"
                    "     MainWindow = channel.objects.MainWindow;"
                    "});"
                    "\n"
                    );

    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::fillSettingPage()  {

    qDebug() << "fillSettingPage -  Set the pmUseForCadence " << account->use_pm_for_cadence;

    QString jsCode = QString("$('#switch-pm-cadence').bootstrapSwitch('state', %1);").arg(account->use_pm_for_cadence);
    jsCode += QString("$('#switch-pm-speed').bootstrapSwitch('state', %1);").arg(account->use_pm_for_speed);

    qDebug() << "jsCodeISL:" << jsCode;

    ui->webView_settings->page()->runJavaScript(
        "if(typeof window.$==='function'){" + jsCode + "}");
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::updateVecStudio(QVector<UserStudio> vecUserStudio) {

    qDebug() << "zzzzz MainWindow::updateVecStudio";
    this->vecUserStudio = vecUserStudio;
}




//QString displayName;  = 0
//int FTP;              = 1
//int LTHR;             = 2
//int hrID;             = 3
//int power             = 4
//int cadenceID;        = 5
//int speedID;          = 6
//int fecID;            = 7
//int wheelCircMM;      = 8
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::updateFieldForUser(int riderID, int fieldNumber, QVariant value) {

    qDebug() << "updateDisplayNameForUser" << riderID << "fieldNumber" << fieldNumber << "value" << value;


    qDebug() << "disablePowerCurveForUser" << riderID;
    if (riderID >= vecUserStudio.size())
        return;
    UserStudio myUserStudio = vecUserStudio.at(riderID-1);

    if (fieldNumber == 0) {
        myUserStudio.setDisplayName(value.toString());
    }
    else if (fieldNumber == 1) {
        myUserStudio.setFTP(value.toInt());
    }
    else if (fieldNumber == 2) {
        myUserStudio.setLTHR(value.toInt());
    }
    else if (fieldNumber == 3) {
        myUserStudio.setHrID(value.toInt());
    }
    else if (fieldNumber == 4) {
        myUserStudio.setPowerID(value.toInt());
    }
    else if (fieldNumber == 5) {
        myUserStudio.setCadenceID(value.toInt());
    }
    else if (fieldNumber == 6) {
        myUserStudio.setSpeedID(value.toInt());
    }
    else if (fieldNumber == 7) {
        myUserStudio.setFecID(value.toInt());
    }
    else if (fieldNumber == 8) {
        myUserStudio.setWheelCircMM(value.toInt());
    }

    vecUserStudio.replace(riderID-1, myUserStudio);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::enableStudioMode(bool enable) {

    account->enable_studio_mode = enable;

    //    nb_user_studio

    qDebug() << "Enable Studio Mode!" << enable;


    if (enable) {
        this->setWindowTitle("MaximumTrainer - Studio");
    }
    else {
        this->setWindowTitle("MaximumTrainer");
    }

    // Disable the Sensors tab (index 4) while in studio mode: sensor pairing is
    // per-rider and not meaningful in the multi-rider studio view.
    ftb->setTabEnabled(4, !enable);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::setNumberUserStudio(int nbUser) {

    qDebug() << "setNumberUserStudio" << nbUser;
    account->nb_user_studio = nbUser;

}

/////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::loadConfigStudio() {

    qDebug() << "loadConfigStudio";

    //load path
    QSettings settings;
    settings.beginGroup("studiopath");
    QString path = settings.value("loadPath", Util::getMaximumTrainerDocumentPath() ).toString();
    settings.endGroup();

    QString file = QFileDialog::getOpenFileName(this, tr("Load Studio Profile"),
                                                path,
                                                tr("Studio Save File(*.xml)"));
    if (file.isEmpty())
        return;



    //Parse File and reset QWebView with QVector
    XmlUtil *xmlUtil = new XmlUtil(this);
    vecUserStudio = xmlUtil->parseUserStudioFile(file);
    ui->widget_bottomMenu->setGeneralMessage(QString(tr("Studio Profile %1 loaded")).arg(file), 5000);


    //save path
    settings.beginGroup("studiopath");
    settings.setValue("loadPath", file);
    settings.endGroup();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::saveConfigStudio() {

    qDebug() << "saveConfigStudio";

    //load path
    QSettings settings;
    settings.beginGroup("studiopath");
    QString path = settings.value("loadPath", Util::getMaximumTrainerDocumentPath() ).toString();
    settings.endGroup();

    QString file = QFileDialog::getSaveFileName(this, tr("Save Studio Profile As"),
                                                path,
                                                tr("Studio Save File(*.xml)"));

    if (file.isEmpty())
        return;

    qDebug() << "Saving FILE" << file;

    //Save Studio User to XML File
    bool success = XmlUtil::saveUserStudioFile(vecUserStudio, file);
    if (success) {
        ui->widget_bottomMenu->setGeneralMessage(QString(tr("Studio Profile saved as %1")).arg(file), 5000);
    }

    //save path
    settings.beginGroup("studiopath");
    settings.setValue("loadPath", file);
    settings.endGroup();
}


/////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::updateZoneInterface() {

    qDebug() << "update zone interface!********************";

    QString jsToExecute = QString("$('#FTP').val( '%1' ); ").arg((QString::number(account->FTP)));
    jsToExecute += QString("$('#LTHR').val( '%1' ); ").arg((QString::number(account->LTHR)));
    //    ui->webView_zones->page()->mainFrame()->documentElement().evaluateJavaScript(jsToExecute + "; null");
    ui->webView_zones->page()->runJavaScript(
        "if(typeof window.$==='function'){" + jsToExecute + "}");

}

/////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::showWorkoutCreator() {

    ui->tabWidget_workout->setCurrentIndex(1);
}

/////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::showWorkoutList() {

    qDebug() << "showWorkoutList!*";
    ui->tabWidget_workout->setCurrentIndex(0);
}


/////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::loadSettings() {



    QSettings settings;


    settings.beginGroup("MainWindow");
    resize(settings.value("size", QSize(1150, 700)).toSize());
    move(settings.value("pos", QPoint(200, 200)).toPoint());
    bool wasMaximized = settings.value("maximized", false).toBool();
    //    resize( QSize(1024, 768));
    //    move(QPoint(200, 200));
    settings.endGroup();


    if (wasMaximized) {
        this->showMaximized();
    }
    else {
        this->showNormal();
    }
}




/////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::saveSettings() {


    QSettings settings;

    settings.beginGroup("MainWindow");
    settings.setValue("size", size());
    settings.setValue("pos", pos());

    if (this->isMaximized()) {
        settings.setValue("maximized", true);
    }
    else {
        settings.setValue("maximized", false);
    }

    settings.endGroup();



}





void MainWindow::sendDataToSettingsOrStudioPage(int deviceType, int numberDeviceFound, QList<int> lstDevicePaired, QList<int> lstTypeDevicePaired, bool fromStudioPage) {

    qDebug() << "sendDataToSettingsOrStudioPage" << deviceType << numberDeviceFound << "..." << fromStudioPage;
    //transform List to JS format
    QStringList temp;
    std::transform(lstDevicePaired.begin(), lstDevicePaired.end(), std::back_inserter(temp), [](int i){ return QString::number(i); });

    QStringList temp2;
    std::transform(lstTypeDevicePaired.begin(), lstTypeDevicePaired.end(), std::back_inserter(temp2), [](int i){ return QString::number(i); });

    QString script("foundSensor(%1, %2, [%3], [%4], %5);");
    QString arg1= QString::number(deviceType);
    QString arg2= QString::number(lstDevicePaired.size());
    QString arg3 = temp.join(',');
    QString arg4 = temp2.join(',');
    QString arg5 = QString::number(fromStudioPage);

    QString jsToRun = script.arg(arg1, arg2, arg3, arg4, arg5);
    qDebug() << "here is the script to run:" << jsToRun;

    // The studio page is now a native widget; only the (legacy) settings web
    // page still consumes this JS callback.
    if (!fromStudioPage) {
        ui->webView_settings->page()->runJavaScript(
            "if(typeof foundSensor==='function'){" + jsToRun + "}");
    }

}

//////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::closeEvent(QCloseEvent *event) {


    qDebug() << "closeEvent";

    //    Qt::WindowFlags flags = Qt::Window;
    //    if (settings->forceWorkoutWindowOnTop)
    //        flags = flags | Qt::WindowStaysOnTopHint;
    //    this->setWindowFlags(flags);


    if (isInsideWorkout) {
        QMessageBox msgBox;
        msgBox.setWindowFlags(msgBox.windowFlags() | Qt::WindowStaysOnTopHint);
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setText(tr("A workout is still active."));
        msgBox.setInformativeText(tr("Please close any active workout before leaving MaximumTrainer."));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.exec();
        event->ignore();
        return;
    }

    // Wait for hub to close
    //Stop Hub thread
    //this->closeCSM(true);


    //Save Studio User to XML File
    XmlUtil::saveUserStudioFile(vecUserStudio, "");

    //Save filter Field
    ui->tab_workout1->saveFilterFields();

    Q_UNUSED(event);
    saveSettings();
    this->setVisible(false);

    savingWindow.show();

    //Save Settings and Account List workout done to xml file
    XmlUtil::saveLocalSaveFile(account);


    // Put updated account data to server (skip when running in offline mode –
    // there is no server to reach and the blocking event loop would time out).
    if (!account->isOffline) {
        replySaveAccount = UserDAO::putAccount(account);
        QObject::connect(replySaveAccount, SIGNAL(finished()), this, SLOT(postDataAccountFinished()) );
        loop.exec(); //dont leave until data uploaded to server
    }


    savingWindow.hide();
    qDebug () << "closeEvent Done mainWindow";

    // quitOnLastWindowClosed is disabled (see main.cpp), so quit explicitly
    // once the main window has finished closing.
    qApp->quit();
}

/////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::postDataAccountFinished() {

    //success, process data
    if (replySaveAccount->error() == QNetworkReply::NoError) {
        qDebug() << "no error postDataAccountFinished!";
        loop.quit();
    }

    // error, retry request
    else {
        if (saveAccountTry > 5) {
            savingWindow.setMessage("Could not save on server");
            LOG_WARN("MainWindow", QStringLiteral("putAccount: 5 retries exhausted — giving up"));
            loop.quit();
        }
        else {
            saveAccountTry++;
            LOG_WARN("MainWindow",
                     QStringLiteral("putAccount error (attempt ") + QString::number(saveAccountTry)
                     + QStringLiteral("): ") + replySaveAccount->errorString());
            replySaveAccount = UserDAO::putAccount(account);
            connect(replySaveAccount, SIGNAL(finished()), this, SLOT(postDataAccountFinished()) );
        }
    }

}

//QMENUBAR
/////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::on_actionExit_triggered()
{
    this->close();
}


//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
void MainWindow::on_actionAbout_MT_triggered()
{
    const QString appName = QCoreApplication::applicationName();
    QString nameWithVersion = appName + " " + QCoreApplication::applicationVersion();
    QString copyright = tr("Copyright 2013-2019 maximus321")
                      + "<br/>"
                      + tr("Copyright 2019-present MaximumTrainer maintainers");
    const QString repoUrl = QStringLiteral("https://github.com/MaximumTrainer/MaximumTrainer_Redux");
    QString openSource = tr("%1 is free and open source. Contributions are welcome — "
                            "fork it, open an issue, or send a pull request on "
                            "<a href=\"%2\">GitHub</a>.")
                            .arg(appName, repoUrl);
    this->setStyleSheet("QMessageBox { messagebox-text-interaction-flags: 5; }");
    QMessageBox::about(this,
                       tr("About ") + appName,
                       "<b>"+ nameWithVersion + "</b> - " + tr("Build on ")  + Environnement::getDateBuilded() + "<br/>" +
                       copyright + "<hr/>" +
                       openSource + "<hr/>" +
                       tr("Normalized Power®(NP), Training Stress Score®(TSS) and Intensity Factor®(IF) are registered trademarks of Peaksware LLC."));


}
//-----------------------------------------------
void MainWindow::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this);



}
//-----------------------------------------------
void MainWindow::on_actionRequest_Help_triggered()
{
    // Open the online user guide in the user's default browser.
    QDesktopServices::openUrl(QUrl(QStringLiteral(
        "https://maximumtrainer.github.io/MaximumTrainer_Redux/user-guide.html")));
}
//-----------------------------------------------
void MainWindow::on_actionKeyboard_Shortcuts_triggered()
{
    DialogKeyboardShortcuts dlg(this);
    dlg.exec();
}
//-----------------------------------------------
void MainWindow::on_actionCheck_for_Updates_triggered()
{
    // Abort any in-flight check before starting a new one to prevent races.
    if (replyVersionCheck) {
        replyVersionCheck->abort();
        replyVersionCheck->deleteLater();
        replyVersionCheck = nullptr;
    }

    replyVersionCheck = VersionDAO::getVersion();
    if (!replyVersionCheck) return;

    connect(replyVersionCheck, &QNetworkReply::finished,
            this, &MainWindow::slotVersionCheckFinished);
}
//-----------------------------------------------
void MainWindow::slotVersionCheckFinished()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();

    // Ignore stale replies from an aborted request.
    if (reply != replyVersionCheck) return;
    replyVersionCheck = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, tr("Update Check"),
            tr("Unable to check for updates:\n%1").arg(reply->errorString()));
        return;
    }

    const QString latestTag = Util::parseJsonObjectVersion(QString::fromUtf8(reply->readAll()));
    if (latestTag.isEmpty()) {
        QMessageBox::warning(this, tr("Update Check"),
            tr("Could not determine latest version. Please try again later."));
        return;
    }

    if (Util::isVersionNewer(Environnement::getVersion(), latestTag)) {
        UpdateDialog dlg(latestTag, this);
        dlg.exec();
    } else {
        QMessageBox::information(this, tr("Check for Updates"),
            tr("You are up to date (%1).").arg(Environnement::getVersion()));
    }
}
//-----------------------------------------------
void MainWindow::on_actionPreferences_triggered()
{

    dconfig->exec();

}
//-----------------------------------------------
void MainWindow::on_actionWorkout_triggered()
{
    Util::openWorkoutFolder("null");
}
//-----------------------------------------------
void MainWindow::on_actionHistory_triggered()
{
    ftb->setCurrentIndex(5); // History
}


//------------------------------------------------------
void MainWindow::addWorkoutToQueue(const Workout &workout)
{
    m_workoutQueue->addWorkout(workout.getFilePath(), workout.getName());
    m_queueDock->show();
    ui->widget_bottomMenu->setGeneralMessage(
        tr("\"%1\" added to queue (%2 workout(s)).")
            .arg(workout.getName())
            .arg(m_workoutQueue->count()),
        4000);
}



//------------------------------------------------------
void MainWindow::on_actionCreate_New_triggered()
{


    ui->tabWidget_workout->setCurrentIndex(1);
    ftb->setCurrentIndex(0);
    ui->tab_create->resetWorkout();


}



//------------------------------------------------------
void MainWindow::on_actionOpen_Ride_triggered()
{

    qDebug() << "Doing free ride!";


    QList<Interval> lstInterval;
    Interval interval;
    lstInterval.append(interval);

    Workout freeRide("null", Workout::OPEN_RIDE, lstInterval,
                     tr("Free Ride"), "MaximumTrainer", tr("Go and ride as long as you want"), "-", Workout::T_OTHERS);

    executeWorkout(freeRide);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::executeWorkout(Workout workout) {

    DialogConnectionMethod connDlg(this);
    if (connDlg.exec() != QDialog::Accepted)
        return;

    // ── Simulation path ───────────────────────────────────────────────────
    if (connDlg.selectedMethod() == DialogConnectionMethod::Simulation) {
        SimulatorHub *simHub = new SimulatorHub(this);

        // Show WorkoutDialog NON-MODALLY (window-modal via setWindowModality,
        // run on the main event loop instead of QDialog::exec()). The embedded
        // QWebEngine video player reparents widgets when it initialises, which
        // destroys and recreates the dialog's native window; inside a nested
        // exec() loop that window-destroy calls QEventLoop::exit() and tears the
        // dialog down. Running on the main loop removes that hazard. Post-workout
        // cleanup moves to the finished() handler below.
        WorkoutDialog *w = new WorkoutDialog(workout, lstRadio, vecUserStudio);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->setWindowModality(Qt::ApplicationModal);

        connect(simHub, SIGNAL(signal_hr(int,int)),               w, SLOT(HrDataReceived(int,int)));
        connect(simHub, SIGNAL(signal_cadence(int,int)),          w, SLOT(CadenceDataReceived(int,int)));
        connect(simHub, SIGNAL(signal_speed(int,double)),         w, SLOT(TrainerSpeedDataReceived(int,double)));
        connect(simHub, SIGNAL(signal_power(int,int)),            w, SLOT(PowerDataReceived(int,int)));
        connect(simHub, SIGNAL(signal_oxygen(int,double,double)), w, SLOT(OxygenValueChanged(int,double,double)));

        connect(w, SIGNAL(setLoad(int,double)),  simHub, SLOT(setLoad(int,double)));
        connect(w, SIGNAL(setSlope(int,double)), simHub, SLOT(setSlope(int,double)));
        connect(w, SIGNAL(stopDecodingMsgHub()), simHub, SLOT(stopDecodingMsg()));

        connect(w, SIGNAL(fitFileReady(QString, QString, QString)), this, SLOT(checkToUploadFile(QString,QString,QString)));
        connect(w, SIGNAL(ftp_lthr_changed()), this, SLOT(updateZoneInterface()));
        connect(w, SIGNAL(ftp_lthr_changed()), ui->tab_workout1, SLOT(updateTableViewMetrics()));
        connect(w, SIGNAL(ftpUserStudioChanged(QVector<UserStudio>)), this, SLOT(updateVecStudio(QVector<UserStudio>)));

        connect(w, &QDialog::finished, this, [this, simHub]() {
            workoutOver();
            simHub->stopDecodingMsg();
            delete simHub;
            ui->webView_achiev->reload();
            // Auto-advance to next queued workout if one exists
            tryAdvanceWorkoutQueue();
        });

        simHub->start();
        workoutExecuting();
        QApplication::restoreOverrideCursor();
        w->show();
        return;
    }

    // ── BTLE Device path ─────────────────────────────────────────────────
#ifndef GC_WASM_BUILD
    // If the user has saved sensors (Sensors tab), connect to all of them at
    // once via the connection-status dialog. With no saved sensors we fall
    // through to the legacy single-device scanner below.
    {
        const QMap<BtleSensorRole, BtleSavedSensor> savedSensors = BtleSensorStore::loadAll();
        if (!savedSensors.isEmpty()) {
            SensorConnectDialog connectDlg(savedSensors, account->wheel_circ, this);
            connect(&connectDlg, &SensorConnectDialog::openSensorPreferences,
                    this, [this, &connectDlg]() {
                        // "Manage Sensors" – cancel the connect flow and switch
                        // to the Sensors tab so the user can edit their devices.
                        connectDlg.reject();
                        ftb->setCurrentIndex(4);
                    });

            if (connectDlg.exec() != QDialog::Accepted)
                return;   // user cancelled / chose Manage Sensors

            QMap<BtleSensorRole, BtleHub*> hubsByRole = connectDlg.connectedHubs();
            if (!hubsByRole.isEmpty()) {
                connectDlg.detachHubs();   // we now own the hubs
                startWorkoutWithHubs(workout, hubsByRole);
                return;
            }
            // Empty result = "Skip": drop through to the legacy manual scanner.
        }
    }
#endif

    BtleScannerDialog scanner(this);
    if (scanner.exec() != QDialog::Accepted || !scanner.hasSelection())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);


    BtleHub *btleHub = new BtleHub(this);
    if (account->wheel_circ > 0)
        btleHub->setWheelCircumferenceMm(account->wheel_circ);

    {
        QEventLoop loop;
        connect(btleHub, &BtleHub::deviceConnected,    &loop, &QEventLoop::quit);
        connect(btleHub, &BtleHub::deviceDisconnected, &loop, &QEventLoop::quit);
        connect(btleHub, &BtleHub::connectionError,
                &loop, [&loop](const QString &) { loop.quit(); });
        btleHub->connectToDevice(scanner.selectedDevice());

        QApplication::restoreOverrideCursor();
        loop.exec();
    }

    if (!btleHub->isConnected()) {
        QMessageBox::warning(this,
                             tr("Connection Failed"),
                             tr("Could not connect to the selected Bluetooth device.\n"
                                "Please check the device is powered on and try again."));
        delete btleHub;
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Show WorkoutDialog NON-MODALLY (window-modal, run on the main event loop
    // instead of QDialog::exec()) so the embedded QWebEngine video player does
    // not tear down the dialog when it initialises. See the simulation path
    // above for the full rationale. Post-workout cleanup moves to finished().
    WorkoutDialog *w = new WorkoutDialog(workout, lstRadio, vecUserStudio);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowModality(Qt::ApplicationModal);

    connect(btleHub, SIGNAL(signal_hr(int,int)),               w, SLOT(HrDataReceived(int,int)));
    connect(btleHub, SIGNAL(signal_cadence(int,int)),          w, SLOT(CadenceDataReceived(int,int)));
    connect(btleHub, SIGNAL(signal_speed(int,double)),         w, SLOT(TrainerSpeedDataReceived(int,double)));
    connect(btleHub, SIGNAL(signal_power(int,int)),            w, SLOT(PowerDataReceived(int,int)));
    connect(btleHub, SIGNAL(signal_oxygen(int,double,double)), w, SLOT(OxygenValueChanged(int,double,double)));
    connect(btleHub, &BtleHub::signal_battery, w, &WorkoutDialog::batteryStatusReceived);

    // Surface BLE disconnections mid-workout so WorkoutDialog can pause and
    // the user sees the DOM reconnect overlay (WASM) or a status message.
    connect(btleHub, &BtleHub::connectionError, w, &WorkoutDialog::onBleConnectionError);

    connect(w, SIGNAL(setLoad(int,double)),  btleHub, SLOT(setLoad(int,double)));
    connect(w, SIGNAL(setSlope(int,double)), btleHub, SLOT(setSlope(int,double)));
    connect(w, SIGNAL(stopDecodingMsgHub()), btleHub, SLOT(stopDecodingMsg()));

#ifdef Q_OS_WASM
    // On WASM, BtleHub is aliased to BtleHubWasm which exposes scanForDevice().
    // The DOM overlay's Reconnect button routes through bleReconnectRequestC →
    // BtleHubWasm::scanForDevice(), but WorkoutDialog can also emit reconnectDevice()
    // if needed from Qt-side logic.
    connect(w, &WorkoutDialog::reconnectDevice, btleHub, &BtleHub::scanForDevice);
#endif

    connect(w, SIGNAL(fitFileReady(QString, QString, QString)), this, SLOT(checkToUploadFile(QString,QString,QString)));
    connect(w, SIGNAL(ftp_lthr_changed()), this, SLOT(updateZoneInterface()));
    connect(w, SIGNAL(ftp_lthr_changed()), ui->tab_workout1, SLOT(updateTableViewMetrics()));
    connect(w, SIGNAL(ftpUserStudioChanged(QVector<UserStudio>)), this, SLOT(updateVecStudio(QVector<UserStudio>)));

    connect(w, &QDialog::finished, this, [this, btleHub]() {
        workoutOver();
        btleHub->disconnectFromDevice();
        delete btleHub;
        ui->webView_achiev->reload();
        // Auto-advance to next queued workout if one exists
        tryAdvanceWorkoutQueue();
    });

    workoutExecuting();
    QApplication::restoreOverrideCursor();
    w->show();
}


#ifndef GC_WASM_BUILD
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::startWorkoutWithHubs(const Workout &workout,
                                      const QMap<BtleSensorRole, BtleHub*> &hubsByRole)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Show WorkoutDialog NON-MODALLY (window-modal, run on the main event loop)
    // for the same QWebEngine reparenting reason as the single-device path above.
    WorkoutDialog *w = new WorkoutDialog(workout, lstRadio, vecUserStudio);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowModality(Qt::ApplicationModal);

    // One physical device may back several roles (e.g. Trainer + Power), so the
    // map can hold the same hub under multiple keys. Collect the distinct hubs
    // for lifecycle management and to wire shared signals (battery/error) once.
    QSet<BtleHub*> uniqueHubs;
    for (BtleHub *hub : hubsByRole)
        uniqueHubs.insert(hub);

    for (BtleHub *hub : uniqueHubs) {
        connect(hub, &BtleHub::signal_battery,  w, &WorkoutDialog::batteryStatusReceived);
        connect(hub, &BtleHub::connectionError, w, &WorkoutDialog::onBleConnectionError);
        connect(w, SIGNAL(stopDecodingMsgHub()), hub, SLOT(stopDecodingMsg()));
    }

    // Track which hub already drives a metric so a trainer that also exposes
    // power/cadence/speed is not double-wired alongside a dedicated sensor on
    // the same physical device (which would double-count the rolling average).
    BtleHub *powerHub   = nullptr;
    BtleHub *cadenceHub = nullptr;
    BtleHub *speedHub   = nullptr;

    auto wirePower = [&](BtleHub *hub) {
        if (powerHub == hub) return;          // already wired from this device
        powerHub = hub;
        connect(hub, SIGNAL(signal_power(int,int)), w, SLOT(PowerDataReceived(int,int)));
    };
    auto wireCadence = [&](BtleHub *hub) {
        if (cadenceHub == hub) return;
        cadenceHub = hub;
        connect(hub, SIGNAL(signal_cadence(int,int)), w, SLOT(CadenceDataReceived(int,int)));
    };
    auto wireSpeed = [&](BtleHub *hub) {
        if (speedHub == hub) return;
        speedHub = hub;
        connect(hub, SIGNAL(signal_speed(int,double)), w, SLOT(TrainerSpeedDataReceived(int,double)));
    };

    // Wire the trainer first so resistance control is set up and its data takes
    // precedence; dedicated Power/CadenceSpeed sensors then fill any gaps.
    if (hubsByRole.contains(BtleSensorRole::Trainer)) {
        BtleHub *hub = hubsByRole.value(BtleSensorRole::Trainer);
        wirePower(hub);
        wireCadence(hub);
        wireSpeed(hub);
        connect(w, SIGNAL(setLoad(int,double)),  hub, SLOT(setLoad(int,double)));
        connect(w, SIGNAL(setSlope(int,double)), hub, SLOT(setSlope(int,double)));
    }
    if (hubsByRole.contains(BtleSensorRole::Power))
        wirePower(hubsByRole.value(BtleSensorRole::Power));
    if (hubsByRole.contains(BtleSensorRole::CadenceSpeed)) {
        BtleHub *hub = hubsByRole.value(BtleSensorRole::CadenceSpeed);
        wireCadence(hub);
        wireSpeed(hub);
    }
    if (hubsByRole.contains(BtleSensorRole::HeartRate))
        connect(hubsByRole.value(BtleSensorRole::HeartRate),
                SIGNAL(signal_hr(int,int)), w, SLOT(HrDataReceived(int,int)));
    if (hubsByRole.contains(BtleSensorRole::Oxygen))
        connect(hubsByRole.value(BtleSensorRole::Oxygen),
                SIGNAL(signal_oxygen(int,double,double)), w, SLOT(OxygenValueChanged(int,double,double)));

    connect(w, SIGNAL(fitFileReady(QString, QString, QString)), this, SLOT(checkToUploadFile(QString,QString,QString)));
    connect(w, SIGNAL(ftp_lthr_changed()), this, SLOT(updateZoneInterface()));
    connect(w, SIGNAL(ftp_lthr_changed()), ui->tab_workout1, SLOT(updateTableViewMetrics()));
    connect(w, SIGNAL(ftpUserStudioChanged(QVector<UserStudio>)), this, SLOT(updateVecStudio(QVector<UserStudio>)));

    connect(w, &QDialog::finished, this, [this, uniqueHubs]() {
        workoutOver();
        for (BtleHub *hub : uniqueHubs) {
            hub->disconnectFromDevice();
            delete hub;
        }
        ui->webView_achiev->reload();
        // Auto-advance to next queued workout if one exists
        tryAdvanceWorkoutQueue();
    });

    workoutExecuting();
    QApplication::restoreOverrideCursor();
    w->show();
}
#endif // GC_WASM_BUILD



//-------------------------------------------------------
QString MainWindow::loadPathImport() const {

    QSettings settings;

    settings.beginGroup("Importer");
    QString path = settings.value("loadPath", QDir::homePath() ).toString();
    settings.endGroup();

    return path;

}
//------------------------------------------------------------------
void MainWindow::savePathImport(const QString& filepath) {

    QSettings settings;

    settings.beginGroup("Importer");
    settings.setValue("loadPath", filepath);
    settings.endGroup();
}


//------------------------------------------------------------------------------
void MainWindow::on_actionSingle_Workout_triggered()
{
    QString file = QFileDialog::getOpenFileName(this, tr("Import"),
                                                loadPathImport(),
                                                tr("Workout Files(*.erg *.mrc)"));

    if (file.isEmpty())
        return;

    Workout importedWorkout = ImporterWorkout::importWorkoutFromFile(file, account->FTP);

    savePathImport(file);


    //open workout editor with Workout
    ui->tabWidget_workout->setCurrentIndex(1);
    ftb->setCurrentIndex(0);
    ui->tab_create->editWorkout(importedWorkout);


    ui->widget_bottomMenu->setGeneralMessage(QString(tr("Workout %1 successfully imported")).arg(file), 5000);


}

//------------------------------------------------------------------------------
void MainWindow::on_actionMultiple_Workouts_triggered()
{
    qDebug() << "Multiple Workouts importer";


    QString folder =  QFileDialog::getExistingDirectory(this, tr("Select Folder Containing Workout Files To Import (.erg and .mrc)"),
                                                        loadPathImport());


    if (folder.isEmpty())
        return;

    bool result = ImporterWorkout::batchImportWorkoutFromFolder(folder, account->FTP);

    if (result) {
        //Open Folder in explorer (finder)
        QDesktopServices::openUrl(QUrl("file:///" + folder + "/MT Workouts/"));
    }
}







////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::checkToUploadFile(const QString& filename, const QString& nameOnly, const QString& description) {

    qDebug() << "check to upload Fit file";

    // ── Plan adherence: auto-mark this workout as completed ──────────────────
    if (m_adherenceStore && !nameOnly.isEmpty()) {
        const QDate today = QDate::currentDate();
        m_adherenceStore->addCompleted(today, nameOnly, filename);
    }

    // Strava + Intervals.icu auto-upload (each has its own toggle). The
    // post-workout WorkoutDialog buttons remain for manual one-off uploads.

    // Strava — refresh the access token if it's expired (via the token Worker),
    // then upload. Access tokens live only 6 h, so check expires_at first.
    if (account->strava_auto_upload &&
        !account->strava_access_token.isEmpty() &&
        NetworkMonitor::instance()->isOnline()) {

        m_pendingStravaFile = filename;
        m_pendingStravaName = nameOnly;
        m_pendingStravaDesc = description;

        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const bool tokenValid = account->strava_token_expires_at > now + 60;
        if (tokenValid || account->strava_refresh_token.isEmpty()) {
            startStravaAutoUpload();
        } else {
            ui->widget_bottomMenu->setGeneralMessage(tr("Refreshing Strava token..."));
            QNetworkReply *refresh = StravaService::refreshToken(account->strava_refresh_token);
            if (!refresh) {
                startStravaAutoUpload();
            } else {
                connect(refresh, &QNetworkReply::finished, this, [this, refresh]() {
                    refresh->deleteLater();
                    if (refresh->error() == QNetworkReply::NoError) {
                        Util::parseJsonStravaObject(QString::fromUtf8(refresh->readAll()));
                        account->saveStravaCredentials();
                    } else {
                        LOG_WARN("MainWindow",
                                 QStringLiteral("Strava token refresh failed: ")
                                 + refresh->errorString());
                    }
                    startStravaAutoUpload();
                });
            }
        }
    }

    // Intervals.icu
    if (account->intervals_icu_auto_upload &&
        !account->intervals_icu_athlete_id.isEmpty() &&
        (!account->intervals_icu_api_key.isEmpty() ||
         !account->intervals_icu_access_token.isEmpty()) &&
        NetworkMonitor::instance()->isOnline()) {

        ui->widget_bottomMenu->setGeneralMessage(tr("Uploading your activity to Intervals.icu..."));
        IntervalsIcuService *svc = new IntervalsIcuService(this);
        svc->setCredentials(account->intervals_icu_api_key, account->intervals_icu_athlete_id);
        svc->setAccessToken(account->intervals_icu_access_token);
        const QString externalId = QFileInfo(filename).baseName();
        replyIntervalsIcuUpload = svc->uploadActivity(filename, nameOnly, externalId);
        if (replyIntervalsIcuUpload) {
            connect(replyIntervalsIcuUpload, SIGNAL(finished()), this, SLOT(slotIntervalsIcuUploadFinished()));
        }
        svc->deleteLater();
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kicks off the actual Strava upload (assumes a fresh access token). The reply
// is handled by the existing slotStravaUploadFinished → poll-status machinery.
void MainWindow::startStravaAutoUpload()
{
    ui->widget_bottomMenu->setGeneralMessage(tr("Uploading your activity to Strava..."));
    replyStravaUpload = ExtRequest::stravaUploadFile(
        account->strava_access_token,
        m_pendingStravaName,
        m_pendingStravaDesc,
        true,                              // indoor / trainer activity
        account->strava_private_upload,
        QStringLiteral("ride"),
        m_pendingStravaFile);
    if (replyStravaUpload)
        connect(replyStravaUpload, SIGNAL(finished()), this, SLOT(slotStravaUploadFinished()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::slotIntervalsIcuUploadFinished()
{
    qDebug() << "slotIntervalsIcuUploadFinished";

    if (replyIntervalsIcuUpload->error() == QNetworkReply::NoError) {
        ui->widget_bottomMenu->setGeneralMessage(
            tr("Your activity was successfully uploaded to Intervals.icu"), 5000);
    } else {
        const int httpStatus = replyIntervalsIcuUpload
            ->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 401) {
            ui->widget_bottomMenu->setGeneralMessage(
                tr("Intervals.icu upload failed: invalid API key (401)"), 5000);
        } else if (httpStatus == 409) {
            ui->widget_bottomMenu->setGeneralMessage(
                tr("Activity already present on Intervals.icu"), 5000);
        } else {
            ui->widget_bottomMenu->setGeneralMessage(
                "Intervals.icu: " + replyIntervalsIcuUpload->errorString(), 5000);
        }
        LOG_WARN("MainWindow",
                 QStringLiteral("Intervals.icu upload failed: ")
                 + replyIntervalsIcuUpload->errorString()
                 + QStringLiteral(" HTTP ") + QString::number(httpStatus));
    }
    replyIntervalsIcuUpload->deleteLater();
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::onNetworkOnlineChanged(bool isOnline)
{
    // Enable or disable the Intervals.icu sidebar tab (index 1).
    // setTabEnabled grays out the tab entry when offline so users see it is
    // temporarily unavailable rather than it simply vanishing.
    ftb->setTabEnabled(1, isOnline);

    // If the user is currently on the Intervals.icu tab and we just went
    // offline, navigate back to the Workout tab so they land on something
    // functional.
    if (!isOnline && currentIndexLeftMenu == 1) {
        ftb->setCurrentIndex(0);
        leftMenuChanged(0);
    }
}

void MainWindow::slotSystemThemeChanged()
{
    auto *account = qApp->property("Account").value<Account*>();
    if (!account) return;
    // Only react when the user has chosen "System" mode.
    if (account->app_theme == 2 /*System*/)
        AppTheme::apply(qApp, AppTheme::System);
}



////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::slotStravaUploadFinished()
{
    qDebug() << "slotStravaUploadFinished";


    //success, process data
    if (replyStravaUpload->error() == QNetworkReply::NoError) {
        qDebug() << "no error strava!";
        QByteArray arrayData =  replyStravaUpload->readAll();
        stravaUploadID = Util::parseIdJsonStravaUploadObject(QString(arrayData));
        qDebug() << "UPLOAD ID IS:" << stravaUploadID;
        timerCheckUploadStatus = new QTimer(this);
        connect(timerCheckUploadStatus, SIGNAL(timeout()), this, SLOT(slotStravaCheckUploadStatus()) );
        timerCheckUploadStatus->start(3000);
    }
    else {
        LOG_WARN("MainWindow",
                 QStringLiteral("Strava upload failed: ") + replyStravaUpload->errorString());
        ui->widget_bottomMenu->setGeneralMessage("Strava : " + replyStravaUpload->errorString(), 5000);
    }
    replyStravaUpload->deleteLater();

}

////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::slotStravaCheckUploadStatus() {

    qDebug() << "slotStravaCheckUploadStatus";

    replyStravaUploadStatus = ExtRequest::stravaCheckUploadStatus(account->strava_access_token, stravaUploadID );
    connect(replyStravaUploadStatus, SIGNAL(finished()), this, SLOT(slotStravaUploadStatusFinished()) );
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::slotStravaUploadStatusFinished() {

    qDebug() << "slotStravaUploadStatusFinished";


    QString msgReady = tr("Your activity was successfully uploaded to Strava");
    QString msgError = tr("There was an error processing your activity to Strava");


    //success, process data
    if (replyStravaUploadStatus->error() == QNetworkReply::NoError) {

        QByteArray arrayData =  replyStravaUploadStatus->readAll();
        int codeReturn = Util::parseStravaUploadStatus(QString(arrayData));
        // -1 = Not normal, stop checking for status...
        //  0 = Completed (Ready)
        //  1 = Still In process
        //  2 = Error
        if (codeReturn == -1) {
            timerCheckUploadStatus->stop();
            ui->widget_bottomMenu->removeGeneralMessage();
        }
        else if (codeReturn == 0) {
            timerCheckUploadStatus->stop();
            ui->widget_bottomMenu->setGeneralMessage(msgReady, 5000);
        }
        else if (codeReturn == 2) {
            timerCheckUploadStatus->stop();
            ui->widget_bottomMenu->setGeneralMessage(msgError, 5000);

        }

    }
    else {
        timerCheckUploadStatus->stop();
        LOG_WARN("MainWindow",
                 QStringLiteral("Strava upload status check failed: ")
                 + replyStravaUploadStatus->errorString());
        ui->widget_bottomMenu->setGeneralMessage("Strava : " + replyStravaUploadStatus->errorString(), 5000);
    }
    replyStravaUploadStatus->deleteLater();
}









///////////////////////////////////////////////////////////////
void MainWindow::setPmForCadence(bool usedFor) {

    qDebug() << "setPmForCadence" << usedFor;
    account->use_pm_for_cadence = usedFor;
}
///////////////////////////////////////////////////////////////
void MainWindow::setPmForSpeed(bool usedFor) {

    qDebug() << "setPmForSpeed" << usedFor;
    account->use_pm_for_speed = usedFor;
}
///////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// ─────────────────────────────────────────────────────────────────────────────
// Screenshot mode
//
// Invoked via `--screenshots [dir]` CLI flag.  MainWindow is shown without a
// login dialog and a timer-driven state machine navigates through the key UI
// views, capturing one PNG per view.  The app quits automatically when done.
// ─────────────────────────────────────────────────────────────────────────────

// Delays (ms) applied AFTER completing each step, before starting the next.
// Indexed by the step index that just finished: ssDelays[step] → next step.
static const int ssDelays[] = {
    800,   // after step  0 (captured main window)      → switch to WorkoutCreator
    1000,  // after step  1 (loaded WorkoutCreator)     → capture WorkoutCreator
    500,   // after step  2 (captured WorkoutCreator)   → launch WorkoutDialog
    9000,  // after step  3 (launched WorkoutDialog)    → capture running workout
    200,   // after step  4 (captured workout)          → close WorkoutDialog
    600,   // after step  5 (closed WorkoutDialog)      → enable studio mode
    2000,  // after step  6 (enabled studio mode)       → capture Studio
    500,   // after step  7 (captured Studio)           → switch to Intervals.icu
    1500,  // after step  8 (switched to Intervals.icu) → capture Intervals.icu
    500,   // after step  9 (captured Intervals.icu)    → switch to Plan
    1500,  // after step 10 (switched to Plan)          → capture Plan
    500,   // after step 11 (captured Plan)             → switch to History
    1500,  // after step 12 (switched to History)       → capture History
};

Workout MainWindow::makeDemoWorkout() const
{
    // Build a representative structured threshold workout for screenshots.
    Interval i1;
    i1.setTime(QTime(0, 10, 0));
    i1.setDisplayMsg(tr("Warm-up Build"));
    i1.setPowerStepType(Interval::PROGRESSIVE);
    i1.setTargetFTP_start(0.60); i1.setTargetFTP_end(0.75); i1.setTargetFTP_range(10);
    i1.setRightPowerTarget(-1.0);
    i1.setCadenceStepType(Interval::FLAT);
    i1.setTargetCadence_start(85); i1.setTargetCadence_end(90); i1.setTargetCadence_range(5);

    Interval i2;
    i2.setTime(QTime(0, 8, 0));
    i2.setDisplayMsg(tr("Threshold Interval"));
    i2.setPowerStepType(Interval::FLAT);
    i2.setTargetFTP_start(0.95); i2.setTargetFTP_end(0.95); i2.setTargetFTP_range(5);
    i2.setRightPowerTarget(-1.0);
    i2.setCadenceStepType(Interval::FLAT);
    i2.setTargetCadence_start(90); i2.setTargetCadence_end(90); i2.setTargetCadence_range(5);

    Interval i3;
    i3.setTime(QTime(0, 4, 0));
    i3.setDisplayMsg(tr("Recovery"));
    i3.setPowerStepType(Interval::FLAT);
    i3.setTargetFTP_start(0.50); i3.setTargetFTP_end(0.50); i3.setTargetFTP_range(10);
    i3.setRightPowerTarget(-1.0);
    i3.setCadenceStepType(Interval::FLAT);
    i3.setTargetCadence_start(80); i3.setTargetCadence_end(80); i3.setTargetCadence_range(10);

    Interval i4;
    i4.setTime(QTime(0, 8, 0));
    i4.setDisplayMsg(tr("Threshold Interval"));
    i4.setPowerStepType(Interval::FLAT);
    i4.setTargetFTP_start(0.95); i4.setTargetFTP_end(0.95); i4.setTargetFTP_range(5);
    i4.setRightPowerTarget(-1.0);
    i4.setCadenceStepType(Interval::FLAT);
    i4.setTargetCadence_start(90); i4.setTargetCadence_end(90); i4.setTargetCadence_range(5);

    Interval i5;
    i5.setTime(QTime(0, 5, 0));
    i5.setDisplayMsg(tr("Cool Down"));
    i5.setPowerStepType(Interval::PROGRESSIVE);
    i5.setTargetFTP_start(0.65); i5.setTargetFTP_end(0.50); i5.setTargetFTP_range(10);
    i5.setRightPowerTarget(-1.0);
    i5.setCadenceStepType(Interval::FLAT);
    i5.setTargetCadence_start(85); i5.setTargetCadence_end(85); i5.setTargetCadence_range(10);

    QList<Interval> intervals;
    intervals << i1 << i2 << i3 << i4 << i5;

    return Workout("screenshot_demo.workout", Workout::USER_MADE, intervals,
                   tr("My Threshold Workout"), "MaximumTrainer",
                   tr("A structured threshold workout"), "Base", Workout::T_THRESHOLD);
}

void MainWindow::startScreenshotMode(const QString &outputDir)
{
    qDebug() << "Screenshot mode: output dir =" << outputDir;
    QDir().mkpath(outputDir);
    m_ssOutputDir = outputDir;
    m_ssStep      = 0;

    // Force offline so WorkoutDialog skips the online session check.
    // There is no server to reach in screenshot/CI mode, and the blocking
    // "could not retrieve session" message-box that appears after 3 failed
    // putAccount retries would otherwise cause the child process to hang.
    // Note: MainWindow already obtains the Account singleton via
    // qApp->property("Account") in many other methods; this is consistent
    // with that established pattern for an inherently offline code-path.
    if (auto *acct = qApp->property("Account").value<Account*>())
        acct->isOffline = true;

    // Navigate ALL WebEngine views (including nested ones in child widgets) to
    // blank HTML.  External pages depend on jQuery / RxJS loaded from CDN; in
    // a CI environment without internet access the CDN requests fail, producing
    // console errors and, in some configurations (macOS, Windows), crashes.
    // Using findChildren covers webView_workouts (Main_WorkoutPage) and
    // webView_createWorkout (WorkoutCreator) in addition to the direct children
    // of MainWindow's UI — all would otherwise try to run jQuery code against
    // a page that never loaded jQuery.
    static const QString kBlankHtml =
        QStringLiteral("<html><body></body></html>");
    for (auto *wv : findChildren<QWebEngineView*>())
        wv->setHtml(kBlankHtml);

    resize(1280, 720);
    move(100, 50);
    QCoreApplication::processEvents();

    // Kick off the first step after the window has had time to fully paint.
    QTimer::singleShot(2000, this, SLOT(screenshotNextStep()));
}

void MainWindow::screenshotNextStep()
{
    const int step = m_ssStep++;

    switch (step) {

    // ── Step 0: main window – workout list ────────────────────────────────
    case 0:
        ftb->setCurrentIndex(0);
        ui->tabWidget_workout->setCurrentIndex(0);
        QCoreApplication::processEvents();
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_main_window.png"), "PNG");
        qDebug() << "Screenshot: main_window";
        // Capture the Preferences dialog as the "settings" screenshot. The
        // former server-hosted Settings tab was removed; the native Preferences
        // dialog (DialogMainWindowConfig) is now the settings surface.
        if (dconfig) {
            dconfig->show();
            dconfig->raise();
            QCoreApplication::processEvents();
            dconfig->grab().save(m_ssOutputDir + QLatin1String("/screenshot_settings.png"), "PNG");
            dconfig->hide();
            qDebug() << "Screenshot: settings (Preferences dialog)";
        }
        // Capture the Bluetooth Sensors page (FancyTabBar index 4), which hosts
        // sensor pairing plus the trainer/sensor settings.
        ftb->setCurrentIndex(4);
        QCoreApplication::processEvents();
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_sensors.png"), "PNG");
        qDebug() << "Screenshot: sensors";
        ftb->setCurrentIndex(0);
        QCoreApplication::processEvents();
        break;

    // ── Step 1: switch to WorkoutCreator and load demo workout ───────────
    case 1:
        ftb->setCurrentIndex(0);
        ui->tabWidget_workout->setCurrentIndex(1);
        ui->tab_create->editWorkout(makeDemoWorkout());
        QCoreApplication::processEvents();
        break;

    // ── Step 2: capture WorkoutCreator ───────────────────────────────────
    case 2:
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_workout_editor.png"), "PNG");
        qDebug() << "Screenshot: workout_editor";
        break;

    // ── Step 3: launch WorkoutDialog with SimulatorHub (non-modal) ───────
    case 3: {
        ftb->setCurrentIndex(0);
        ui->tabWidget_workout->setCurrentIndex(0);

        m_ssSimHub = new SimulatorHub(this);
        m_ssSimHub->setUserID(1); // userID must be 1-based; default 0 causes arrUserStudioWidget[-1] OOB crash
        m_ssWorkoutDlg = new WorkoutDialog(makeDemoWorkout(), lstRadio, vecUserStudio, this);

        connect(m_ssSimHub, SIGNAL(signal_hr(int,int)),
                m_ssWorkoutDlg, SLOT(HrDataReceived(int,int)));
        connect(m_ssSimHub, SIGNAL(signal_cadence(int,int)),
                m_ssWorkoutDlg, SLOT(CadenceDataReceived(int,int)));
        connect(m_ssSimHub, SIGNAL(signal_speed(int,double)),
                m_ssWorkoutDlg, SLOT(TrainerSpeedDataReceived(int,double)));
        connect(m_ssSimHub, SIGNAL(signal_power(int,int)),
                m_ssWorkoutDlg, SLOT(PowerDataReceived(int,int)));
        connect(m_ssSimHub, SIGNAL(signal_oxygen(int,double,double)),
                m_ssWorkoutDlg, SLOT(OxygenValueChanged(int,double,double)));

        m_ssSimHub->start();
        m_ssWorkoutDlg->show();
        m_ssWorkoutDlg->raise();
        m_ssWorkoutDlg->activateWindow();
        QCoreApplication::processEvents();
        break;
    }

    // ── Step 4: capture workout running ──────────────────────────────────
    case 4:
        if (m_ssWorkoutDlg) {
            m_ssWorkoutDlg->raise();
            QCoreApplication::processEvents();
            m_ssWorkoutDlg->grab().save(
                m_ssOutputDir + QLatin1String("/screenshot_workout_running.png"), "PNG");
            qDebug() << "Screenshot: workout_running";
        }
        break;

    // ── Step 5: close WorkoutDialog ───────────────────────────────────────
    case 5:
        if (m_ssWorkoutDlg) {
            // Use hide() rather than close() to avoid triggering reject() →
            // sureYouWantToQuit() → start_or_pause_workout(), which would
            // show a blocking "save progress?" message-box in screenshot mode.
            m_ssWorkoutDlg->hide();
            delete m_ssWorkoutDlg;
            m_ssWorkoutDlg = nullptr;
        }
        if (m_ssSimHub) {
            m_ssSimHub->stopDecodingMsg();
            delete m_ssSimHub;
            m_ssSimHub = nullptr;
        }
        QCoreApplication::processEvents();
        break;

    // ── Step 6: enable studio mode and switch to Studio tab (tab 3) ──────
    case 6:
        setNumberUserStudio(6);
        enableStudioMode(true);
        ftb->setCurrentIndex(3);
        raise();
        activateWindow();
        QCoreApplication::processEvents();
        break;

    // ── Step 7: capture studio mode ───────────────────────────────────────
    case 7:
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_studio_mode.png"), "PNG");
        qDebug() << "Screenshot: studio_mode";
        enableStudioMode(false);
        break;

    // ── Step 8: switch to Intervals.icu tab (tab 1) ─────────────────────
    case 8:
        ftb->setCurrentIndex(1);
        raise();
        activateWindow();
        QCoreApplication::processEvents();
        break;

    // ── Step 9: capture Intervals.icu ────────────────────────────────────
    case 9:
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_activity_history.png"), "PNG");
        qDebug() << "Screenshot: activity_history (Intervals.icu tab)";
        break;

    // ── Step 10: switch to Plan tab (tab 2) ───────────────────────────────
    case 10:
        ftb->setCurrentIndex(2);
        ui->tabWidget->setCurrentIndex(0);   // Plan sub-tab
        raise();
        activateWindow();
        QCoreApplication::processEvents();
        break;

    // ── Step 11: capture Plan ─────────────────────────────────────────────
    case 11:
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_plan.png"), "PNG");
        qDebug() << "Screenshot: plan";
        break;

    // ── Step 12: switch to History tab (tab 5) ────────────────────────────
    case 12:
        ftb->setCurrentIndex(5);
        raise();
        activateWindow();
        QCoreApplication::processEvents();
        break;

    // ── Step 13: capture History ─────────────────────────────────────────
    case 13:
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_history.png"), "PNG");
        qDebug() << "Screenshot: history";
        QTimer::singleShot(300, qApp, SLOT(quit()));
        return; // No further steps — quit is already scheduled.

    default:
        QTimer::singleShot(300, qApp, SLOT(quit()));
        return;
    }

    // Schedule the next step after its delay.
    const int numDelays = static_cast<int>(sizeof(ssDelays) / sizeof(ssDelays[0]));
    if (step < numDelays)
        QTimer::singleShot(ssDelays[step], this, SLOT(screenshotNextStep()));
    else
        QTimer::singleShot(300, qApp, SLOT(quit()));
}