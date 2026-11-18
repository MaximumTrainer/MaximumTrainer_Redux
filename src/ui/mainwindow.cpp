#include "mainwindow.h"

#include <functional>
#include "setting_keys.h"
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
#include "asyncdialogs.h"
#include "groupconfig.h"
#include "groupsession.h"
#include "groupworkoutcodec.h"
#include "joingroupdialog.h"

namespace {
/// Runs @p load the first time @p view is shown, so pages on tabs the user
/// never opens do not start a Chromium renderer (memory, network, JS) at login.
class LoadOnFirstShow : public QObject
{
public:
    LoadOnFirstShow(QWidget *view, std::function<void()> load)
        : QObject(view), m_load(std::move(load))
    {
        if (view->isVisible()) {
            QTimer::singleShot(0, this, [this]() { fire(); });
            return;
        }
        view->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Show) {
            watched->removeEventFilter(this);
            fire();
        }
        return false;
    }

private:
    void fire()
    {
        if (!m_load)
            return;
        auto load = std::move(m_load);
        m_load = nullptr;
        deleteLater();
        load();
    }

    std::function<void()> m_load;
};
} // namespace

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

// Run the confirmed Log Out path (skipping the confirmation box). Exposed as
// window.mt_triggerLogout() so Playwright can assert the full logout →
// login-screen cycle survives on WASM (#344).
EMSCRIPTEN_KEEPALIVE
void mt_trigger_logout_impl()
{
    if (!g_mainWindow) return;
    QMetaObject::invokeMethod(g_mainWindow.data(), "performLogout",
                              Qt::QueuedConnection);
}

