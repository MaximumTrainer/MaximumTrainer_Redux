// MainWindow's demo and test tooling: the --screenshots tour, the
// --sensor-check BLE validation run and the demo workout view. Kept out
// of mainwindow.cpp so product code and tooling don't interleave.
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
#include <QElapsedTimer>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QGuiApplication>
#include <QStyleHints>
#include <QScopeGuard>
#include <functional>
#include <QLabel>

#include "util.h"
#ifndef GC_WASM_BUILD
#include "device_session.h"
#include "btle_hub.h"
#endif
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
#include <QKeyEvent>
#include "networkmonitor.h"
#include "dialogkeyboardshortcuts.h"
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
#endif
#include "sensorswidget.h"
#include "asyncdialogs.h"
#include "groupconfig.h"
#include "groupsession.h"
#include "groupworkoutcodec.h"
#include "joingroupdialog.h"

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
    0,     // after step  5 (closed WorkoutDialog)      → (unused)
    0,     // after step  6 (unused)
    500,   // after step  7 (unused)                    → switch to Intervals.icu
    1500,  // after step  8 (switched to Intervals.icu) → capture Intervals.icu
    0,     // after step  9 (captured Intervals.icu)    → (unused)
    0,     // after step 10 (unused)
    500,   // after step 11 (unused)                    → switch to History
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

void MainWindow::launchDemoWorkout()
{
    ftb->setCurrentIndex(0);
    ui->tabWidget_workout->setCurrentIndex(0);

    m_ssSimHub = new SimulatorHub(this);
    m_ssSimHub->setRowingMode(account->rowing_mode); // demo follows the app mode
    m_ssWorkoutDlg = new WorkoutDialog(makeDemoWorkout(), lstRadio, this);

    connectSimulatorHub(m_ssSimHub, m_ssWorkoutDlg);

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
    // so the metric band is unobstructed.
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

    QTimer::singleShot(3000, this, &MainWindow::sensorCheckNextStep);
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
        grab(QStringLiteral("balance_only"));  // what a Bluetooth power meter provides
        m_scReport << QStringLiteral("BALANCE  : Right 53%% -> sensor_balance_only.png (decoded from CPM 0x2A63)");
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
    QTimer::singleShot(900, this, &MainWindow::sensorCheckNextStep);
}


void MainWindow::startScreenshotMode(const QString &outputDir)
{
    qInfo() << "Screenshot mode: output dir =" << outputDir;
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
    QTimer::singleShot(2000, this, &MainWindow::screenshotNextStep);
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
        qInfo() << "Screenshot: main_window";
        ftb->setCurrentIndex(m_tabs.settings);
        for (const auto &[section, name] : std::initializer_list<std::pair<SettingsPage::Section, const char *>>{
                 {SettingsPage::Section::Profile, "settings_profile"},
                 {SettingsPage::Section::Accounts, "settings_accounts"},
                 {SettingsPage::Section::Ride, "settings_ride"},
                 {SettingsPage::Section::RideScreen, "settings"},
                 {SettingsPage::Section::Sounds, "settings_sounds"},
                 {SettingsPage::Section::Media, "settings_media"},
                 {SettingsPage::Section::General, "settings_general"},
                 {SettingsPage::Section::Logs, "settings_logs"},
                 {SettingsPage::Section::Help, "settings_help"}}) {
            dconfig->showSection(section);
            QCoreApplication::processEvents();
            grab().save(m_ssOutputDir + QStringLiteral("/screenshot_%1.png").arg(QLatin1String(name)), "PNG");
        }
        dconfig->showSection(SettingsPage::Section::Profile);
        qInfo() << "Screenshot: settings";
        ftb->setCurrentIndex(m_tabs.courses);
        QCoreApplication::processEvents();
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_courses.png"), "PNG");
        qInfo() << "Screenshot: courses";
        // The Devices page hosts sensor pairing plus the trainer/sensor settings.
        ftb->setCurrentIndex(m_tabs.devices);
        QCoreApplication::processEvents();
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_sensors.png"), "PNG");
        qInfo() << "Screenshot: sensors";
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
        qInfo() << "Screenshot: workout_editor";
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
            qInfo() << "Screenshot: workout_running";
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

    // Steps 6-7 used to capture the removed Studio page.
    case 6:
    case 7:
        break;

    // ── Step 8: switch to Intervals.icu tab (tab 1) ─────────────────────
    case 8:
        ftb->setCurrentIndex(m_tabs.intervals);
        raise();
        activateWindow();
        QCoreApplication::processEvents();
        break;

    // ── Step 9: capture Intervals.icu ────────────────────────────────────
    case 9:
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_activity_history.png"), "PNG");
        qInfo() << "Screenshot: activity_history (Intervals.icu tab)";
        break;

    // Steps 10-11 used to capture the removed Plan page.
    case 10:
    case 11:
        break;

    // ── Step 12: switch to History tab (tab 4) ────────────────────────────
    case 12:
        ftb->setCurrentIndex(m_tabs.history);
        raise();
        activateWindow();
        QCoreApplication::processEvents();
        break;

    // ── Step 13: capture History ─────────────────────────────────────────
    case 13:
        grab().save(m_ssOutputDir + QLatin1String("/screenshot_history.png"), "PNG");
        qInfo() << "Screenshot: history";
        break;

    default:
        scheduleScreenshotQuit();
        return;
    }

    // Schedule the next step after its delay.
    const int numDelays = static_cast<int>(sizeof(ssDelays) / sizeof(ssDelays[0]));
    if (step < numDelays)
        QTimer::singleShot(ssDelays[step], this, &MainWindow::screenshotNextStep);
    else
        scheduleScreenshotQuit();
}

