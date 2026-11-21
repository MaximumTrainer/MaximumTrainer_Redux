#include "mainwindow.h"

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
#include <QToolButton>
#include "updatenotifier.h"
#include "achievementswidget.h"
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
#include "networkmonitor.h"
#include "dialogkeyboardshortcuts.h"
#include "apptheme.h"

#include <QDir>
#include <QRegularExpression>
#include <QWebEngineView>
#ifndef GC_WASM_BUILD
#endif

#include "extrequest.h"
#include "strava_service.h"
#ifdef GC_WASM_BUILD
#include "btle_scanner_dialog_wasm.h"
#else
#include "btle_scanner_dialog.h"
#include "btle_sensor_store.h"
#include "sensor_connector.h"
#include "sensorstatusstrip.h"
#include "studioconnectionboard.h"
#include "userstudiowidget.h"
#include "zwift_click_manager.h"
#endif
#include "sensorswidget.h"
#include "studiowidget.h"
#include "asyncdialogs.h"
#include "groupconfig.h"
#include "groupsession.h"
#include "groupworkoutcodec.h"
#include "joingroupdialog.h"

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
    delete ui;

}


MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {

    ui->setupUi(this);
    this->setEnabled(false);
    ui->widget_bottomMenu->setGeneralMessage(tr("Retrieving Data...")); //will be removed when response is received
    ui->actionOpen_Ride->setEnabled(true);

    this->settings = qApp->property("User_Settings").value<Settings*>();
    this->account = qApp->property("Account").value<Account*>();

    // Initialise the Intervals.icu tab with current credentials
    ui->tab_intervals_icu->refreshCredentials();


    // ------------------------------------- BTLE ready ---------------------------------


    ftb = new FancyTabBar(FancyTabBar::TabBarPosition::Left, ui->widget_fancyMenu);
    ftb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Tab indices match the pages in stackedWidget_menu.
    ftb->insertTab(0, QIcon(account->rowing_mode ? ":/image/icon/rower"
                                                  : ":/image/icon/workoutMan"), tr("Workout"));
    ftb->insertTab(1, QIcon(":/image/icon/intervals"),   tr("Intervals.icu"));
    ftb->insertTab(2, QIcon(":/image/icon/studio"), tr("Studio"));
    ftb->insertTab(3, QIcon(":/image/icon/bluetooth"), tr("Devices"));
    ftb->insertTab(4, QIcon(":/image/icon/chart"), tr("History"));
    ftb->insertTab(5, QIcon(":/image/icon/trophy"), tr("Achievements"));

    ftb->setTabEnabled(0, true);
    ftb->setTabEnabled(1, true);
    ftb->setTabEnabled(2, true);
    ftb->setTabEnabled(3, true);
    ftb->setTabEnabled(4, true);
    ftb->setTabEnabled(5, true);


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
    // Catch up on achievements the ride history already earned, without popups.
    QTimer::singleShot(0, achievementManager, [achievementManager]() { achievementManager->evaluateHistory(false); });
    m_achievementsPage = new AchievementsWidget(ui->stackedWidget_menu);
    ui->stackedWidget_menu->addWidget(m_achievementsPage);   // page 5, after the .ui pages


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
    // Screenshot mode feeds the docs: no "Update available" banner in them.
    if (!QCoreApplication::arguments().contains(QLatin1String("--screenshots"), Qt::CaseInsensitive)) {
        auto *updateNotifier = new UpdateNotifier(this);
        connect(updateNotifier, &UpdateNotifier::updateAvailable, this, &MainWindow::showUpdateAvailable);
        updateNotifier->check();
    }

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


    // Tabs: 0 Workout, 1 Intervals.icu, 2 Studio, 3 Devices, 4 History, 5 Achievements.
    // Stacked pages: Workout 0, Intervals 1, Studio 2, History 3, Sensors 4, Achievements 5.
    static const int tabToPage[] = {0, 1, 2, 4, 3, 5};
    const int pageIndex = (tabSelected >= 0 && tabSelected < 6)
                              ? tabToPage[tabSelected]
                              : tabSelected;
    ui->stackedWidget_menu->setCurrentIndex(pageIndex);
    currentIndexLeftMenu = tabSelected;

    // Refresh saved sensors each time the Devices tab is opened.
    if (tabSelected == 3) {
        if (auto *sw = qobject_cast<SensorsWidget*>(ui->sensorsWidget))
            sw->reload();
    }
    if (tabSelected == 5 && m_achievementsPage)
        m_achievementsPage->reload();
    // Refresh studio settings each time the Studio tab is opened.
    if (tabSelected == 2) {
        if (auto *sw = qobject_cast<StudioWidget*>(ui->studioWidget))
            sw->reload();
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

    // Disable the Devices tab while in studio mode: sensor pairing is
    // per-rider and not meaningful in the multi-rider studio view.
    ftb->setTabEnabled(3, !enable);

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
    QString openSource = tr("%1 is free and open source. Contributions are welcome: "
                            "fork it, open an issue, or send a pull request on "
                            "<a href=\"%2\">GitHub</a>.")
                            .arg(appName, repoUrl);
    // open(), not QMessageBox::about(): its exec() is fatal on WASM.
    auto *about = new QMessageBox(this);
    about->setAttribute(Qt::WA_DeleteOnClose);
    about->setWindowTitle(tr("About ") + appName);
    about->setIconPixmap(windowIcon().pixmap(64, 64));
    about->setTextFormat(Qt::RichText);
    about->setTextInteractionFlags(Qt::TextBrowserInteraction);
    about->setText("<b>"+ nameWithVersion + "</b><br/>" +
                   copyright + "<hr/>" +
                   openSource + "<hr/>" +
                   tr("Built with <a href=\"https://www.qt.io/\">Qt</a> %1, used under the "
                      "<a href=\"https://www.gnu.org/licenses/lgpl-3.0.html\">LGPL v3</a>.")
                       .arg(QString::fromLatin1(qVersion())) + "<br/>" +
                   tr("Normalized Power®(NP), Training Stress Score®(TSS) and Intensity Factor®(IF) are registered trademarks of Peaksware LLC."));
    about->open();
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
void MainWindow::showUpdateAvailable(const QString &latestVersion)
{
    const QString label = tr("Update available: %1").arg(latestVersion);
    auto *download = new QAction(label, this);
    download->setToolTip(tr("Open the download page for MaximumTrainer %1").arg(latestVersion));
    connect(download, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl(urlGitHubReleasesPage));
    });

    // Help menu entry for platforms with a native menu bar (macOS), where a
    // corner widget is not shown.
    ui->menu->insertAction(ui->menu->actions().value(0), download);
    ui->menu->insertSeparator(ui->menu->actions().value(1));

    auto *button = new QToolButton(ui->menuBar);
    button->setDefaultAction(download);
    button->setAutoRaise(true);
    button->setCursor(Qt::PointingHandCursor);
    QFont font = button->font();
    font.setBold(true);
    button->setFont(font);
    QPalette palette = button->palette();
    palette.setColor(QPalette::ButtonText, palette.color(QPalette::Highlight));
    button->setPalette(palette);
    ui->menuBar->setCornerWidget(button, Qt::TopRightCorner);
    button->show();
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
    // sensor dialogs and the BLE connect wait), during which
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
    if (account->use_simulated_sensors)
        startSimulatedWorkout(workout);
    else
        startWasmBleWorkout(workout);
    return;
#else
    auto launchGuard = qScopeGuard([this]() { m_launchingWorkout = false; });

    // Devices > Simulated Sensors: generated data, no hardware.
    if (account->use_simulated_sensors) {
        startSimulatedWorkout(workout);
        return;
    }

    // ── BTLE Device path ─────────────────────────────────────────────────
    // Studio mode: every rider's saved sensors connect at once inside the
    // workout window (connection board, then a dot per sensor on each card).
    if (account->enable_studio_mode) {
        startStudioWorkoutConnectingSensors(workout);
        return;
    }

    // Saved sensors connect inside the workout window; with none saved, pick
    // one device with the scanner below.
    {
        const QMap<BtleSensorRole, BtleSavedSensor> savedSensors = BtleSensorStore::loadAll();
        if (!SensorConnector::slotsFor(savedSensors, account->rowing_mode).isEmpty()) {
            startWorkoutConnectingSensors(workout, savedSensors);
            return;
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

    connect(w, &WorkoutDialog::ftp_lthr_changed, ui->tab_workout1, &Main_WorkoutPage::updateTableViewMetrics);
    connect(w, &WorkoutDialog::ftpUserStudioChanged, this, &MainWindow::updateVecStudio);

    connect(w, &QDialog::finished, this, [this, simHubs]() {
        workoutOver();
        for (SimulatorHub *simHub : simHubs) {
            simHub->stopDecodingMsg();
            delete simHub;
        }
    });

    for (SimulatorHub *simHub : simHubs)
        simHub->start();
    attachPendingGroupSession(w);
    workoutExecuting();
    QApplication::restoreOverrideCursor();
    w->show();
    w->showSimulatedDataNotice();
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

    connect(w, &WorkoutDialog::ftp_lthr_changed, ui->tab_workout1, &Main_WorkoutPage::updateTableViewMetrics);
    connect(w, &WorkoutDialog::ftpUserStudioChanged, this, &MainWindow::updateVecStudio);

    connect(w, &QDialog::finished, this, [this, btleHub]() {
        workoutOver();
        btleHub->disconnectFromDevice();
        delete btleHub;
    });

    attachPendingGroupSession(w);
    workoutExecuting();
    QApplication::restoreOverrideCursor();
    w->show();
    m_launchingWorkout = false;
}


#ifndef GC_WASM_BUILD
void MainWindow::startWorkoutConnectingSensors(const Workout &workout,
                                               const QMap<BtleSensorRole, BtleSavedSensor> &savedSensors)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Show WorkoutDialog NON-MODALLY (window-modal, run on the main event loop)
    // for the same QWebEngine reparenting reason as the single-device path above.
    WorkoutDialog *w = new WorkoutDialog(workout, lstRadio, vecUserStudio);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowModality(Qt::ApplicationModal);

    auto *connector = new SensorConnector(savedSensors, account->rowing_mode, this);
    SensorStatusStrip *strip = w->sensorStrip();
    for (const SensorConnector::Slot &slot : connector->sensorSlots())
        strip->addSensor(int(slot.role), BtleSensorStore::roleDisplayName(slot.role), slot.saved.name);
    connect(connector, &SensorConnector::statusChanged, strip,
            [strip](BtleSensorRole role, SensorConnector::Status status) {
                strip->setState(int(role), SensorStatusStrip::State(int(status)));
            });
    connect(strip, &SensorStatusStrip::retryRequested, connector,
            [connector](int key) { connector->retry(BtleSensorRole(key)); });

    // Re-wire everything that is connected whenever a hub (re)connects, so the
    // source priorities still hold: a dedicated HR strap or cadence sensor that
    // comes up after the trainer takes over from the trainer's own reading.
    connect(connector, &SensorConnector::hubConnected, w, [this, w, connector](BtleHub *connectedHub) {
        for (BtleHub *hub : connector->hubs()) {
            disconnect(hub, nullptr, w, nullptr);
            disconnect(w, nullptr, hub, nullptr);
            disconnect(hub, &BtleHub::signal_trainerProvidesHr, this, nullptr);
        }
        const QMap<BtleSensorRole, BtleHub*> connected = connector->connectedHubs();
        QSet<BtleHub*> wiredHubs;
        wireHubsToDialog(w, connected, wiredHubs);
        const BtleSensorRole ftmsRole = account->rowing_mode ? BtleSensorRole::Rower : BtleSensorRole::Trainer;
        if (connected.value(ftmsRole, nullptr) == connectedHub)
            w->trainerConnected();
    });

    if (account->use_zwift_click) {
        constexpr int kZwiftClickKey = 1000;
        auto *click = new ZwiftClickManager();
        strip->addSensor(kZwiftClickKey, tr("Zwift Click"));
        connect(click, &ZwiftClickManager::connectionChanged, strip, [strip, click]() {
            strip->setState(kZwiftClickKey, click->rightConnected() ? SensorStatusStrip::State::Connected
                                                                    : SensorStatusStrip::State::Searching);
        });
        w->adoptZwiftClickManager(click);
        click->start();
    }

    connect(w, &WorkoutDialog::ftp_lthr_changed, ui->tab_workout1, &Main_WorkoutPage::updateTableViewMetrics);
    connect(w, &WorkoutDialog::ftpUserStudioChanged, this, &MainWindow::updateVecStudio);
    connect(w, &QDialog::finished, this, [this, connector]() {
        workoutOver();
        delete connector;   // disconnects and frees every hub
    });

    attachPendingGroupSession(w);
    workoutExecuting();
    QApplication::restoreOverrideCursor();
    w->show();
    connector->start();
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

void MainWindow::startStudioWorkoutConnectingSensors(const Workout &workout)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    const QVector<UserStudio> riders = prepareStudioRiders(false);
    WorkoutDialog *w = new WorkoutDialog(workout, lstRadio, riders);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowModality(Qt::ApplicationModal);

    QList<SensorConnector*> connectors;
    StudioConnectionBoard *board = nullptr;
    const int nbRiders = qBound(1, account->nb_user_studio, constants::nbMaxUserStudio);
    for (int rider = 1; rider <= nbRiders; ++rider) {
        auto *connector = new SensorConnector(BtleSensorStore::loadAll(rider), account->rowing_mode, this);
        if (connector->isEmpty()) {
            delete connector;   // no saved sensors: the rider's card stays empty
            continue;
        }
        connector->setHubUserId(rider);
        connectors.append(connector);
        if (!board)
            board = w->studioConnectionBoard();

        const QString riderName = (rider - 1 < riders.size() && !riders.at(rider - 1).getDisplayName().trimmed().isEmpty())
                                      ? riders.at(rider - 1).getDisplayName().trimmed()
                                      : tr("Rider%1").arg(rider);
        board->addRider(rider, riderName);
        UserStudioWidget *card = w->riderWidget(rider);
        for (const SensorConnector::Slot &slot : connector->sensorSlots()) {
            const QString role = BtleSensorStore::roleDisplayName(slot.role);
            board->addSensor(rider, int(slot.role), role, slot.saved.name);
            if (card)
                card->addSensor(int(slot.role), role);
        }
        connect(connector, &SensorConnector::statusChanged, w,
                [board, card, rider](BtleSensorRole role, SensorConnector::Status status) {
                    const auto state = SensorStatusStrip::State(int(status));
                    board->setState(rider, int(role), state);
                    if (card)
                        card->setSensorState(int(role), state);
                });
        connect(board, &StudioConnectionBoard::retryRequested, connector,
                [connector, rider](int boardRider, int key) {
                    if (boardRider == rider)
                        connector->retry(BtleSensorRole(key));
                });
        if (card)
            connect(card, &UserStudioWidget::sensorRetryRequested, connector,
                    [connector](int key) { connector->retry(BtleSensorRole(key)); });

        // Same re-wiring as solo, per rider: everything this rider has connected.
        connect(connector, &SensorConnector::hubConnected, w, [this, w, connector, rider](BtleHub *connectedHub) {
            for (BtleHub *hub : connector->hubs()) {
                disconnect(hub, nullptr, w, nullptr);
                disconnect(w, nullptr, hub, nullptr);
                disconnect(hub, &BtleHub::signal_trainerProvidesHr, this, nullptr);
            }
            const QMap<BtleSensorRole, BtleHub*> connected = connector->connectedHubs();
            QSet<BtleHub*> wiredHubs;
            wireHubsToDialog(w, connected, wiredHubs, rider);
            const BtleSensorRole ftmsRole = account->rowing_mode ? BtleSensorRole::Rower : BtleSensorRole::Trainer;
            if (connected.value(ftmsRole, nullptr) == connectedHub)
                w->trainerConnected();
        });
    }

    connect(w, &WorkoutDialog::ftp_lthr_changed, ui->tab_workout1, &Main_WorkoutPage::updateTableViewMetrics);
    connect(w, &WorkoutDialog::ftpUserStudioChanged, this, &MainWindow::updateVecStudio);
    connect(w, &QDialog::finished, this, [this, connectors]() {
        workoutOver();
        qDeleteAll(connectors);   // disconnects and frees every hub
    });

    attachPendingGroupSession(w);
    workoutExecuting();
    QApplication::restoreOverrideCursor();
    w->show();
    for (SensorConnector *connector : connectors)
        connector->start();
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
