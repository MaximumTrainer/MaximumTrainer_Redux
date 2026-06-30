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
#include <QFile>
#include <QTextStream>
#include <QGuiApplication>
#include <QStyleHints>
#include <QScopeGuard>

#include "util.h"
#include "logger.h"
#include "environnement.h"
#include "soundplayer.h"
#include "dialogmainwindowconfig.h"
#include "workoutdialog.h"
#include "workout.h"
#include "mycreatorplot.h"
#include "importerworkout.h"
#include "importerworkoutzwo.h"
#include "intervalsicudao.h"
#include "xmlutil.h"
#include "managerachievement.h"
#include "simulator_hub.h"
#include "dialog_connection_method.h"
#include "networkmonitor.h"
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
#include <QtWebEngineCore/QWebEngineDownloadRequest>
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

#ifdef GC_WASM_BUILD
#include <emscripten.h>
#include <QPointer>

static QPointer<MainWindow> g_mainWindow;

extern "C" {

// Drive the running WASM app into the metric-dashboard view by launching the
// simulator-fed demo workout. Exposed to JS as window.mt_startDemoWorkout()
// so the Playwright suite can assert the QML dashboard actually renders into
// the Qt canvas in-browser.
EMSCRIPTEN_KEEPALIVE
void mt_start_demo_workout_impl()
{
    if (!g_mainWindow) return;
    // No server session exists for a demo workout; force offline so the dialog
    // never blocks on an online session check (mirrors screenshot mode).
    if (auto *acct = qApp->property("Account").value<Account*>())
        acct->isOffline = true;
    QMetaObject::invokeMethod(g_mainWindow.data(), "launchDemoWorkout",
                              Qt::QueuedConnection);
}

// Switch the demo workout's content pane to the retro ghost-race (the "Game"
// option). Exposed as window.mt_showRace() so Playwright can assert RetroRace.qml
// actually composites in-browser (catches a missing QtQuick.Shapes module).
EMSCRIPTEN_KEEPALIVE
void mt_show_demo_race_impl()
{
    if (!g_mainWindow) return;
    QMetaObject::invokeMethod(g_mainWindow.data(), "showDemoRaceView",
                              Qt::QueuedConnection);
}

} // extern "C"

EM_JS(void, js_exposeWasmDemoWorkout, (), {
    window.mt_startDemoWorkout = function() {
        Module._mt_start_demo_workout_impl();
    };
    window.mt_showRace = function() {
        Module._mt_show_demo_race_impl();
    };
});
#endif // GC_WASM_BUILD





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

    planObject = new PlanObject(this);         ///Used with QWebView Plan page

    replyIntervalsIcuZwo    = nullptr;


    createWebChannelPlan();

    // Right-click context menu on the Plan (Intervals.icu calendar) view
    ui->webView_plan->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->webView_plan, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(showPlanContextMenu(QPoint)));

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



    ftb = new FancyTabBar(FancyTabBar::TabBarPosition::Left, ui->widget_fancyMenu);
    ftb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Tab indices must stay in sync with the pages in stackedWidget_menu
    // (see leftMenuChanged): 0 Workout, 1 Intervals.icu, 2 Studio, 3 Devices,
    // 4 History. The former Profile and Settings web-view tabs were removed —
    // FTP/LTHR/weight now live in the Preferences dialog.
    //
    // The "Plan" tab is intentionally HIDDEN (not inserted) pending a clear spec
    // — see issue #299: its Intervals.icu-calendar view needs a separate web
    // login (the app's OAuth/API auth doesn't carry into the embedded browser),
    // and the Trainerweb (TrainerDB) view's downloads are broken, so it only
    // confused users. The page still lives in stackedWidget_menu (index 2); to
    // bring it back, re-insert the tab below and restore tabToPage in
    // leftMenuChanged().
    ftb->insertTab(0, QIcon(":/image/icon/workoutMan"), tr("Workout"));
    ftb->insertTab(1, QIcon(":/image/icon/intervals"),   tr("Intervals.icu"));
    ftb->insertTab(2, QIcon(":/image/icon/studio"), tr("Studio"));
    ftb->insertTab(3, QIcon(":/image/icon/bluetooth"), tr("Devices"));
    ftb->insertTab(4, QIcon(":/image/icon/chart"), tr("History"));

    ftb->setTabEnabled(0, true);
    ftb->setTabEnabled(1, true);
    ftb->setTabEnabled(2, true);
    ftb->setTabEnabled(3, true);
    ftb->setTabEnabled(4, true);



    ftb->setCurrentIndex(0);
    connect(ftb, SIGNAL(currentChanged(int)), this, SLOT(leftMenuChanged(int)) );


    ui->tabWidget_workout->tabBar()->setObjectName("tabBarWorkout");
    setStyleSheet(qApp->styleSheet());

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Track OS colour-scheme changes for System theme mode.
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            &MainWindow::slotSystemThemeChanged);
#endif


    // Studio rider config (name/FTP/LTHR) lives in QSettings alongside the
    // per-rider sensors and ERG settings (UserStudio::loadStudioConfig).
    vecUserStudio = UserStudio::loadStudioConfig();


    ManagerAchievement *achievementManager = new ManagerAchievement(this);
    qApp->setProperty("ManagerAchievement", QVariant::fromValue<ManagerAchievement*>(achievementManager));



    //Parse Workouts
    ui->tab_workout1->parseIncludedWorkouts();
    ui->tab_workout1->parseMapWorkout(account->FTP);
    ui->tab_workout1->parseUserWorkouts();


    currentIndexLeftMenu = 0;


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


    /// Update create workout graph on FTP and LTHR change
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

    // Native Studio page: enabling studio mode / changing rider count routes
    // through the same MainWindow logic the old web page used.
    connect(ui->studioWidget, &StudioWidget::studioModeChanged, this, &MainWindow::enableStudioMode);
    connect(ui->studioWidget, &StudioWidget::riderCountChanged, this, &MainWindow::setNumberUserStudio);
    connect(ui->studioWidget, &StudioWidget::ridersChanged, this, &MainWindow::updateVecStudio);

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
    connect(m_queuePanel, &QueuePanelWidget::startQueueRequested,
            this, &MainWindow::startWorkoutQueue);