// ─────────────────────────────────────────────────────────────────────────────
// Workout benchmark (--bench-workout [seconds])
//
// Runs the simulated demo workout and reports the CPU the whole process used
// while it ran, as a repeatable baseline for performance work. The first few
// seconds (dialog construction, first layout) are excluded.
// ─────────────────────────────────────────────────────────────────────────────
#if !defined(Q_OS_WIN) && !defined(Q_OS_WASM)
#include <sys/resource.h>

static double processCpuSeconds()
{
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6
         + usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
}

void MainWindow::runWorkoutBenchmark(int seconds)
{
    // Phase 1: the workout screen open but not started (what a rider sees
    // before pedalling). Phase 2: the workout running.
    struct Window { double cpu; qint64 wallMs; };
    auto measure = [](const Window &start, const QElapsedTimer &wall, const char *label) {
        const double cpu = processCpuSeconds() - start.cpu;
        const double elapsed = (wall.elapsed() - start.wallMs) / 1000.0;
        const QString line = QStringLiteral("BENCH %1: %2 s wall, %3 s CPU, %4 % of one core")
                                 .arg(QLatin1String(label)).arg(elapsed, 0, 'f', 1).arg(cpu, 0, 'f', 2)
                                 .arg(100.0 * cpu / elapsed, 0, 'f', 1);
        fprintf(stderr, "%s\n", qPrintable(line));
        LOG_INFO("Benchmark", line);
        return elapsed;
    };
    launchDemoWorkout();
    auto *wall = new QElapsedTimer;
    wall->start();
    QTimer::singleShot(3000, this, [this, seconds, wall, measure]() {
        const Window idle{processCpuSeconds(), wall->elapsed()};
        QTimer::singleShot(15000, this, [this, seconds, wall, measure, idle]() {
            measure(idle, *wall, "idle (not started)");
            QMetaObject::invokeMethod(m_ssWorkoutDlg, "start_or_pause_workout", Qt::DirectConnection);
            QTimer::singleShot(3000, this, [this, seconds, wall, measure]() {
                const Window running{processCpuSeconds(), wall->elapsed()};
                QTimer::singleShot(seconds * 1000, this, [wall, measure, running]() {
                    measure(running, *wall, "running");
                    delete wall;
                    QCoreApplication::quit();
                });
            });
        });
    });
}
#else
void MainWindow::runWorkoutBenchmark(int seconds)
{
    Q_UNUSED(seconds)
    fprintf(stderr, "--bench-workout is not supported on this platform\n");
    QCoreApplication::quit();
}
#endif


