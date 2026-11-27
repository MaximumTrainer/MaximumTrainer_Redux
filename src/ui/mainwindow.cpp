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
#include <QLabel>
#include <QHBoxLayout>
#include <QMenu>
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
#include "settingspage.h"
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
#include "device_session.h"
#include "sensor_connector.h"
#include "sensorstatusstrip.h"
#include "zwift_click_manager.h"
#endif
#include "sensorswidget.h"
#include "asyncdialogs.h"
#include "groupconfig.h"
#include "groupsession.h"
#include "groupworkoutcodec.h"
#include "joingroupdialog.h"
#include "coursespage.h"
#include "sidebaricons.h"

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
    window.mt_triggerLogout = function() {
        Module._mt_trigger_logout_impl();
    };
    window.mt_openPreferences = function() {
        Module._mt_open_preferences_impl();
    };
});
#endif // GC_WASM_BUILD


namespace {
QIcon workoutTabIcon(bool rowing)
{
    return rowing ? QIcon(QStringLiteral(":/image/icon/rower")) : SidebarIcons::workout();
}
}

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

    // Stacked pages: Workout 0, Intervals 1, History 2, Sensors 3,
    // Achievements 4, Courses 5 and Settings 6 (added below).
    int nextTab = 0;
    auto addTab = [&](int &slot, int page, const QIcon &icon, const QString &label) {
        slot = nextTab++;
        ftb->insertTab(slot, icon, label);
        m_tabToPage.append(page);
    };
    addTab(m_tabs.workout, 0, workoutTabIcon(account->rowing_mode), tr("Workout"));
    addTab(m_tabs.courses, 5, CoursesPage::tabIcon(), tr("Courses"));
    addTab(m_tabs.intervals, 1, SidebarIcons::intervals(), tr("Intervals.icu"));
    addTab(m_tabs.history, 2, SidebarIcons::stats(), tr("Stats"));
    addTab(m_tabs.achievements, 4, QIcon(":/image/icon/trophy"), tr("Achievements"));
    addTab(m_tabs.devices, 3, SidebarIcons::devices(), tr("Devices"));
    addTab(m_tabs.settings, 6, SidebarIcons::settings(), tr("Settings"));
    ftb->setGroupTitle(m_tabs.workout, tr("RIDE"));
    ftb->setGroupTitle(m_tabs.intervals, tr("TRAINING"));
    ftb->setGroupTitle(m_tabs.devices, tr("SETUP"));
    for (int tab = 0; tab < nextTab; ++tab)
        ftb->setTabEnabled(tab, true);


    ftb->setCurrentIndex(0);
    connect(ftb, &FancyTabBar::currentChanged, this, &MainWindow::leftMenuChanged);


    ui->tabWidget_workout->tabBar()->setObjectName("tabBarWorkout");
    setStyleSheet(qApp->styleSheet());

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    // Track OS colour-scheme changes for System theme mode.
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            &MainWindow::slotSystemThemeChanged);
#endif



    ManagerAchievement *achievementManager = new ManagerAchievement(this);
    qApp->setProperty("ManagerAchievement", QVariant::fromValue<ManagerAchievement*>(achievementManager));
    // Catch up on achievements the ride history already earned, without popups.
    QTimer::singleShot(0, achievementManager, [achievementManager]() { achievementManager->evaluateHistory(false); });
    m_achievementsPage = new AchievementsWidget(ui->stackedWidget_menu);
    ui->stackedWidget_menu->addWidget(m_achievementsPage);   // page 4, after the .ui pages
    m_coursesPage = new CoursesPage(ui->stackedWidget_menu);
    m_coursesPage->setProfile(account->FTP, account->weight_kg, account->bike_weight_kg, account->userCda,
                                  !account->intervals_icu_access_token.isEmpty());
    ui->stackedWidget_menu->addWidget(m_coursesPage);     // page 5
    connect(m_coursesPage, &CoursesPage::rideRequested, this, &MainWindow::rideCourse);
    connect(m_coursesPage, &CoursesPage::weightEdited, this, [this](double weightKg) {
        account->saveProfileFields(account->FTP, account->LTHR, weightKg);
        m_coursesPage->setProfile(account->FTP, account->weight_kg, account->bike_weight_kg, account->userCda, false);
    });


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
    m_hostGroupAction = ui->menuTEst->addAction(tr("Host Group Workout…"));
    connect(m_hostGroupAction, &QAction::triggered, this, [this] {
        const std::optional<Workout> selected = ui->tab_workout1->selectedWorkout();
        if (!selected) {
            AsyncDialogs::information(this, tr("Host Group Workout"),
                tr("Select a workout in the Workout list first, then choose Host Group Workout."));
            return;
        }
        hostGroupWorkout(*selected);
    });
    m_joinGroupAction = ui->menuTEst->addAction(tr("Join Group Workout…"));
    m_joinGroupAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_J));
    connect(m_joinGroupAction, &QAction::triggered, this, &MainWindow::showJoinGroupDialog);
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


    // Settings tab
    dconfig = new SettingsPage(ui->stackedWidget_menu);
    ui->stackedWidget_menu->addWidget(dconfig);   // page 6
    connect(dconfig, &SettingsPage::logoutRequested, this, &MainWindow::on_actionLogout_triggered);
    connect(dconfig, &SettingsPage::keyboardShortcutsRequested, this, &MainWindow::on_actionKeyboard_Shortcuts_triggered);
    connect(dconfig, &SettingsPage::userGuideRequested, this, &MainWindow::on_actionRequest_Help_triggered);
    connect(dconfig, &SettingsPage::aboutRequested, this, &MainWindow::on_actionAbout_MT_triggered);
    connect(dconfig, &SettingsPage::ridesFolderRequested, this, &MainWindow::on_actionHistory_triggered);
    connect(dconfig, &SettingsPage::workoutsFolderRequested, this, &MainWindow::on_actionWorkout_triggered);
    setupPageActions();
    // Screenshot mode feeds the docs: no "Update available" banner in them.
    if (!QCoreApplication::arguments().contains(QLatin1String("--screenshots"), Qt::CaseInsensitive)) {
        auto *updateNotifier = new UpdateNotifier(this);
        connect(updateNotifier, &UpdateNotifier::updateAvailable, this, &MainWindow::showUpdateAvailable);
        updateNotifier->check();
    }

    // Athlete profile (FTP/LTHR/weight) edited in Preferences → recompute zones
    // and workout targets, same as the old profile page's FTP-change path.
    connect(dconfig, &SettingsPage::profileChanged,
            this, &MainWindow::ftpAndTabProfileChanged);
    // Rowing mode (Beta) applies live: swap the Workout tab icon and the
    // Devices tab's FTMS slot (Trainer ↔ Rower). Workout sessions read the
    // mode when they start, so nothing else needs refreshing.
    connect(dconfig, &SettingsPage::rowingModeChanged, this, [this]() {
        ftb->setTabIcon(0, workoutTabIcon(account->rowing_mode));
        if (auto *sw = qobject_cast<SensorsWidget*>(ui->sensorsWidget))
            sw->refreshRowingMode();
    });


    leftMenuChanged(0);


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
            dconfig, &SettingsPage::setOnlineMode);

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

}