// Open the Preferences dialog. Exposed as window.mt_openPreferences() so
// Playwright can assert it opens without killing the runtime (it used to run
// dconfig->exec(), a fatal nested event loop on WASM — #344 follow-up).
EMSCRIPTEN_KEEPALIVE
void mt_open_preferences_impl()
{
    if (!g_mainWindow) return;
    QMetaObject::invokeMethod(g_mainWindow.data(), "on_actionPreferences_triggered",
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
    window.mt_triggerLogout = function() {
        Module._mt_trigger_logout_impl();
    };
    window.mt_openPreferences = function() {
        Module._mt_open_preferences_impl();
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
    connect(ui->webView_plan, &QWebEngineView::customContextMenuRequested, this, &MainWindow::showPlanContextMenu);

    // Load Intervals.icu athlete calendar, or show a setup prompt if no credentials configured
    if (!account->intervals_icu_athlete_id.isEmpty()) {
        new LoadOnFirstShow(ui->webView_plan, [this]() {
            ui->webView_plan->setUrl(QUrl(urlIntervalsIcuCalendar.arg(account->intervals_icu_athlete_id)));
        });
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
    new LoadOnFirstShow(ui->webView_trainerweb_plans, [this]() {
        ui->webView_trainerweb_plans->setUrl(
            QUrl(QStringLiteral("https://trainerdb-84bdb.firebaseapp.com/listplan")));
    });
    new LoadOnFirstShow(ui->webView_trainerweb_creator, [this]() {
        ui->webView_trainerweb_creator->setUrl(
            QUrl(QStringLiteral("https://trainerdb-84bdb.firebaseapp.com/listworkoutcreator")));
    });
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
    ftb->insertTab(0, QIcon(account->rowing_mode ? ":/image/icon/rower"
                                                  : ":/image/icon/workoutMan"), tr("Workout"));
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
    connect(ftb, &FancyTabBar::currentChanged, this, &MainWindow::leftMenuChanged);


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
    connect (ui->tab_workout1, &Main_WorkoutPage::executeWorkout, this, &MainWindow::executeWorkout);

#ifndef GC_WASM_BUILD
    connect(ui->tab_workout1, &Main_WorkoutPage::hostGroupWorkout, this, &MainWindow::hostGroupWorkout);
    ui->menuTEst->addSeparator();
    QAction *actionHostGroup = ui->menuTEst->addAction(tr("Host Group Workout…"));
    connect(actionHostGroup, &QAction::triggered, this, [this] {
        const std::optional<Workout> selected = ui->tab_workout1->selectedWorkout();
        if (!selected) {
            AsyncDialogs::information(this, tr("Host Group Workout"),
                tr("Select a workout in the Workout list first, then choose Host Group Workout."));
            return;
        }
        hostGroupWorkout(*selected);
    });
    QAction *actionJoinGroup = ui->menuTEst->addAction(tr("Join Group Workout…"));
    actionJoinGroup->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
    connect(actionJoinGroup, &QAction::triggered, this, &MainWindow::showJoinGroupDialog);
#endif


    /// Connect Workout Creator --> WorkoutList Page
    connect(ui->tab_create, &WorkoutCreator::workoutCreated, ui->tab_workout1, &Main_WorkoutPage::addWorkout);
    connect(ui->tab_create, &WorkoutCreator::workoutCreated, this, &MainWindow::showWorkoutList);
    connect(ui->tab_create, &WorkoutCreator::workoutOverwrited, ui->tab_workout1, &Main_WorkoutPage::overwriteWorkout);
    connect(ui->tab_create, &WorkoutCreator::workoutOverwrited, this, &MainWindow::showWorkoutList);

    connect(ui->tab_workout1, qOverload<Workout>(&Main_WorkoutPage::editWorkout), this, &MainWindow::showWorkoutCreator);
    connect(ui->tab_workout1, qOverload<Workout>(&Main_WorkoutPage::editWorkout), ui->tab_create, &WorkoutCreator::editWorkout);


    connect(ui->tab_create, &WorkoutCreator::showStatusBarMessage, ui->widget_bottomMenu, qOverload<const QString&, int>(&BottomBar::setGeneralMessage));


    /// Update create workout graph on FTP and LTHR change
    connect(this, &MainWindow::ftpAndTabProfileChanged, ui->tab_create, &WorkoutCreator::computeWorkout);
    ///Also update workout metrics based on FTP
    connect(this, &MainWindow::ftpAndTabProfileChanged, ui->tab_workout1, &Main_WorkoutPage::updateTableViewMetrics);
    connect(this, &MainWindow::ftpAndTabProfileChanged, ui->tab_workout1, &Main_WorkoutPage::refreshMapWorkout);


    /// Load Settings
    loadSettings();
    isInsideWorkout = false;


    //DialogConfig
    dconfig = new DialogMainWindowConfig(this);

    connect(dconfig, &DialogMainWindowConfig::intervalsIcuCredentialsChanged, this, &MainWindow::reloadPlanWebView);
    connect(dconfig, &DialogMainWindowConfig::intervalsIcuCredentialsChanged,
            ui->tab_intervals_icu, &TabIntervalsIcu::refreshCredentials);
    // Athlete profile (FTP/LTHR/weight) edited in Preferences → recompute zones
    // and workout targets, same as the old profile page's FTP-change path.
    connect(dconfig, &DialogMainWindowConfig::profileChanged,
            this, &MainWindow::ftpAndTabProfileChanged);
    // Rowing mode (Beta) applies live: swap the Workout tab icon and the
    // Devices tab's FTMS slot (Trainer ↔ Rower). Workout sessions read the
    // mode when they start, so nothing else needs refreshing.
    connect(dconfig, &DialogMainWindowConfig::rowingModeChanged, this, [this]() {
        ftb->setTabIcon(0, QIcon(account->rowing_mode ? ":/image/icon/rower"
                                                      : ":/image/icon/workoutMan"));
        if (auto *sw = qobject_cast<SensorsWidget*>(ui->sensorsWidget))
            sw->refreshRowingMode();
    });


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


    connect(planObject, &PlanObject::signal_goToWorkout, this, &MainWindow::goToWorkoutPlanFilter);

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
                ui->tab_workout1->refreshUserWorkout();
                ui->widget_bottomMenu->setGeneralMessage(
                    count > 0
                        ? tr("Intervals.icu sync complete — %1 workout(s) imported.").arg(count)
                        : tr("Intervals.icu sync complete — no new workouts found."),
                    6000);
            });
    connect(ui->tab_intervals_icu, &TabIntervalsIcu::syncFailed,
            this, [this](const QString &err) {
                AsyncDialogs::warning(this, tr("Intervals.icu Sync Failed"), err);
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

    // No local file system in the browser: the Import actions run
    // QFileDialog::getOpenFileName/getExistingDirectory (a fatal nested event
    // loop on WASM) and the Folder menu opens local paths that don't exist.
    ui->menuImport->menuAction()->setVisible(false);
    ui->menuFolder->menuAction()->setVisible(false);
#endif
}


void MainWindow::goToWorkoutPlanFilter(const QString& workoutId) {


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
    connect(replyIntervalsIcuZwo, &QNetworkReply::finished, this, &MainWindow::onIntervalsIcuWorkoutDownloaded);
}


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


void MainWindow::showPlanContextMenu(const QPoint &pos) {

    QMenu *menu = new QMenu(ui->webView_plan);
    menu->addAction(tr("Refresh"), this, &MainWindow::reloadPlanWebView);
    // popup() instead of exec(): a nested event loop is fatal on WASM.
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(ui->webView_plan->mapToGlobal(pos));
}

// trigger after a .zwo file downloaded from Intervals.icu is saved
void MainWindow::goToWorkoutNameFilterFromIntervals(const QString &workoutName) {

    qInfo() << "Intervals.icu workout downloaded:" << workoutName;

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
        AsyncDialogs::warning(this,
                              tr("Queue Error"),
                              tr("Could not load the queued workout:\n%1\n(%2)").arg(name, path));
        tryAdvanceWorkoutQueue();
    }
}

void MainWindow::tryAdvanceWorkoutQueue()
{
    // Take the next item NOW (atomically), skipping any blank entries. The
    // event loop keeps running while the countdown below is open, so if we only
    // read here and deferred the dequeue, a re-entrant advance could read the
    // same item again or observe a half-updated queue — that produced the
    // empty-path "Could not load the next queued workout" error.
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

    // Countdown runs via open() + finished — exec() is fatal on WASM.
    auto *countdown = new WorkoutCountdownDialog(nextName, 60, this);
    countdown->setAttribute(Qt::WA_DeleteOnClose);
    connect(countdown, &QDialog::finished, this,
            [this, countdown, nextName, nextPath](int result) {
        if (result != QDialog::Accepted) {
            // User chose "Cancel Queue" — clear the remaining queue so it
            // doesn't auto-advance after subsequent workouts.
            if (countdown->dialogResult() == WorkoutCountdownDialog::Result::Cancelled)
                m_workoutQueue->clear();
            return;
        }

        // Defer via singleShot so the dialog teardown fully unwinds before
        // the next workout begins — avoids unbounded recursion with long queues.
        QTimer::singleShot(0, this, [this, nextName, nextPath]() {
            XmlUtil xmlNext;
            Workout next = xmlNext.parseSingleWorkoutXml(nextPath);
            if (!next.getName().isEmpty()) {
                executeWorkout(next);
            } else {
                AsyncDialogs::warning(this,
                                      tr("Queue Error"),
                                      tr("Could not load the next queued workout:\n%1\n(%2)")
                                          .arg(nextName, nextPath));
                // The bad entry is already removed; keep the queue moving.
                tryAdvanceWorkoutQueue();
            }
        });
    });
    countdown->open();
}

void MainWindow::workoutExecuting() {

    this->setDisabled(true);
    isInsideWorkout = true;
    qInfo() << "Workout started";


    for (int i=0; i<ftb->getNumberOfTabs(); i++)
        ftb->setTabEnabled(i, false);

}
void MainWindow::workoutOver() {

    this->setDisabled(false);
    isInsideWorkout = false;
    qInfo() << "Workout closed";

    for (int i=0; i<ftb->getNumberOfTabs(); i++)
        ftb->setTabEnabled(i, true);

    if (account->enable_studio_mode) {
        enableStudioMode(true);
    }

}


void MainWindow::leftMenuChanged(int tabSelected) {


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


void MainWindow::createWebChannelPlan() {


    QFile webChannelJsFile(":/qtwebchannel/qwebchannel.js");
    if(  !webChannelJsFile.open(QIODevice::ReadOnly) ) {
        LOG_WARN("MainWindow", QStringLiteral("Could not open qwebchannel.js: ") + webChannelJsFile.errorString());
    }
    else {
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


void MainWindow::updateVecStudio(QVector<UserStudio> vecUserStudio) {

    this->vecUserStudio = vecUserStudio;

    // Persist immediately (e.g. after a Studio FTP test updates every rider's
    // FTP/LTHR) so the Studio tab reflects the new values on its next open and
    // they survive a restart, rather than only being written on app close.
    UserStudio::saveStudioConfig(this->vecUserStudio);
}

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
void MainWindow::updateFieldForUser(int riderID, int fieldNumber, QVariant value) {


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

void MainWindow::enableStudioMode(bool enable) {

    account->enable_studio_mode = enable;

    //    nb_user_studio

    qInfo() << "Studio mode:" << enable;


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

void MainWindow::setNumberUserStudio(int nbUser) {

    account->nb_user_studio = nbUser;

    // Persist so the selected rider count survives a restart.
    account->saveDisplayPrefs();
}

void MainWindow::showWorkoutCreator() {

    ui->tabWidget_workout->setCurrentIndex(1);
}

void MainWindow::showWorkoutList() {

    ui->tabWidget_workout->setCurrentIndex(0);
}


void MainWindow::loadSettings() {


    const QSettings settings;
    resize(AppSettings::read(settings, SettingKeys::MainWindow::Size));
    move(AppSettings::read(settings, SettingKeys::MainWindow::Pos));
    const bool wasMaximized = AppSettings::read(settings, SettingKeys::MainWindow::Maximized);


    if (wasMaximized) {
        this->showMaximized();
    }
    else {
        this->showNormal();
    }
}


void MainWindow::saveSettings() {


    QSettings settings;
    AppSettings::write(settings, SettingKeys::MainWindow::Size, size());
    AppSettings::write(settings, SettingKeys::MainWindow::Pos, pos());
    AppSettings::write(settings, SettingKeys::MainWindow::Maximized, isMaximized());


}


void MainWindow::closeEvent(QCloseEvent *event) {


    //    Qt::WindowFlags flags = Qt::Window;
    //    if (settings->forceWorkoutWindowOnTop)
    //        flags = flags | Qt::WindowStaysOnTopHint;
    //    this->setWindowFlags(flags);


    if (isInsideWorkout) {
        // Shown non-blocking: exec() is a qFatal on Qt WASM without asyncify,
        // and the close is refused regardless of when the box is dismissed.
        auto *msgBox = new QMessageBox(this);
        msgBox->setAttribute(Qt::WA_DeleteOnClose);
        msgBox->setWindowFlags(msgBox->windowFlags() | Qt::WindowStaysOnTopHint);
        msgBox->setIcon(QMessageBox::Question);
        msgBox->setText(tr("A workout is still active."));
        msgBox->setInformativeText(tr("Please close any active workout before leaving MaximumTrainer."));
        msgBox->setStandardButtons(QMessageBox::Ok);
        msgBox->setDefaultButton(QMessageBox::Ok);
        msgBox->open();
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
    QTimer::singleShot(300, qApp, &QCoreApplication::quit);
}

//QMENUBAR
void MainWindow::on_actionExit_triggered()
{
    this->close();
}

void MainWindow::on_actionLogout_triggered()
{
    // Confirm asynchronously: QMessageBox::question() spins a nested event
    // loop (QDialog::exec), which is a qFatal on Qt WASM without asyncify —
    // the whole runtime aborted on Log Out (#344).
    AsyncDialogs::question(
        this, tr("Log Out"),
        tr("Log out of Intervals.icu and return to the login screen?"),
        [this]() { performLogout(); });
}

void MainWindow::performLogout()
{
    if (account)
        account->logout();

    // close() runs the normal save path; closeEvent then re-shows the login
    // screen in-process (instead of quitting) because we're logging out.
    m_loggingOut = true;
    this->close();
}


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
void MainWindow::on_actionAbout_Qt_triggered()
{
    QMessageBox::aboutQt(this);


}
void MainWindow::on_actionRequest_Help_triggered()
{
    // Open the online user guide in the user's default browser.
    QDesktopServices::openUrl(QUrl(QStringLiteral(
        "https://maximumtrainer.github.io/MaximumTrainer_Redux/user-guide.html")));
}
void MainWindow::on_actionKeyboard_Shortcuts_triggered()
{
    // open(), not exec(): a nested event loop is fatal on WASM.
    auto *dlg = new DialogKeyboardShortcuts(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->open();
}
void MainWindow::on_actionPreferences_triggered()
{
    dconfig->open();
}
void MainWindow::on_actionWorkout_triggered()
{
    Util::openWorkoutFolder("null");
}
void MainWindow::on_actionHistory_triggered()
{
    // "Folder → Open History Folder": open the history folder in the file
    // manager (it previously just switched to the History tab by mistake).
    Util::openHistoryFolder();
}


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


void MainWindow::on_actionCreate_New_triggered()
{


    ui->tabWidget_workout->setCurrentIndex(1);
    ftb->setCurrentIndex(0);
    ui->tab_create->resetWorkout();


}


void MainWindow::on_actionOpen_Ride_triggered()
{

    qInfo() << "Free ride started";


    QList<Interval> lstInterval;
    Interval interval;
    lstInterval.append(interval);

    Workout freeRide("null", Workout::OPEN_RIDE, lstInterval,
                     tr("Free Ride"), "MaximumTrainer", tr("Go and ride as long as you want"), "-", Workout::T_OTHERS);

    executeWorkout(freeRide);
}


void MainWindow::executeWorkout(Workout workout) {

    // Re-entrancy guard: the launch flow below runs nested event loops (the
    // connection-method / sensor dialogs and the BLE connect wait), during which
    // a queued double-click on the workout list could otherwise re-enter here and
    // spawn a second (or third) WorkoutDialog. Block until this launch resolves;
    // once the workout is actually running isInsideWorkout keeps it blocked.
    if (m_launchingWorkout || isInsideWorkout)
        return;
    m_launchingWorkout = true;

#ifdef GC_WASM_BUILD
    // WASM: fully async chain — any nested event loop (QDialog::exec /
    // QEventLoop::exec) is a qFatal on Qt WASM without asyncify. The
    // m_launchingWorkout guard is released by each terminal continuation.
    auto *connDlg = new DialogConnectionMethod(this);
    connDlg->setAttribute(Qt::WA_DeleteOnClose);
    connect(connDlg, &QDialog::finished, this, [this, workout, connDlg](int result) {
        if (result != QDialog::Accepted) {
            m_launchingWorkout = false;
            return;
        }
        if (connDlg->selectedMethod() == DialogConnectionMethod::Simulation)
            startSimulatedWorkout(workout);
        else
            startWasmBleWorkout(workout);
    });
    connDlg->open();
    return;
#else
    auto launchGuard = qScopeGuard([this]() { m_launchingWorkout = false; });

    DialogConnectionMethod connDlg(this);
    if (connDlg.exec() != QDialog::Accepted)
        return;


    // ── Simulation path ───────────────────────────────────────────────────
    if (connDlg.selectedMethod() == DialogConnectionMethod::Simulation) {
        startSimulatedWorkout(workout);
        return;
    }

    // ── BTLE Device path ─────────────────────────────────────────────────
    // Studio mode: connect each rider's own saved sensor package in turn, tag
    // every hub with that rider's id, then run the workout with all of them.
    if (account->enable_studio_mode) {
        const int nbRiders = qBound(1, account->nb_user_studio, constants::nbMaxUserStudio);
        QList<QPair<int, QMap<BtleSensorRole, BtleHub*>>> riderHubs;

        for (int rider = 1; rider <= nbRiders; ++rider) {
            const QMap<BtleSensorRole, BtleSavedSensor> saved = BtleSensorStore::loadAll(rider);
            if (saved.isEmpty())
                continue;   // rider configured no sensors — their box stays empty

            SensorConnectDialog connectDlg(saved, this);
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
            SensorConnectDialog connectDlg(savedSensors, this);
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

    BtleScannerDialog scanner(this);
    if (scanner.exec() != QDialog::Accepted || !scanner.hasSelection())
        return;

    QApplication::setOverrideCursor(Qt::WaitCursor);


    BtleHub *btleHub = new BtleHub(this);

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
        AsyncDialogs::warning(this,
                              tr("Connection Failed"),
                              tr("Could not connect to the selected Bluetooth device.\n"
                                 "Please check the device is powered on and try again."));
        delete btleHub;
        return;
    }

    startWorkoutWithConnectedHub(workout, btleHub);
#endif // GC_WASM_BUILD
}


void MainWindow::connectSimulatorHub(SimulatorHub *hub, WorkoutDialog *dialog)
{
    connect(hub, &SimulatorHub::signal_hr,      dialog, &WorkoutDialog::HrDataReceived);
    connect(hub, &SimulatorHub::signal_cadence, dialog, &WorkoutDialog::CadenceDataReceived);
    connect(hub, &SimulatorHub::signal_speed,   dialog, &WorkoutDialog::TrainerSpeedDataReceived);
    connect(hub, &SimulatorHub::signal_power,   dialog, &WorkoutDialog::PowerDataReceived);
    connect(hub, &SimulatorHub::signal_balance, dialog, &WorkoutDialog::PowerBalanceDataReceived);
    connect(hub, &SimulatorHub::signal_oxygen,  dialog, &WorkoutDialog::OxygenValueChanged);
}

void MainWindow::startSimulatedWorkout(const Workout &workout)
{
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
        simHub->setRowingMode(account->rowing_mode);
        simHub->setUserID(i + 1);

        connectSimulatorHub(simHub, w);

        connect(w, &WorkoutDialog::setLoad, simHub, &SimulatorHub::setLoad);
        connect(w, &WorkoutDialog::setSlope, simHub, &SimulatorHub::setSlope);
        connect(w, &WorkoutDialog::stopDecodingMsgHub, simHub, &SimulatorHub::stopDecodingMsg);

        simHubs.append(simHub);
    }
    w->enableTrainerControl();

    connect(w, &WorkoutDialog::fitFileReady, this, &MainWindow::checkToUploadFile);
    connect(w, &WorkoutDialog::ftp_lthr_changed, ui->tab_workout1, &Main_WorkoutPage::updateTableViewMetrics);
    connect(w, &WorkoutDialog::ftpUserStudioChanged, this, &MainWindow::updateVecStudio);

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
    attachPendingGroupSession(w);
    workoutExecuting();
    QApplication::restoreOverrideCursor();
    w->show();
    m_launchingWorkout = false;
}


#ifdef GC_WASM_BUILD
void MainWindow::startWasmBleWorkout(const Workout &workout)
{
    // Browser BLE picker via open(); the connect wait is signal-driven (the
    // desktop QEventLoop wait would be a fatal nested loop here).
    auto *scanner = new BtleScannerDialog(this);
    scanner->setAttribute(Qt::WA_DeleteOnClose);
    connect(scanner, &QDialog::finished, this, [this, workout, scanner](int result) {
        if (result != QDialog::Accepted || !scanner->hasSelection()) {
            m_launchingWorkout = false;
            return;
        }

        BtleHub *btleHub = new BtleHub(this);

        // Context object: deleting it disconnects all three outcome handlers,
        // so exactly one continuation runs per connection attempt.
        QObject *attempt = new QObject(this);
        connect(btleHub, &BtleHub::deviceConnected, attempt,
                [this, workout, btleHub, attempt]() {
            attempt->deleteLater();
            startWorkoutWithConnectedHub(workout, btleHub);
        });
        const auto onFailed = [this, btleHub, attempt]() {
            attempt->deleteLater();
            AsyncDialogs::warning(this,
                                  tr("Connection Failed"),
                                  tr("Could not connect to the selected Bluetooth device.\n"
                                     "Please check the device is powered on and try again."));
            btleHub->deleteLater();
            m_launchingWorkout = false;
        };
        connect(btleHub, &BtleHub::deviceDisconnected, attempt, onFailed);
        connect(btleHub, &BtleHub::connectionError, attempt,
                [onFailed](const QString &) { onFailed(); });

        btleHub->connectToDevice(scanner->selectedDevice());
    });
    scanner->open();
}
#endif // GC_WASM_BUILD


void MainWindow::startWorkoutWithConnectedHub(const Workout &workout, BtleHub *btleHub)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Show WorkoutDialog NON-MODALLY (window-modal, run on the main event loop
    // instead of QDialog::exec()) so the embedded QWebEngine video player does
    // not tear down the dialog when it initialises. See the simulation path
    // above for the full rationale. Post-workout cleanup moves to finished().
    WorkoutDialog *w = new WorkoutDialog(workout, lstRadio, vecUserStudio);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowModality(Qt::ApplicationModal);

    connect(btleHub, &BtleHub::signal_hr, w, &WorkoutDialog::HrDataReceived);
    connect(btleHub, &BtleHub::signal_cadence, w, &WorkoutDialog::CadenceDataReceived);
    connect(btleHub, &BtleHub::signal_speed, w, &WorkoutDialog::TrainerSpeedDataReceived);
    connect(btleHub, &BtleHub::signal_power, w, &WorkoutDialog::PowerDataReceived);
#ifndef GC_WASM_BUILD
    // The Web Bluetooth hub has no L/R balance signal.
    connect(btleHub, &BtleHub::signal_balance, w, &WorkoutDialog::PowerBalanceDataReceived);
#endif
    connect(btleHub, &BtleHub::signal_oxygen, w, &WorkoutDialog::OxygenValueChanged);
    connect(btleHub, &BtleHub::signal_battery, w, &WorkoutDialog::batteryStatusReceived);

    // Surface BLE disconnections mid-workout so WorkoutDialog can pause and
    // the user sees the DOM reconnect overlay (WASM) or a status message.
    connect(btleHub, &BtleHub::connectionError, w, &WorkoutDialog::onBleConnectionError);

    connect(w, &WorkoutDialog::setLoad, btleHub, &BtleHub::setLoad);
    connect(w, &WorkoutDialog::setSlope, btleHub, &BtleHub::setSlope);
    connect(w, &WorkoutDialog::setResistance, btleHub, &BtleHub::setResistanceLevel);
    connect(w, &WorkoutDialog::stopDecodingMsgHub, btleHub, &BtleHub::stopDecodingMsg);
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

    connect(w, &WorkoutDialog::fitFileReady, this, &MainWindow::checkToUploadFile);
    connect(w, &WorkoutDialog::ftp_lthr_changed, ui->tab_workout1, &Main_WorkoutPage::updateTableViewMetrics);
    connect(w, &WorkoutDialog::ftpUserStudioChanged, this, &MainWindow::updateVecStudio);

    connect(w, &QDialog::finished, this, [this, btleHub]() {
        workoutOver();
        btleHub->disconnectFromDevice();
        delete btleHub;
        // Auto-advance to next queued workout if one exists
        tryAdvanceWorkoutQueue();
    });

    attachPendingGroupSession(w);
    workoutExecuting();
    QApplication::restoreOverrideCursor();
    w->show();
    m_launchingWorkout = false;
}


#ifndef GC_WASM_BUILD
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

    connect(w, &WorkoutDialog::fitFileReady, this, &MainWindow::checkToUploadFile);
    connect(w, &WorkoutDialog::ftp_lthr_changed, ui->tab_workout1, &Main_WorkoutPage::updateTableViewMetrics);
    connect(w, &WorkoutDialog::ftpUserStudioChanged, this, &MainWindow::updateVecStudio);

    connect(w, &QDialog::finished, this, [this, allHubs]() {
        workoutOver();
        for (BtleHub *hub : allHubs) {
            hub->disconnectFromDevice();
            delete hub;
        }
        // Auto-advance to next queued workout if one exists
        tryAdvanceWorkoutQueue();
    });

    attachPendingGroupSession(w);
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
                                  const QMap<BtleSensorRole, BtleHub*> &hubsByRoleIn,
                                  QSet<BtleHub*> &allHubs,
                                  int riderIndex)
{
    // A rower is FTMS like a trainer — alias it onto the Trainer wiring path
    // (power/stroke-rate/pace channels, resistance control, HR bridge) so the
    // rest of this function needs no rowing-specific branches.
    QMap<BtleSensorRole, BtleHub*> hubsByRole = hubsByRoleIn;
    if (hubsByRole.contains(BtleSensorRole::Rower) && !hubsByRole.contains(BtleSensorRole::Trainer))
        hubsByRole.insert(BtleSensorRole::Trainer, hubsByRole.value(BtleSensorRole::Rower));

    QSet<BtleHub*> uniqueHubs;
    for (BtleHub *hub : hubsByRole)
        uniqueHubs.insert(hub);

    for (BtleHub *hub : uniqueHubs) {
        allHubs.insert(hub);
        connect(hub, &BtleHub::signal_battery,  w, &WorkoutDialog::batteryStatusReceived);
        connect(hub, &BtleHub::connectionError, w, &WorkoutDialog::onBleConnectionError);
        connect(w, &WorkoutDialog::stopDecodingMsgHub, hub, &BtleHub::stopDecodingMsg);
    }

    BtleHub *powerHub   = nullptr;
    BtleHub *cadenceHub = nullptr;
    BtleHub *speedHub   = nullptr;
    BtleHub *hrHub      = nullptr;

    auto wireHr = [&](BtleHub *hub) {
        if (hrHub == hub) return;
        hrHub = hub;
        connect(hub, &BtleHub::signal_hr, w, &WorkoutDialog::HrDataReceived);
    };

    auto wirePower = [&](BtleHub *hub) {
        if (powerHub == hub) return;          // already wired from this device
        powerHub = hub;
        connect(hub, &BtleHub::signal_power, w, &WorkoutDialog::PowerDataReceived);
        connect(hub, &BtleHub::signal_balance, w, &WorkoutDialog::PowerBalanceDataReceived);
    };
    auto wireCadence = [&](BtleHub *hub) {
        if (cadenceHub == hub) return;
        cadenceHub = hub;
        connect(hub, &BtleHub::signal_cadence, w, &WorkoutDialog::CadenceDataReceived);
    };
    auto wireSpeed = [&](BtleHub *hub) {
        if (speedHub == hub) return;
        speedHub = hub;
        connect(hub, &BtleHub::signal_speed, w, &WorkoutDialog::TrainerSpeedDataReceived);
    };

    // Wire the trainer first so resistance control is set up and its data takes
    // precedence, except cadence: a dedicated CadenceSpeed sensor owns that.
    if (hubsByRole.contains(BtleSensorRole::Trainer)) {
        BtleHub *hub = hubsByRole.value(BtleSensorRole::Trainer);
        wirePower(hub);
        if (trainerSuppliesCadence(hubsByRoleIn.keys()))
            wireCadence(hub);
        wireSpeed(hub);
        // Remember when this trainer bridges an HR strap so the sensors page can
        // mark the Heart Rate slot "provided by trainer" next time it opens.
        connect(hub, &BtleHub::signal_trainerProvidesHr, this,
                [riderIndex]() { BtleSensorStore::setTrainerProvidesHr(true, riderIndex); });
        connect(w, &WorkoutDialog::setLoad, hub, &BtleHub::setLoad);
        connect(w, &WorkoutDialog::setSlope, hub, &BtleHub::setSlope);
        connect(w, &WorkoutDialog::setResistance, hub, &BtleHub::setResistanceLevel);
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
                &BtleHub::signal_oxygen, w, &WorkoutDialog::OxygenValueChanged);
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

    connect(w, &WorkoutDialog::fitFileReady, this, &MainWindow::checkToUploadFile);
    connect(w, &WorkoutDialog::ftp_lthr_changed, ui->tab_workout1, &Main_WorkoutPage::updateTableViewMetrics);
    connect(w, &WorkoutDialog::ftpUserStudioChanged, this, &MainWindow::updateVecStudio);

    connect(w, &QDialog::finished, this, [this, allHubs]() {
        workoutOver();
        for (BtleHub *hub : allHubs) {
            hub->disconnectFromDevice();
            delete hub;
        }
        tryAdvanceWorkoutQueue();
    });

    attachPendingGroupSession(w);
    workoutExecuting();
    QApplication::restoreOverrideCursor();
    w->show();
}
#endif // GC_WASM_BUILD


QString MainWindow::loadPathImport() const {

    return AppSettings::read(SettingKeys::LastPath::WorkoutImport, QDir::homePath());

}
void MainWindow::savePathImport(const QString& filepath) {

    AppSettings::write(SettingKeys::LastPath::WorkoutImport, filepath);
}


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

void MainWindow::on_actionMultiple_Workouts_triggered()
{


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


void MainWindow::checkToUploadFile(const QString& filename, const QString& nameOnly, const QString& description) {

    Q_UNUSED(description);

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


void MainWindow::setPmForCadence(bool usedFor) {

    account->use_pm_for_cadence = usedFor;
}
void MainWindow::setPmForSpeed(bool usedFor) {

    account->use_pm_for_speed = usedFor;
}


QString MainWindow::groupDisplayName() const {
    if (!account->first_name.trimmed().isEmpty()) return account->first_name.trimmed();
    if (!account->display_name.trimmed().isEmpty()) return account->display_name.trimmed();
    return tr("Rider");
}

bool MainWindow::canRideInGroup(const Workout &workout) {
    if (isInsideWorkout || m_launchingWorkout) return false;
    if (account->enable_studio_mode) {
        AsyncDialogs::information(this, tr("Group Workout"),
            tr("Group workouts aren't available in Studio mode. Turn Studio mode off to ride with others."));
        return false;
    }
    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {
        AsyncDialogs::information(this, tr("Group Workout"),
            tr("Free Ride can't be shared yet. Pick a structured workout to ride with others."));
        return false;
    }
    return true;
}

void MainWindow::hostGroupWorkout(const Workout &workout) {
    if (!canRideInGroup(workout)) return;

    const std::optional<GroupProtocol::SharedWorkout> shared = GroupWorkoutCodec::encode(workout);
    if (!shared) {
        AsyncDialogs::warning(this, tr("Host Group Workout"), tr("This workout could not be prepared for sharing."));
        return;
    }

    auto *session = new GroupSession(GroupConfig::relayBaseUrl(), this);
    ui->widget_bottomMenu->setGeneralMessage(tr("Creating a group room…"), 10000);
    connect(session, &GroupSession::joined, this, [this, session, workout, shared] {
        session->disconnect(this);
        session->shareWorkout(*shared);
        ui->widget_bottomMenu->setGeneralMessage(tr("Group room %1 created").arg(session->roomCode()), 5000);
        startGroupWorkout(workout, session);
    }, Qt::QueuedConnection);
    connect(session, &GroupSession::relayError, this, [this, session](const QString &errorCode) {
        session->disconnect(this);
        session->deleteLater();
        AsyncDialogs::warning(this, tr("Host Group Workout"),
            errorCode == QLatin1String("relay_unreachable")
                ? tr("Could not reach the group server. Check your internet connection.")
                : tr("The group server refused to create a room (%1).").arg(errorCode));
    });
    session->hostRoom(groupDisplayName());
}

void MainWindow::showJoinGroupDialog() {
    if (isInsideWorkout || m_launchingWorkout) return;
    auto *dialog = new JoinGroupDialog(GroupConfig::relayBaseUrl(), groupDisplayName(), this);
    connect(dialog, &JoinGroupDialog::workoutReady, this,
            [this](GroupSession *session, const Workout &workout) {
        if (!canRideInGroup(workout)) {
            session->leave();
            session->deleteLater();
            return;
        }
        startGroupWorkout(workout, session);
    }, Qt::QueuedConnection);
    dialog->open();
}

void MainWindow::startGroupWorkout(const Workout &workout, GroupSession *session) {
    session->setParent(this);
    m_pendingGroupSession = session;
    executeWorkout(workout);
#ifndef GC_WASM_BUILD
    // The desktop launch runs to the shown WorkoutDialog before returning, so a
    // session that is still pending means the rider cancelled the launch.
    if (m_pendingGroupSession) {
        m_pendingGroupSession->leave();
        m_pendingGroupSession->deleteLater();
        m_pendingGroupSession = nullptr;
    }
#endif
}

void MainWindow::attachPendingGroupSession(WorkoutDialog *dialog) {
    if (!m_pendingGroupSession) return;
    GroupSession *session = m_pendingGroupSession;
    m_pendingGroupSession = nullptr;
    dialog->setGroupSession(session);
    connect(dialog, &QDialog::finished, session, [session] {
        session->leave();
        session->deleteLater();
    });
}