void MainWindow::runDeviceSessionCheck(const QString &outDir)
{
#ifndef GC_WASM_BUILD
    QDir().mkpath(outDir);
    account->use_simulated_sensors = false;
    account->isOffline = true;
    account->display_video = 0;
    resize(1280, 800);
    startDevices();

    auto *clock = new QElapsedTimer;
    clock->start();
    auto *report = new QStringList;
    const auto note = [clock, report](const QString &line) {
        const QString stamped = QStringLiteral("[%1 s] %2").arg(clock->elapsed() / 1000.0, 0, 'f', 1).arg(line);
        qInfo().noquote() << "DeviceSessionCheck" << stamped;
        report->append(stamped);
    };
    const auto finish = [this, outDir, report, clock](int code) {
        grab().save(outDir + QLatin1String("/main_window.png"), "PNG");
        QFile file(outDir + QLatin1String("/report.txt"));
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&file) << report->join(QLatin1Char('\n')) << '\n';
        delete report;
        delete clock;
        QTimer::singleShot(500, qApp, [code]() { qApp->exit(code); });
    };
    const auto findRide = []() -> WorkoutDialog * {
        for (QWidget *top : QApplication::topLevelWidgets())
            if (auto *ride = qobject_cast<WorkoutDialog *>(top))
                if (ride->isVisible())
                    return ride;
        return nullptr;
    };
    // Polls \a done every 250 ms for up to \a timeoutMs, then runs \a next(ok).
    const auto waitFor = [](std::function<bool()> done, int timeoutMs, std::function<void(bool)> next) {
        auto *poll = new QTimer;
        auto *waited = new QElapsedTimer;
        waited->start();
        QObject::connect(poll, &QTimer::timeout, poll, [poll, waited, done, timeoutMs, next]() {
            const bool ok = done();
            if (!ok && waited->elapsed() < timeoutMs)
                return;
            poll->deleteLater();
            delete waited;
            next(ok);
        });
        poll->start(250);
    };
    // Counts power packets the open ride receives, to prove the hub is wired to it.
    auto *packets = new int(0);
    const auto countPower = [this, packets]() {
        *packets = 0;
        if (BtleHub *trainer = m_devices->trainerHub())
            connect(trainer, &BtleHub::signal_power, m_devices, [packets](int, int) { ++*packets; });
    };

    // Nobody pedals during the check, so one 222 W packet goes through the real
    // hub; the open ride's power box must show it (read before the trainer's
    // next real 0 W packet overwrites it).
    const auto ridePowerShows222 = [this, findRide]() {
        BtleHub *trainer = m_devices->trainerHub();
        WorkoutDialog *ride = findRide();
        if (!trainer || !ride)
            return false;
        // The JetBlack reports power over Cycling Power (FTMS power is then ignored).
        trainer->simulateNotification(0x2A63, QByteArray::fromHex("0000de00"));
        auto *box = ride->findChild<QWidget *>(QStringLiteral("wid_2_infoBoxPower"));
        auto *value = box ? box->findChild<QLabel *>(QStringLiteral("label_currentValue")) : nullptr;
        return value && value->text().contains(QLatin1String("222"));
    };

    note(QStringLiteral("session mode %1, %2 device(s)").arg(int(m_devices->mode())).arg(m_devices->items().size()));
    waitFor([this]() { return m_devices->trainerHub() != nullptr; }, 90000,
            [=, this](bool trainerUp) {
        note(trainerUp ? QStringLiteral("trainer connected at startup") : QStringLiteral("FAIL trainer never connected"));
        if (!trainerUp) { finish(1); return; }
        countPower();
        grab().save(outDir + QLatin1String("/main_window_connected.png"), "PNG");

        on_actionOpen_Ride_triggered();
        WorkoutDialog *first = findRide();
        note(first ? QStringLiteral("ride 1 open") : QStringLiteral("FAIL ride 1 did not open"));
        if (!first) { finish(1); return; }
        const int before = *packets;
        QTimer::singleShot(6000, this, [=, this]() {
            note(QStringLiteral("ride 1: %1 real power packet(s) from the trainer in 6 s").arg(*packets - before));
            const bool wired1 = ridePowerShows222();
            note(wired1 ? QStringLiteral("ride 1 shows power from the trainer hub")
                        : QStringLiteral("FAIL ride 1 is not wired to the trainer hub"));
            if (WorkoutDialog *ride = findRide())
                ride->grab().save(outDir + QLatin1String("/ride1.png"), "PNG");
            if (WorkoutDialog *ride = findRide())
                QMetaObject::invokeMethod(ride, "reject");
            QTimer::singleShot(4000, this, [=, this]() {
                const bool stillUp = m_devices->trainerHub() && m_devices->trainerHub()->isConnected();
                note(stillUp ? QStringLiteral("trainer still connected after closing ride 1")
                             : QStringLiteral("FAIL trainer dropped after closing ride 1"));
                QElapsedTimer *opened = new QElapsedTimer;
                opened->start();
                const int atOpen = *packets;
                on_actionOpen_Ride_triggered();
                Q_UNUSED(atOpen)
                waitFor(ridePowerShows222, 15000, [=, this](bool flowing) {
                    note(flowing ? QStringLiteral("ride 2 shows power from the trainer hub %1 ms after opening").arg(opened->elapsed())
                                 : QStringLiteral("FAIL ride 2 is not wired to the trainer hub"));
                    delete opened;
                    if (WorkoutDialog *ride = findRide()) {
                        ride->grab().save(outDir + QLatin1String("/ride2.png"), "PNG");
                        QMetaObject::invokeMethod(ride, "reject");
                    }
                    QTimer::singleShot(3000, this, [=, this]() {
                        const bool up = m_devices->trainerHub() && m_devices->trainerHub()->isConnected();
                        note(up ? QStringLiteral("trainer still connected after closing ride 2")
                                : QStringLiteral("FAIL trainer dropped after closing ride 2"));
                        const int trainerKey = int(account->rowing_mode ? BtleSensorRole::Rower : BtleSensorRole::Trainer);
                        m_devices->disconnectDevice(trainerKey);
                        // One background-retry tick must not bring it back.
                        QTimer::singleShot(35000, this, [=, this]() {
                            const bool off = m_devices->trainerHub() == nullptr;
                            note(off ? QStringLiteral("trainer stayed disconnected through a retry tick")
                                     : QStringLiteral("FAIL disconnected trainer came back on its own"));
                            grab().save(outDir + QLatin1String("/main_window_trainer_off.png"), "PNG");
                            m_devices->retry(trainerKey);
                            waitFor([this]() { return m_devices->trainerHub() != nullptr; }, 30000, [=](bool back) {
                                note(back ? QStringLiteral("trainer reconnected on request")
                                          : QStringLiteral("FAIL trainer did not reconnect"));
                                finish(wired1 && stillUp && flowing && up && off && back ? 0 : 1);
                            });
                        });
                    });
                });
            });
        });
    });
#else
    Q_UNUSED(outDir)
    qApp->quit();
#endif
}