void MainWindow::leftMenuChanged(int tabSelected) {


    if (tabSelected < 0 || tabSelected >= m_tabToPage.size())
        return;
    ui->stackedWidget_menu->setCurrentIndex(m_tabToPage.at(tabSelected));
    currentIndexLeftMenu = tabSelected;
#ifndef GC_WASM_BUILD
    // Settings may have changed on the page just left (paired sensors, Zwift
    // Click, simulated sensors); reconnects only when they did.
    if (m_devices && !isInsideWorkout)
        m_devices->reconfigure();
#endif

    // Refresh saved sensors each time the Devices tab is opened.
    if (tabSelected == m_tabs.devices) {
        if (auto *sw = qobject_cast<SensorsWidget*>(ui->sensorsWidget))
            sw->reload();
    }
    if (tabSelected == m_tabs.achievements && m_achievementsPage)
        m_achievementsPage->reload();
    if (tabSelected == m_tabs.courses && m_coursesPage)
        m_coursesPage->setProfile(account->FTP, account->weight_kg, account->bike_weight_kg, account->userCda,
                                  !account->intervals_icu_access_token.isEmpty());
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


void MainWindow::startDevices()
{
#ifndef GC_WASM_BUILD
    if (m_devices)
        return;
    m_devices = new DeviceSession(account, this);

    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("deviceBar"));
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);
    m_deviceStrip = new SensorStatusStrip(bar);
    m_deviceStrip->setAutoHide(false);
    m_deviceStrip->layout()->setContentsMargins(4, 2, 4, 2);
    m_deviceNote = new QLabel(bar);
    m_deviceNote->setObjectName(QStringLiteral("deviceBarNote"));
    row->addWidget(m_deviceStrip);
    row->addWidget(m_deviceNote);
    bar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(bar, &QWidget::customContextMenuRequested, this,
            [this, bar](const QPoint &pos) { showDeviceMenu(-1, bar->mapToGlobal(pos)); });
    connect(m_deviceStrip, &SensorStatusStrip::chipMenuRequested, this, &MainWindow::showDeviceMenu);
    ui->widget_bottomMenu->setStatusWidget(bar);

    connect(m_devices, &DeviceSession::itemsChanged, this, &MainWindow::rebuildDeviceBar);
    connect(m_devices, &DeviceSession::modeChanged, this, &MainWindow::rebuildDeviceBar);
    connect(m_devices, &DeviceSession::statusChanged, m_deviceStrip,
            [this](int key, SensorConnector::Status status) {
                m_deviceStrip->setState(key, SensorStatusStrip::State(int(status)));
            });
    connect(m_deviceStrip, &SensorStatusStrip::retryRequested, m_devices, &DeviceSession::retry);
    if (auto *sensors = qobject_cast<SensorsWidget*>(ui->sensorsWidget)) {
        // A connected device stops advertising, so the pairing scan could not see it.
        connect(sensors, &SensorsWidget::pairingStarted, m_devices, [this]() { m_devices->setPaused(true); });
        connect(sensors, &SensorsWidget::pairingFinished, m_devices, [this]() { m_devices->setPaused(false); });
        connect(sensors, &SensorsWidget::devicesChanged, m_devices, &DeviceSession::reconfigure);
    }
    m_devices->reconfigure();
    rebuildDeviceBar();