#ifdef GC_WASM_BUILD
    // Expose the demo-workout test hook so Playwright can drive the app into
    // the metric-dashboard view and verify QML rendering in the browser.
    g_mainWindow = this;
    js_exposeWasmDemoWorkout();
#endif
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////


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
    // Filter on the SAVED filename stem, not the raw name: the workout list
    // derives a workout's name from its filename, so a colon/slash/etc. removed
    // for the filename (e.g. "A:B" → "A_B") must be mirrored in the search term
    // or the just-imported workout can't be found (#294).
    ui->tab_workout1->setFilterWorkoutName(uniqueSafeName);
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
                // Filter on the saved filename stem so a sanitised name (e.g.
                // colon → "_") still matches the listed workout (#294).
                ui->tab_workout1->setFilterWorkoutName(uniqueName);
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
// Launch the queue from item #1 (triggered by the Start button). Dequeues the
// first workout itself and runs it directly; the normal finished()→advance chain
// then handles the rest. Dequeuing here is what prevents the double-run the issue
// describes (where double-clicking a list row also re-ran it as queue item #1).
void MainWindow::startWorkoutQueue()
{
    if (m_launchingWorkout || isInsideWorkout)
        return;

    // Take the next non-blank item atomically (see tryAdvanceWorkoutQueue).
    QString name;
    QString path;
    while (!m_workoutQueue->isEmpty()) {
        name = m_workoutQueue->name(0);
        path = m_workoutQueue->dequeueFilePath();
        if (!path.isEmpty())
            break;
    }
    if (path.isEmpty())
        return;

    XmlUtil xml;
    Workout first = xml.parseSingleWorkoutXml(path);
    if (!first.getName().isEmpty()) {
        executeWorkout(first);
    } else {
        QMessageBox::warning(this,
                             tr("Queue Error"),
                             tr("Could not load the queued workout:\n%1\n(%2)").arg(name, path));
        tryAdvanceWorkoutQueue();
    }
}