#endif
}

#ifndef GC_WASM_BUILD
void MainWindow::rebuildDeviceBar()
{
    m_deviceStrip->clear();
    for (const DeviceSession::Item &item : m_devices->items()) {
        m_deviceStrip->addSensor(item.key, item.name, item.device);
        m_deviceStrip->setState(item.key, SensorStatusStrip::State(int(item.status)));
    }
    QString note;
    switch (m_devices->mode()) {
    case DeviceSession::Mode::Live:          break;
    case DeviceSession::Mode::Simulated:     note = tr("Simulated sensors"); break;
    case DeviceSession::Mode::NothingPaired: note = tr("No devices paired - pair them on the Devices page"); break;
    case DeviceSession::Mode::Paused:        note = tr("Devices paused while pairing"); break;
    }
    m_deviceNote->setText(note);
    m_deviceNote->setVisible(!note.isEmpty());
    m_deviceStrip->setVisible(!m_devices->items().isEmpty());
}

void MainWindow::showDeviceMenu(int key, const QPoint &globalPos)
{
    QMenu menu(this);
    for (const DeviceSession::Item &item : m_devices->items()) {
        if (item.key != key)
            continue;
        const QString device = item.device.isEmpty() ? item.name : item.device;
        if (item.status == SensorConnector::Status::Disconnected)
            menu.addAction(tr("Reconnect %1").arg(device), this, [this, key]() { m_devices->retry(key); });
        else
            menu.addAction(tr("Disconnect %1").arg(device), this, [this, key]() { m_devices->disconnectDevice(key); });
        menu.addSeparator();
    }
    menu.addAction(tr("Devices page..."), this, [this]() { ftb->setCurrentIndex(m_tabs.devices); });
    menu.exec(globalPos);
}
#endif

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
    ftb->setCurrentIndex(m_tabs.settings);
}
void MainWindow::showUpdateAvailable(const QString &latestVersion)
{
    const QString label = tr("Update available: %1").arg(latestVersion);
    auto *download = new QAction(label, this);
    download->setToolTip(tr("Open the download page for MaximumTrainer %1").arg(latestVersion));
    connect(download, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl(urlGitHubReleasesPage));
    });

    // Also in the Help menu (macOS keeps its menu bar).
    ui->menu->insertAction(ui->menu->actions().value(0), download);
    ui->menu->insertSeparator(ui->menu->actions().value(1));

    auto *button = new QToolButton(ui->centralWidget);
    button->setDefaultAction(download);
    button->setCursor(Qt::PointingHandCursor);
    QFont font = button->font();
    font.setBold(true);
    button->setFont(font);
    QPalette palette = button->palette();
    palette.setColor(QPalette::ButtonText, palette.color(QPalette::Highlight));
    button->setPalette(palette);
    ui->gridLayout->addWidget(button, 0, 1, Qt::AlignRight);
    button->show();
}
void MainWindow::setupPageActions()
{
    // Without a menu bar an action's shortcut only works if a visible widget
    // holds the action.
    addActions({ui->actionPreferences, ui->actionKeyboard_Shortcuts, ui->actionCreate_New, ui->actionExit});
    ui->actionPreferences->setText(tr("Settings"));
    ui->actionPreferences->setMenuRole(QAction::PreferencesRole);
    ui->actionAbout_MT->setMenuRole(QAction::AboutRole);
    ui->actionExit->setMenuRole(QAction::QuitRole);
#ifndef Q_OS_MAC
    ui->menuBar->setVisible(false);
#endif

    // Workout page: what used to be the Workout menu, beside the List/Editor tabs.
    auto *bar = new QWidget(ui->tabWidget_workout);
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 2, 8, 2);
    row->setSpacing(6);
    const auto addButton = [&](const QString &label, const QString &toolTip) {
        auto *button = new QToolButton(bar);
        button->setText(label);
        button->setToolTip(toolTip);
        row->addWidget(button);
        return button;
    };
    connect(addButton(tr("New"), tr("Create a workout (Ctrl+N)")), &QToolButton::clicked,
            this, &MainWindow::on_actionCreate_New_triggered);