void MainWindow::tryAdvanceWorkoutQueue()
{
    // Take the next item NOW (atomically), skipping any blank entries. The
    // countdown below spins a nested event loop, so if we only read here and
    // deferred the dequeue, a re-entrant advance could read the same item again
    // or observe a half-updated queue — that produced the empty-path "Could not
    // load the next queued workout" error.
    QString nextName;
    QString nextPath;
    while (!m_workoutQueue->isEmpty()) {
        nextName = m_workoutQueue->name(0);
        nextPath = m_workoutQueue->dequeueFilePath();
        if (!nextPath.isEmpty())
            break;
    }
    if (nextPath.isEmpty())
        return;

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
    QTimer::singleShot(0, this, [this, nextName, nextPath]() {
        XmlUtil xmlNext;
        Workout next = xmlNext.parseSingleWorkoutXml(nextPath);
        if (!next.getName().isEmpty()) {
            executeWorkout(next);
        } else {
            QMessageBox::warning(this,
                                 tr("Queue Error"),
                                 tr("Could not load the next queued workout:\n%1\n(%2)")
                                     .arg(nextName, nextPath));
            // The bad entry is already removed; keep the queue moving.
            tryAdvanceWorkoutQueue();
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



    // Visible tabs don't map 1:1 to stacked-page indices: the Profile/Settings
    // pages were removed, and the "Plan" page (stacked index 2) is kept but its
    // tab is hidden (see the insertTab block / issue #299). Visible tab order is
    // 0 Workout, 1 Intervals.icu, 2 Studio, 3 Devices, 4 History; the matching
    // stacked pages are Workout 0, Intervals 1, Studio 3, Sensors 5, History 4.
    static const int tabToPage[] = {0, 1, 3, 5, 4};
    const int pageIndex = (tabSelected >= 0 && tabSelected < 5)
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


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::updateVecStudio(QVector<UserStudio> vecUserStudio) {

    this->vecUserStudio = vecUserStudio;

    // Persist immediately (e.g. after a Studio FTP test updates every rider's
    // FTP/LTHR) so the Studio tab reflects the new values on its next open and
    // they survive a restart, rather than only being written on app close.
    UserStudio::saveStudioConfig(this->vecUserStudio);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
QVector<UserStudio> MainWindow::prepareStudioRiders(bool simulator) {

    QVector<UserStudio> riders = vecUserStudio;   // copy; never persisted from here
    const int nbRiders = qBound(1, account->nb_user_studio, constants::nbMaxUserStudio);

    for (int i = 0; i < nbRiders && i < riders.size(); ++i) {
        UserStudio u = riders.at(i);

        // Fall back to the account's values when the rider left FTP/LTHR unset,
        // so per-rider targets are sane.
        if (u.getFTP()  <= 0) u.setFTP(account->FTP);
        if (u.getLTHR() <= 0) u.setLTHR(account->LTHR);

        // createUserStudioWidget() hides the HR/Power sections unless the
        // matching flag is set. The simulator drives every metric, so show all;
        // otherwise reflect what this rider actually has saved.
        if (simulator) {
            u.setHrID(1);
            u.setPowerID(1);
            u.setCadenceID(1);
            u.setSpeedID(1);
            u.setFecID(0);
        } else {
#ifndef GC_WASM_BUILD
            const QMap<BtleSensorRole, BtleSavedSensor> saved = BtleSensorStore::loadAll(i + 1);
            const bool hasTrainer = saved.contains(BtleSensorRole::Trainer);
            const bool ergOn = StudioWidget::studioErgControl(i + 1, account->control_trainer_resistance);
            u.setHrID(saved.contains(BtleSensorRole::HeartRate) ? 1 : 0);
            u.setPowerID((saved.contains(BtleSensorRole::Power) || hasTrainer) ? 1 : 0);
            u.setCadenceID((saved.contains(BtleSensorRole::CadenceSpeed) || hasTrainer) ? 1 : 0);
            u.setSpeedID((saved.contains(BtleSensorRole::CadenceSpeed) || hasTrainer) ? 1 : 0);
            // WorkoutDialog::sendLoads() emits setLoad(getFecID(), …) per rider;
            // use the 1-based rider id so each trainer hub (setUserID) filters to
            // its own target. 0 disables ERG for this rider (no trainer, or the
            // rider turned trainer control off).
            u.setFecID((hasTrainer && ergOn) ? (i + 1) : 0);
#endif
        }

        riders.replace(i, u);
    }

    return riders;
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

    // Persist so the toggle state survives a restart (saveDisplayPrefs writes
    // enable_studio_mode + nb_user_studio together).
    account->saveDisplayPrefs();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::setNumberUserStudio(int nbUser) {

    qDebug() << "setNumberUserStudio" << nbUser;
    account->nb_user_studio = nbUser;

    // Persist so the selected rider count survives a restart.
    account->saveDisplayPrefs();
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


    //Save Studio rider config (name/FTP/LTHR) to QSettings
    UserStudio::saveStudioConfig(vecUserStudio);

    //Save filter Field
    ui->tab_workout1->saveFilterFields();

    Q_UNUSED(event);
    saveSettings();
    this->setVisible(false);

    // Save Settings and Account List workout done to xml file. Everything is
    // local and synchronous now — the old maximumtrainer.com server PUT
    // (backend removed in #245) used to retry here for ~6 minutes behind a
    // blocking "Saving your data…" window before giving up.
    XmlUtil::saveLocalSaveFile(account);

    qDebug () << "closeEvent Done mainWindow";

    // On logout, hand back to the login screen in-process rather than quitting;
    // main.cpp tears this window down and re-shows the login dialog.
    if (m_loggingOut) {
        emit logoutRequested();
        return;
    }

    releaseWebEngineViews();

    // quitOnLastWindowClosed is disabled (see main.cpp), so quit explicitly
    // once the main window has finished closing.
    qApp->quit();
}

void MainWindow::releaseWebEngineViews()
{
    // This MainWindow is heap-allocated in main.cpp and never deleted on quit,
    // so its QWebEngineView children (and their pages) would otherwise outlive
    // the default QWebEngineProfile, which Qt releases at process exit:
    // "Release of profile requested but WebEnginePage still not deleted."
    // Delete the views now — synchronously, before the event loop ends — so
    // every page is gone before the profile is. Recurses into nested views
    // (e.g. the workout/creator web pages inside child widgets). Safe to call
    // more than once: a second pass simply finds no views.
    //
    // WASM has no real QtWebEngine (QWebEngineView is a stub without Q_OBJECT),
    // so there is no profile to release and findChildren<QWebEngineView*> would
    // not even compile there.
#ifndef GC_WASM_BUILD
    for (auto *webView : findChildren<QWebEngineView*>())
        delete webView;
#endif
}

void MainWindow::scheduleScreenshotQuit()
{
    releaseWebEngineViews();
    QTimer::singleShot(300, qApp, SLOT(quit()));
}

//QMENUBAR
/////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::on_actionExit_triggered()
{
    this->close();
}

/////////////////////////////////////////////////////////////////////////////////////////
void MainWindow::on_actionLogout_triggered()
{
    if (QMessageBox::question(
            this, tr("Log Out"),
            tr("Log out of Intervals.icu and return to the login screen?"))
        != QMessageBox::Yes)
        return;

    if (account)
        account->logout();

    // close() runs the normal save path; closeEvent then re-shows the login
    // screen in-process (instead of quitting) because we're logging out.
    m_loggingOut = true;
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
    // "Folder → Open History Folder": open the history folder in the file
    // manager (it previously just switched to the History tab by mistake).
    Util::openHistoryFolder();
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

    // Re-entrancy guard: the launch flow below runs nested event loops (the
    // connection-method / sensor dialogs and the BLE connect wait), during which
    // a queued double-click on the workout list could otherwise re-enter here and
    // spawn a second (or third) WorkoutDialog. Block until this launch resolves;
    // once the workout is actually running isInsideWorkout keeps it blocked.
    if (m_launchingWorkout || isInsideWorkout)
        return;
    m_launchingWorkout = true;
    auto launchGuard = qScopeGuard([this]() { m_launchingWorkout = false; });

    DialogConnectionMethod connDlg(this);
    if (connDlg.exec() != QDialog::Accepted)
        return;


    // ── Simulation path ───────────────────────────────────────────────────
    if (connDlg.selectedMethod() == DialogConnectionMethod::Simulation) {
        // In Studio mode simulate every selected rider; otherwise a single rider.
        const bool studio = account->enable_studio_mode;
        const int nbRiders = studio ? qBound(1, account->nb_user_studio, constants::nbMaxUserStudio) : 1;
        const QVector<UserStudio> riders = studio ? prepareStudioRiders(true) : vecUserStudio;

        // Show WorkoutDialog NON-MODALLY (window-modal via setWindowModality,
        // run on the main event loop instead of QDialog::exec()). The embedded
        // QWebEngine video player reparents widgets when it initialises, which
        // destroys and recreates the dialog's native window; inside a nested
        // exec() loop that window-destroy calls QEventLoop::exit() and tears the
        // dialog down. Running on the main loop removes that hazard. Post-workout
        // cleanup moves to the finished() handler below.
        WorkoutDialog *w = new WorkoutDialog(workout, lstRadio, riders);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->setWindowModality(Qt::ApplicationModal);

        // One simulator hub per rider, each emitting its own 1-based userID so
        // WorkoutDialog routes the data to the matching rider box.
        QList<SimulatorHub*> simHubs;
        for (int i = 0; i < nbRiders; ++i) {
            SimulatorHub *simHub = new SimulatorHub(this);
            simHub->setUserID(i + 1);

            connect(simHub, SIGNAL(signal_hr(int,int)),               w, SLOT(HrDataReceived(int,int)));
            connect(simHub, SIGNAL(signal_cadence(int,int)),          w, SLOT(CadenceDataReceived(int,int)));
            connect(simHub, SIGNAL(signal_speed(int,double)),         w, SLOT(TrainerSpeedDataReceived(int,double)));
            connect(simHub, SIGNAL(signal_power(int,int)),            w, SLOT(PowerDataReceived(int,int)));
            connect(simHub, SIGNAL(signal_balance(int,int)),          w, SLOT(PowerBalanceDataReceived(int,int)));
            connect(simHub, SIGNAL(signal_pedal(int,double,double,double,double,double)), w, SLOT(pedalMetricReceived(int,double,double,double,double,double)));
            connect(simHub, SIGNAL(signal_oxygen(int,double,double)), w, SLOT(OxygenValueChanged(int,double,double)));

            connect(w, SIGNAL(setLoad(int,double)),  simHub, SLOT(setLoad(int,double)));
            connect(w, SIGNAL(setSlope(int,double)), simHub, SLOT(setSlope(int,double)));
            connect(w, SIGNAL(stopDecodingMsgHub()), simHub, SLOT(stopDecodingMsg()));

            simHubs.append(simHub);
        }
        w->enableTrainerControl();

        connect(w, SIGNAL(fitFileReady(QString, QString, QString)), this, SLOT(checkToUploadFile(QString,QString,QString)));
        connect(w, SIGNAL(ftp_lthr_changed()), ui->tab_workout1, SLOT(updateTableViewMetrics()));
        connect(w, SIGNAL(ftpUserStudioChanged(QVector<UserStudio>)), this, SLOT(updateVecStudio(QVector<UserStudio>)));

        connect(w, &QDialog::finished, this, [this, simHubs]() {
            workoutOver();
            for (SimulatorHub *simHub : simHubs) {
                simHub->stopDecodingMsg();
                delete simHub;
            }
            // Auto-advance to next queued workout if one exists
            tryAdvanceWorkoutQueue();
        });

        for (SimulatorHub *simHub : simHubs)
            simHub->start();
        workoutExecuting();
        QApplication::restoreOverrideCursor();
        w->show();
        return;
    }

    // ── BTLE Device path ─────────────────────────────────────────────────
#ifndef GC_WASM_BUILD
    // Studio mode: connect each rider's own saved sensor package in turn, tag
    // every hub with that rider's id, then run the workout with all of them.
    if (account->enable_studio_mode) {
        const int nbRiders = qBound(1, account->nb_user_studio, constants::nbMaxUserStudio);
        QList<QPair<int, QMap<BtleSensorRole, BtleHub*>>> riderHubs;

        for (int rider = 1; rider <= nbRiders; ++rider) {
            const QMap<BtleSensorRole, BtleSavedSensor> saved = BtleSensorStore::loadAll(rider);
            if (saved.isEmpty())
                continue;   // rider configured no sensors — their box stays empty

            SensorConnectDialog connectDlg(saved, account->wheel_circ, this);
            const QString riderName = (rider - 1 < vecUserStudio.size()
                                       && !vecUserStudio.at(rider - 1).getDisplayName().trimmed().isEmpty())
                                          ? vecUserStudio.at(rider - 1).getDisplayName().trimmed()
                                          : tr("Rider%1").arg(rider);
            connectDlg.setWindowTitle(tr("Connect sensors — %1").arg(riderName));

            if (connectDlg.exec() != QDialog::Accepted) {
                // Cancelled: tear down everything connected for earlier riders.
                for (const QPair<int, QMap<BtleSensorRole, BtleHub*>> &done : riderHubs) {
                    QSet<BtleHub*> uniq;
                    for (BtleHub *hub : done.second) uniq.insert(hub);
                    for (BtleHub *hub : uniq) { hub->disconnectFromDevice(); delete hub; }
                }
                return;
            }

            QMap<BtleSensorRole, BtleHub*> hubsByRole = connectDlg.connectedHubs();
            if (!hubsByRole.isEmpty()) {
                connectDlg.detachHubs();   // we now own the hubs
                QSet<BtleHub*> uniq;
                for (BtleHub *hub : hubsByRole) uniq.insert(hub);
                for (BtleHub *hub : uniq) hub->setUserID(rider);
                riderHubs.append({rider, hubsByRole});
            }
        }

        startWorkoutWithStudioHubs(workout, riderHubs);
        return;
    }

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
                        ftb->setCurrentIndex(3);
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
    connect(btleHub, SIGNAL(signal_balance(int,int)),          w, SLOT(PowerBalanceDataReceived(int,int)));
    connect(btleHub, SIGNAL(signal_pedal(int,double,double,double,double,double)), w, SLOT(pedalMetricReceived(int,double,double,double,double,double)));
    connect(btleHub, SIGNAL(signal_oxygen(int,double,double)), w, SLOT(OxygenValueChanged(int,double,double)));
    connect(btleHub, &BtleHub::signal_battery, w, &WorkoutDialog::batteryStatusReceived);

    // Surface BLE disconnections mid-workout so WorkoutDialog can pause and
    // the user sees the DOM reconnect overlay (WASM) or a status message.
    connect(btleHub, &BtleHub::connectionError, w, &WorkoutDialog::onBleConnectionError);

    connect(w, SIGNAL(setLoad(int,double)),  btleHub, SLOT(setLoad(int,double)));
    connect(w, SIGNAL(setSlope(int,double)), btleHub, SLOT(setSlope(int,double)));
    connect(w, SIGNAL(setResistance(int,int)), btleHub, SLOT(setResistanceLevel(int,int)));
    connect(w, SIGNAL(stopDecodingMsgHub()), btleHub, SLOT(stopDecodingMsg()));
    // Virtual shifting prefers FTMS resistance level (0x04) for an instant,
    // gear-like feel; tell the dialog whether this trainer supports it (the
    // feature read may already have completed, so seed it now and also listen).
    connect(btleHub, &BtleHub::signal_resistanceLevelSupported,
            w, &WorkoutDialog::setResistanceLevelSupported);
    w->setResistanceLevelSupported(btleHub->resistanceLevelSupported());
    // Harmless when the device is not a trainer: BtleHub::setLoad() no-ops
    // without an FTMS service.
    w->enableTrainerControl();

#ifdef Q_OS_WASM
    // On WASM, BtleHub is aliased to BtleHubWasm which exposes scanForDevice().
    // The DOM overlay's Reconnect button routes through bleReconnectRequestC →
    // BtleHubWasm::scanForDevice(), but WorkoutDialog can also emit reconnectDevice()
    // if needed from Qt-side logic.
    connect(w, &WorkoutDialog::reconnectDevice, btleHub, &BtleHub::scanForDevice);
#endif

    connect(w, SIGNAL(fitFileReady(QString, QString, QString)), this, SLOT(checkToUploadFile(QString,QString,QString)));
    connect(w, SIGNAL(ftp_lthr_changed()), ui->tab_workout1, SLOT(updateTableViewMetrics()));
    connect(w, SIGNAL(ftpUserStudioChanged(QVector<UserStudio>)), this, SLOT(updateVecStudio(QVector<UserStudio>)));

    connect(w, &QDialog::finished, this, [this, btleHub]() {
        workoutOver();
        btleHub->disconnectFromDevice();
        delete btleHub;
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

    QSet<BtleHub*> allHubs;
    wireHubsToDialog(w, hubsByRole, allHubs);

    connect(w, SIGNAL(fitFileReady(QString, QString, QString)), this, SLOT(checkToUploadFile(QString,QString,QString)));
    connect(w, SIGNAL(ftp_lthr_changed()), ui->tab_workout1, SLOT(updateTableViewMetrics()));
    connect(w, SIGNAL(ftpUserStudioChanged(QVector<UserStudio>)), this, SLOT(updateVecStudio(QVector<UserStudio>)));

    connect(w, &QDialog::finished, this, [this, allHubs]() {
        workoutOver();
        for (BtleHub *hub : allHubs) {
            hub->disconnectFromDevice();
            delete hub;
        }
        // Auto-advance to next queued workout if one exists
        tryAdvanceWorkoutQueue();
    });

    workoutExecuting();
    QApplication::restoreOverrideCursor();
    w->show();
}

// Wire one rider's connected hubs (keyed by role) to the dialog: shared
// battery/error signals once per distinct hub, then per-role data signals with
// local dedup so a trainer that also reports power/cadence/speed is not
// double-counted. Distinct hubs are added to \a allHubs for lifecycle cleanup.
// Called once for solo and once per rider in Studio mode.
void MainWindow::wireHubsToDialog(WorkoutDialog *w,
                                  const QMap<BtleSensorRole, BtleHub*> &hubsByRole,
                                  QSet<BtleHub*> &allHubs,
                                  int riderIndex)
{
    QSet<BtleHub*> uniqueHubs;
    for (BtleHub *hub : hubsByRole)
        uniqueHubs.insert(hub);

    for (BtleHub *hub : uniqueHubs) {
        allHubs.insert(hub);
        connect(hub, &BtleHub::signal_battery,  w, &WorkoutDialog::batteryStatusReceived);
        connect(hub, &BtleHub::connectionError, w, &WorkoutDialog::onBleConnectionError);
        connect(w, SIGNAL(stopDecodingMsgHub()), hub, SLOT(stopDecodingMsg()));
    }

    BtleHub *powerHub   = nullptr;
    BtleHub *cadenceHub = nullptr;
    BtleHub *speedHub   = nullptr;
    BtleHub *hrHub      = nullptr;

    auto wireHr = [&](BtleHub *hub) {
        if (hrHub == hub) return;
        hrHub = hub;
        connect(hub, SIGNAL(signal_hr(int,int)), w, SLOT(HrDataReceived(int,int)));
    };

    auto wirePower = [&](BtleHub *hub) {
        if (powerHub == hub) return;          // already wired from this device
        powerHub = hub;
        connect(hub, SIGNAL(signal_power(int,int)), w, SLOT(PowerDataReceived(int,int)));
        connect(hub, SIGNAL(signal_balance(int,int)), w, SLOT(PowerBalanceDataReceived(int,int)));
        connect(hub, SIGNAL(signal_pedal(int,double,double,double,double,double)), w, SLOT(pedalMetricReceived(int,double,double,double,double,double)));
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
        // Remember when this trainer bridges an HR strap so the sensors page can
        // mark the Heart Rate slot "provided by trainer" next time it opens.
        connect(hub, &BtleHub::signal_trainerProvidesHr, this,
                [riderIndex]() { BtleSensorStore::setTrainerProvidesHr(true, riderIndex); });
        connect(w, SIGNAL(setLoad(int,double)),  hub, SLOT(setLoad(int,double)));
        connect(w, SIGNAL(setSlope(int,double)), hub, SLOT(setSlope(int,double)));
        connect(w, SIGNAL(setResistance(int,int)), hub, SLOT(setResistanceLevel(int,int)));
        connect(hub, &BtleHub::signal_resistanceLevelSupported,
                w, &WorkoutDialog::setResistanceLevelSupported);
        w->setResistanceLevelSupported(hub->resistanceLevelSupported());
        w->enableTrainerControl();
    }
    if (hubsByRole.contains(BtleSensorRole::Power))
        wirePower(hubsByRole.value(BtleSensorRole::Power));
    if (hubsByRole.contains(BtleSensorRole::CadenceSpeed)) {
        BtleHub *hub = hubsByRole.value(BtleSensorRole::CadenceSpeed);
        wireCadence(hub);
        wireSpeed(hub);
    }
    // A dedicated HR strap is more reliable than a trainer-bridged reading, so it
    // takes precedence; otherwise fall back to HR the trainer carries over FTMS.
    if (hubsByRole.contains(BtleSensorRole::HeartRate))
        wireHr(hubsByRole.value(BtleSensorRole::HeartRate));
    else if (hubsByRole.contains(BtleSensorRole::Trainer))
        wireHr(hubsByRole.value(BtleSensorRole::Trainer));
    if (hubsByRole.contains(BtleSensorRole::Oxygen))
        connect(hubsByRole.value(BtleSensorRole::Oxygen),
                SIGNAL(signal_oxygen(int,double,double)), w, SLOT(OxygenValueChanged(int,double,double)));
}

// Studio variant: each rider already had its saved sensors connected and its
// hubs tagged with setUserID(rider). Wire them all to one dialog.
void MainWindow::startWorkoutWithStudioHubs(const Workout &workout,
        const QList<QPair<int, QMap<BtleSensorRole, BtleHub*>>> &riderHubs)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    WorkoutDialog *w = new WorkoutDialog(workout, lstRadio, prepareStudioRiders(false));
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowModality(Qt::ApplicationModal);

    QSet<BtleHub*> allHubs;
    for (const QPair<int, QMap<BtleSensorRole, BtleHub*>> &rider : riderHubs)
        wireHubsToDialog(w, rider.second, allHubs, rider.first);

    connect(w, SIGNAL(fitFileReady(QString, QString, QString)), this, SLOT(checkToUploadFile(QString,QString,QString)));
    connect(w, SIGNAL(ftp_lthr_changed()), ui->tab_workout1, SLOT(updateTableViewMetrics()));
    connect(w, SIGNAL(ftpUserStudioChanged(QVector<UserStudio>)), this, SLOT(updateVecStudio(QVector<UserStudio>)));

    connect(w, &QDialog::finished, this, [this, allHubs]() {
        workoutOver();
        for (BtleHub *hub : allHubs) {
            hub->disconnectFromDevice();
            delete hub;
        }
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

    Q_UNUSED(description);
    qDebug() << "check to upload Fit file";

    // ── Plan adherence: auto-mark this workout as completed ──────────────────
    if (m_adherenceStore && !nameOnly.isEmpty()) {
        const QDate today = QDate::currentDate();
        m_adherenceStore->addCompleted(today, nameOnly, filename);
    }

    // Strava and Intervals.icu auto-uploads are both handled in WorkoutDialog's
    // post-workout panel (so the status — and the "View on …" link — shows
    // there, and the manual button is replaced), which also avoids uploading
    // the same activity twice.
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
    600,   // after step 13 (captured History)          → launch studio workout
    4000,  // after step 14 (launched studio workout)   → capture studio workout
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

void MainWindow::launchDemoWorkout()
{
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
    connect(m_ssSimHub, SIGNAL(signal_balance(int,int)),
            m_ssWorkoutDlg, SLOT(PowerBalanceDataReceived(int,int)));
    connect(m_ssSimHub, SIGNAL(signal_pedal(int,double,double,double,double,double)),
            m_ssWorkoutDlg, SLOT(pedalMetricReceived(int,double,double,double,double,double)));
    connect(m_ssSimHub, SIGNAL(signal_oxygen(int,double,double)),
            m_ssWorkoutDlg, SLOT(OxygenValueChanged(int,double,double)));

    m_ssSimHub->start();
    m_ssWorkoutDlg->enableTrainerControl();   // demo represents a trainer workout (shows the gear UI)
    m_ssWorkoutDlg->show();
    m_ssWorkoutDlg->raise();
    m_ssWorkoutDlg->activateWindow();
    QCoreApplication::processEvents();
}

// ── BLE sensor-check (headless) ──────────────────────────────────────────────
// Drives a demo workout, then injects each sensor type one at a time and grabs
// a screenshot per type, while capturing the trainer-control output (setLoad /
// setSlope) so ERG can be regression-checked.
void MainWindow::runSensorCheck(const QString &outDir)
{
    QDir().mkpath(outDir);
    m_scOutDir = outDir;
    m_scStep = 0;
    m_scReport.clear();
    m_scLoadLog.clear();

    if (auto *acct = qApp->property("Account").value<Account*>())
        acct->isOffline = true;

    // Make every per-metric widget visible so each injected sensor is observable
    // (oxygen and L/R balance are off by default), and use the plain video pane
    // (not the race) so the metric band is unobstructed.
    account->show_hr_widget            = true;
    account->show_power_widget         = true;
    account->show_cadence_widget       = true;
    account->show_speed_widget         = true;
    account->show_oxygen_widget        = true;
    account->show_power_balance_widget = true;
    account->display_video             = 0;

    resize(1280, 900);
    move(60, 40);

    // Reuse the demo-workout setup (creates m_ssSimHub + m_ssWorkoutDlg, wires
    // the sim's sensor signals to the dialog, and shows it).
    launchDemoWorkout();

    // Stop the simulator feed so each manually-injected sensor value is the only
    // thing on screen when grabbed (otherwise the sim overwrites it next tick).
    if (m_ssSimHub) m_ssSimHub->stop();

    // Capture trainer-control output — proves ERG actually drives the trainer
    // (the regression class where trainerControlUserId stayed -1 and nothing was
    // sent). Pretend a controllable trainer is wired.
    connect(m_ssWorkoutDlg, &WorkoutDialog::setLoad, this, [this](int deviceId, double watts) {
        m_scLoadLog << QStringLiteral("setLoad  dev=%1  %2 W").arg(deviceId).arg(watts, 0, 'f', 1);
    });
    connect(m_ssWorkoutDlg, &WorkoutDialog::setSlope, this, [this](int deviceId, double grade) {
        m_scLoadLog << QStringLiteral("setSlope dev=%1  grade=%2").arg(deviceId).arg(grade, 0, 'f', 3);
    });
    m_ssWorkoutDlg->enableTrainerControl();

    // Start the workout so targets/ERG engage (then loads should be emitted).
    QMetaObject::invokeMethod(m_ssWorkoutDlg, "start_or_pause_workout", Qt::QueuedConnection);

    QTimer::singleShot(3000, this, SLOT(sensorCheckNextStep()));
}

void MainWindow::sensorCheckNextStep()
{
    const int step = m_scStep++;
    auto grab = [this](const QString &name) {
        if (!m_ssWorkoutDlg) return;
        m_ssWorkoutDlg->raise();
        QCoreApplication::processEvents();
        m_ssWorkoutDlg->grab().save(m_scOutDir + QLatin1String("/sensor_") + name + QLatin1String(".png"), "PNG");
    };
    if (!m_ssWorkoutDlg) { qApp->quit(); return; }

    switch (step) {
    case 0:   // Heart rate
        m_ssWorkoutDlg->HrDataReceived(1, 152);
        grab(QStringLiteral("hr"));
        m_scReport << QStringLiteral("HR       : injected 152 bpm   -> sensor_hr.png");
        break;
    case 1:   // Power
        m_ssWorkoutDlg->PowerDataReceived(1, 255);
        grab(QStringLiteral("power"));
        m_scReport << QStringLiteral("POWER    : injected 255 W     -> sensor_power.png");
        break;
    case 2:   // Cadence
        m_ssWorkoutDlg->CadenceDataReceived(1, 92);
        grab(QStringLiteral("cadence"));
        m_scReport << QStringLiteral("CADENCE  : injected 92 rpm    -> sensor_cadence.png");
        break;
    case 3:   // Speed
        m_ssWorkoutDlg->TrainerSpeedDataReceived(1, 34.5);
        grab(QStringLiteral("speed"));
        m_scReport << QStringLiteral("SPEED    : injected 34.5 km/h -> sensor_speed.png");
        break;
    case 4:   // Oxygen (Moxy SmO2 / tHb)
        m_ssWorkoutDlg->OxygenValueChanged(1, 62.0, 12.5);
        grab(QStringLiteral("oxygen"));
        m_scReport << QStringLiteral("OXYGEN   : injected 62%/12.5  -> sensor_oxygen.png");
        break;
    case 5:   // L/R balance + pedal metrics (now decoded + wired)
        m_ssWorkoutDlg->PowerBalanceDataReceived(1, 53);
        m_ssWorkoutDlg->pedalMetricReceived(1, 95.0, 92.0, 0.62, 0.55, 0.58);
        grab(QStringLiteral("balance_pedal"));
        m_scReport << QStringLiteral("BALANCE  : Right 53%% + torque/smoothness -> sensor_balance_pedal.png "
                                     "(balance now decoded from CPM 0x2A63 + wired; pedal metrics driven "
                                     "by the simulator, real-sensor decode pending CP Vector 0x2A64)");
        break;
    case 6:   // ERG / trainer-control output captured since start
        grab(QStringLiteral("erg"));
        m_scReport << QStringLiteral("ERG/CTRL : %1 trainer-control command(s) captured:").arg(m_scLoadLog.size());
        for (const QString &l : m_scLoadLog)
            m_scReport << QStringLiteral("           ") + l;
        if (m_scLoadLog.isEmpty())
            m_scReport << QStringLiteral("           *** NONE — ERG is NOT driving the trainer (regression!) ***");
        break;
    default: {
        QFile f(m_scOutDir + QLatin1String("/sensor_check_report.txt"));
        if (f.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&f) << m_scReport.join(QLatin1Char('\n')) << '\n';
        qInfo().noquote() << "\n===== SENSOR CHECK REPORT =====\n" << m_scReport.join(QLatin1Char('\n'));
        qApp->quit();
        return;
    }
    }
    QTimer::singleShot(900, this, SLOT(sensorCheckNextStep()));
}

void MainWindow::showDemoRaceView()
{
    // Content option 2 = Game (retro ghost-race); loads RetroRace.qml.
    if (m_ssWorkoutDlg)
        m_ssWorkoutDlg->showVideoPlayer(2);
    QCoreApplication::processEvents();
}


void MainWindow::startScreenshotMode(const QString &outputDir)
{
    qDebug() << "Screenshot mode: output dir =" << outputDir;
    QDir().mkpath(outputDir);
    m_ssOutputDir = outputDir;
    m_ssStep      = 0;

    // Remember the user's real studio settings; the studio capture steps toggle
    // these, and they are restored before the app quits (see the final step).
    m_ssSavedStudioEnabled = account->enable_studio_mode;
    m_ssSavedRiderCount    = account->nb_user_studio;

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
    // (No real QtWebEngine on WASM — QWebEngineView is a stub without Q_OBJECT,
    // so findChildren<QWebEngineView*> would not compile, and there are no web
    // pages to blank anyway.)
#ifndef GC_WASM_BUILD
    static const QString kBlankHtml =
        QStringLiteral("<html><body></body></html>");
    for (auto *wv : findChildren<QWebEngineView*>())
        wv->setHtml(kBlankHtml);
#endif

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
        // Capture the Bluetooth Sensors page (FancyTabBar index 3), which hosts
        // sensor pairing plus the trainer/sensor settings.
        ftb->setCurrentIndex(3);
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
    case 3:
        launchDemoWorkout();
        break;

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

    // ── Step 6: capture the Studio tab off (switch only) then on ─────────
    case 6: {
        StudioWidget *sw = qobject_cast<StudioWidget*>(ui->studioWidget);
        ftb->setCurrentIndex(2);

        // Off: only the Studio Mode switch row should show, pinned at the top.
        enableStudioMode(false);
        if (sw) sw->reload();
        QCoreApplication::processEvents();
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_studio_off.png"), "PNG");
        qDebug() << "Screenshot: studio_off";

        // On: reveal the full configuration UI for the studio_mode capture.
        setNumberUserStudio(6);
        enableStudioMode(true);
        if (sw) sw->reload();
        raise();
        activateWindow();
        QCoreApplication::processEvents();
        break;
    }

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

    // ── Step 10: show the Plan page. Its left tab is hidden (see #299), so
    //    select the stacked page directly to still capture the screenshot. ──
    case 10:
        ui->stackedWidget_menu->setCurrentIndex(2);   // Plan page (tab hidden)
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

    // ── Step 12: switch to History tab (tab 4) ────────────────────────────
    case 12:
        ftb->setCurrentIndex(4);
        raise();
        activateWindow();
        QCoreApplication::processEvents();
        break;

    // ── Step 13: capture History ─────────────────────────────────────────
    case 13:
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_history.png"), "PNG");
        qDebug() << "Screenshot: history";
        break;

    // ── Step 14: launch a Studio-mode workout with N simulated riders ─────
    case 14: {
        setNumberUserStudio(15);
        enableStudioMode(true);
        ftb->setCurrentIndex(0);
        ui->tabWidget_workout->setCurrentIndex(0);

        const int nbRiders = qBound(1, account->nb_user_studio, constants::nbMaxUserStudio);
        const QVector<UserStudio> riders = prepareStudioRiders(true);
        m_ssWorkoutDlg = new WorkoutDialog(makeDemoWorkout(), lstRadio, riders, this);

        for (int i = 0; i < nbRiders; ++i) {
            SimulatorHub *simHub = new SimulatorHub(this);
            simHub->setUserID(i + 1);
            connect(simHub, SIGNAL(signal_hr(int,int)),      m_ssWorkoutDlg, SLOT(HrDataReceived(int,int)));
            connect(simHub, SIGNAL(signal_cadence(int,int)), m_ssWorkoutDlg, SLOT(CadenceDataReceived(int,int)));
            connect(simHub, SIGNAL(signal_speed(int,double)),m_ssWorkoutDlg, SLOT(TrainerSpeedDataReceived(int,double)));
            connect(simHub, SIGNAL(signal_power(int,int)),   m_ssWorkoutDlg, SLOT(PowerDataReceived(int,int)));
            connect(simHub, SIGNAL(signal_balance(int,int)), m_ssWorkoutDlg, SLOT(PowerBalanceDataReceived(int,int)));
            connect(simHub, SIGNAL(signal_pedal(int,double,double,double,double,double)), m_ssWorkoutDlg, SLOT(pedalMetricReceived(int,double,double,double,double,double)));
            simHub->start();
            m_ssStudioHubs.append(simHub);
        }
        m_ssWorkoutDlg->show();
        m_ssWorkoutDlg->raise();
        m_ssWorkoutDlg->activateWindow();
        QCoreApplication::processEvents();
        break;
    }

    // ── Step 15: capture the Studio workout, then quit ───────────────────
    case 15:
        if (m_ssWorkoutDlg) {
            m_ssWorkoutDlg->raise();
            QCoreApplication::processEvents();
            m_ssWorkoutDlg->grab().save(
                m_ssOutputDir + QLatin1String("/screenshot_studio_workout.png"), "PNG");
            qDebug() << "Screenshot: studio_workout";
            m_ssWorkoutDlg->hide();
            delete m_ssWorkoutDlg;
            m_ssWorkoutDlg = nullptr;
        }
        for (SimulatorHub *simHub : m_ssStudioHubs) {
            simHub->stopDecodingMsg();
            delete simHub;
        }
        m_ssStudioHubs.clear();
        // Restore the user's real studio settings the capture steps changed.
        account->enable_studio_mode = m_ssSavedStudioEnabled;
        account->nb_user_studio     = m_ssSavedRiderCount;
        account->saveDisplayPrefs();
        scheduleScreenshotQuit();
        return; // No further steps — quit is already scheduled.

    default:
        scheduleScreenshotQuit();
        return;
    }

    // Schedule the next step after its delay.
    const int numDelays = static_cast<int>(sizeof(ssDelays) / sizeof(ssDelays[0]));
    if (step < numDelays)
        QTimer::singleShot(ssDelays[step], this, SLOT(screenshotNextStep()));
    else
        scheduleScreenshotQuit();
}