#ifndef GC_WASM_BUILD
    auto *import = addButton(tr("Import"), tr("Import .erg, .mrc or .zwo workouts"));
    import->setMenu(ui->menuImport);
    import->setPopupMode(QToolButton::InstantPopup);
    auto *group = addButton(tr("Group"), tr("Ride a workout with friends over the internet"));
    auto *groupMenu = new QMenu(group);
    groupMenu->addActions({m_hostGroupAction, m_joinGroupAction});
    group->setMenu(groupMenu);
    group->setPopupMode(QToolButton::InstantPopup);
    addAction(m_joinGroupAction);
#endif
    connect(addButton(tr("Free Ride"), tr("Ride without a workout")), &QToolButton::clicked,
            this, &MainWindow::on_actionOpen_Ride_triggered);
    ui->tabWidget_workout->setCornerWidget(bar, Qt::TopRightCorner);
    bar->show();   // the window is already on screen here
#ifndef GC_WASM_BUILD
    connect(ui->historyWidget, &HistoryWidget::openFolderRequested, this, &MainWindow::on_actionHistory_triggered);
#endif
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
    dconfig->stopRadio();
    lstRadio = Util::loadLocalRadioList();   // stations may have changed in Settings or the last ride

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
    if (!m_devices)
        startDevices();
    m_devices->reconfigure();   // e.g. sensors re-paired since the last check

    // Saved sensors connect inside the workout window. With none paired, point
    // the rider to the Devices page (or ride on without sensors).
    const QMap<BtleSensorRole, BtleSavedSensor> savedSensors = BtleSensorStore::loadAll();
    if (SensorConnector::slotsFor(savedSensors, account->rowing_mode).isEmpty() && !account->use_zwift_click) {
        QMessageBox box(QMessageBox::Information, tr("No sensors paired"),
                        tr("Pair your trainer and sensors on the Devices page once; after that they "
                           "connect by themselves when the app starts."),
                        QMessageBox::NoButton, this);
        box.setInformativeText(tr("No hardware? Turn on Simulated Sensors at the bottom of the Devices page."));
        QPushButton *goToDevices = box.addButton(tr("Go to Devices"), QMessageBox::AcceptRole);
        box.addButton(tr("Continue without sensors"), QMessageBox::RejectRole);
        box.setDefaultButton(goToDevices);
        box.exec();
        if (box.clickedButton() == goToDevices) {
            ftb->setCurrentIndex(m_tabs.devices);
            return;
        }
    }
    startWorkoutOnLiveDevices(workout);
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
    // Show WorkoutDialog NON-MODALLY (window-modal via setWindowModality,
    // run on the main event loop instead of QDialog::exec()). The embedded
    // QWebEngine video player reparents widgets when it initialises, which
    // destroys and recreates the dialog's native window; inside a nested
    // exec() loop that window-destroy calls QEventLoop::exit() and tears the
    // dialog down. Running on the main loop removes that hazard. Post-workout
    // cleanup moves to the finished() handler below.
    WorkoutDialog *w = new WorkoutDialog(workout, lstRadio);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowModality(Qt::ApplicationModal);

    auto *simHub = new SimulatorHub(this);
    simHub->setRowingMode(account->rowing_mode);
    connectSimulatorHub(simHub, w);
    // Simulated rides always dual record, so the 2nd power meter UI can be tried without hardware.
    if (!account->rowing_mode) {
        w->setDualRecording(tr("Simulated trainer"), tr("Simulated power meter"));
        simHub->setSecondaryPowerEnabled(true);
        connect(simHub, &SimulatorHub::signal_secondaryPower, w, &WorkoutDialog::SecondaryPowerDataReceived);
    }
    connect(w, &WorkoutDialog::setLoad, simHub, &SimulatorHub::setLoad);
    connect(w, &WorkoutDialog::setSlope, simHub, &SimulatorHub::setSlope);
    connect(w, &WorkoutDialog::stopDecodingMsgHub, simHub, &SimulatorHub::stopDecodingMsg);
    w->enableTrainerControl();

    connect(w, &WorkoutDialog::ftp_lthr_changed, ui->tab_workout1, &Main_WorkoutPage::updateTableViewMetrics);
    
    connect(w, &QDialog::finished, this, [this, simHub]() {
        workoutOver();
        simHub->stopDecodingMsg();
        delete simHub;
    });

    simHub->start();
    attachPendingGroupSession(w);
    workoutExecuting();
    w->startZwiftClick();
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
    WorkoutDialog *w = new WorkoutDialog(workout, lstRadio);
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
    
    connect(w, &QDialog::finished, this, [this, btleHub]() {
        workoutOver();
        btleHub->disconnectFromDevice();
        delete btleHub;
    });

    attachPendingGroupSession(w);
    workoutExecuting();
    w->startZwiftClick();
    QApplication::restoreOverrideCursor();
    w->show();
    m_launchingWorkout = false;
}


#ifndef GC_WASM_BUILD
void MainWindow::startWorkoutOnLiveDevices(const Workout &workout)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Show WorkoutDialog NON-MODALLY (window-modal, run on the main event loop)
    // for the same QWebEngine reparenting reason as the single-device path above.
    WorkoutDialog *w = new WorkoutDialog(workout, lstRadio);
    w->setAttribute(Qt::WA_DeleteOnClose);
    w->setWindowModality(Qt::ApplicationModal);

    const QVector<DeviceSession::Item> items = m_devices->items();
    if (!items.isEmpty()) {
        SensorStatusStrip *strip = w->sensorStrip();
        for (const DeviceSession::Item &item : items) {
            strip->addSensor(item.key, item.name, item.device);
            strip->setState(item.key, SensorStatusStrip::State(int(item.status)));
        }
        connect(m_devices, &DeviceSession::statusChanged, strip,
                [strip](int key, SensorConnector::Status status) {
                    strip->setState(key, SensorStatusStrip::State(int(status)));
                });
        connect(strip, &SensorStatusStrip::retryRequested, m_devices, &DeviceSession::retry);
    }

    // Re-wire everything that is connected whenever a hub (re)connects, so the
    // source priorities still hold: a dedicated HR strap or cadence sensor that
    // comes up after the trainer takes over from the trainer's own reading.
    const auto rewire = [this, w]() {
        if (!m_devices->connector())
            return;
        for (BtleHub *hub : m_devices->connector()->hubs()) {
            disconnect(hub, nullptr, w, nullptr);
            disconnect(w, nullptr, hub, nullptr);
            disconnect(hub, &BtleHub::signal_trainerProvidesHr, this, nullptr);
        }
        QSet<BtleHub*> wiredHubs;
        wireHubsToDialog(w, m_devices->connectedHubs(), wiredHubs, false);
    };
    rewire();
    if (m_devices->trainerHub())
        w->trainerConnected();
    connect(m_devices, &DeviceSession::hubConnected, w, [this, w, rewire](BtleHub *connectedHub) {
        rewire();
        if (connectedHub == m_devices->trainerHub())
            w->trainerConnected();
    });

    // Dual recording: a saved 2nd power meter gets its own .fit file, and both
    // files are named after the device that measured their power.
    const QMap<BtleSensorRole, BtleSavedSensor> saved = BtleSensorStore::loadAll();
    const BtleSavedSensor secondary = saved.value(BtleSensorRole::SecondaryPower);
    if (secondary.isValid() && secondary.enabled && !account->rowing_mode) {
        const BtleSavedSensor trainer = saved.value(BtleSensorRole::Trainer);
        const BtleSavedSensor power = saved.value(BtleSensorRole::Power);
        const bool trainerMeasures = trainer.isValid() && trainer.enabled;
        w->setDualRecording(trainerMeasures ? trainer.name : power.name, secondary.name);
    }

    if (ZwiftClickManager *click = m_devices->clickManager())
        w->useZwiftClickManager(click);
    m_devices->setRideActive(true);

    connect(w, &WorkoutDialog::ftp_lthr_changed, ui->tab_workout1, &Main_WorkoutPage::updateTableViewMetrics);
        connect(w, &QDialog::finished, this, [this]() {
        workoutOver();
        m_devices->setRideActive(false);
        m_devices->returnTrainerToFreeRide();
    });

    attachPendingGroupSession(w);
    workoutExecuting();
    QApplication::restoreOverrideCursor();
    w->show();
}

// Wire the connected hubs (keyed by role) to the dialog: shared
// battery/error signals once per distinct hub, then per-role data signals with
// local dedup so a trainer that also reports power/cadence/speed is not
// double-counted. Distinct hubs are added to \a allHubs for lifecycle cleanup.
void MainWindow::wireHubsToDialog(WorkoutDialog *w,
                                  const QMap<BtleSensorRole, BtleHub*> &hubsByRoleIn,
                                  QSet<BtleHub*> &allHubs,
                                  bool releaseOnClose)
{
    // A rower is FTMS like a trainer — alias it onto the Trainer wiring path
    // (power/stroke-rate/pace channels, resistance control, HR bridge) so the
    // rest of this function needs no rowing-specific branches.
    QMap<BtleSensorRole, BtleHub*> hubsByRole = hubsByRoleIn;
    if (hubsByRole.contains(BtleSensorRole::Rower) && !hubsByRole.contains(BtleSensorRole::Trainer))
        hubsByRole.insert(BtleSensorRole::Trainer, hubsByRole.value(BtleSensorRole::Rower));
    // The 2nd power meter only feeds the dual recording: it must not drive the
    // ride's power, nor pause the ride when it drops.
    BtleHub *secondaryPowerHub = hubsByRole.take(BtleSensorRole::SecondaryPower);

    QSet<BtleHub*> uniqueHubs;
    for (BtleHub *hub : hubsByRole)
        uniqueHubs.insert(hub);

    for (BtleHub *hub : uniqueHubs) {
        allHubs.insert(hub);
        connect(hub, &BtleHub::signal_battery,  w, &WorkoutDialog::batteryStatusReceived);
        connect(hub, &BtleHub::connectionError, w, &WorkoutDialog::onBleConnectionError);
        if (releaseOnClose)
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
                []() { BtleSensorStore::setTrainerProvidesHr(true); });
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
    if (secondaryPowerHub) {
        allHubs.insert(secondaryPowerHub);
        connect(secondaryPowerHub, &BtleHub::signal_power, w, &WorkoutDialog::SecondaryPowerDataReceived);
    }
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
    // Gray out the Intervals.icu tab when offline so users see it is
    // temporarily unavailable rather than it simply vanishing.
    ftb->setTabEnabled(m_tabs.intervals, isOnline);

    // If the user is currently on the Intervals.icu tab and we just went
    // offline, navigate back to the Workout tab so they land on something
    // functional.
    if (!isOnline && currentIndexLeftMenu == m_tabs.intervals) {
        ftb->setCurrentIndex(m_tabs.workout);
        leftMenuChanged(m_tabs.workout);
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

void MainWindow::rideCourse(const RaceCourse &course, const RaceBot::FieldSettings &bots) {
    if (isInsideWorkout || m_launchingWorkout) return;
    QList<Interval> lstInterval;
    lstInterval.append(Interval());
    Workout ride("null", Workout::OPEN_RIDE, lstInterval, course.name(), "MaximumTrainer",
                 course.place(), "-", Workout::T_OTHERS);
    m_pendingRaceCourse = course;
    m_pendingRaceBots = bots;
    executeWorkout(ride);
    m_pendingRaceCourse.reset();   // a cancelled launch never attached it
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
    if (m_pendingRaceCourse) {
        dialog->setRace(*m_pendingRaceCourse, m_pendingRaceBots);
        m_pendingRaceCourse.reset();
    }
    if (!m_pendingGroupSession) return;
    GroupSession *session = m_pendingGroupSession;
    m_pendingGroupSession = nullptr;
    dialog->setGroupSession(session);
    connect(dialog, &QDialog::finished, session, [session] {
        session->leave();
        session->deleteLater();
    });
}
