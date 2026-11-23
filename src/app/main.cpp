#include "mainwindow.h"

#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <functional>
#include <QFontDatabase>
#ifdef Q_OS_WIN
#include <QOperatingSystemVersion>
#endif

#include "z_stylesheet.h"
#include "dialoglogin.h"
#include "globalvars.h"
#include "logger.h"
#include "splashscreen.h"
#include "account.h"
#include "xmlutil.h"
#include "apptheme.h"
#include "env_config.h"
#include "config_bootstrap.h"
#include "environnement.h"
#include "startup_trace.h"
#include "stallwatchdog.h"

#include <QElapsedTimer>
#include <QSurfaceFormat>
#include <QDir>
#include <exception>
#include <memory>

#ifndef Q_OS_WASM
#include <QUrl>
#include "zwift_click_test.h"
#include "groupsimtest.h"
#include "racesimulation.h"
#include "raceview.h"
#include "apptheme.h"
#endif


int main(int argc, char *argv[]) {

    QElapsedTimer startupTimer;
    startupTimer.start();

    // Identity and the log file come first, before QApplication and the splash:
    // the Windows build is a GUI-subsystem exe with no console, so a failure
    // during startup is only diagnosable if the file sink is already open.
    QStringList rawArgs;
    for (int i = 0; i < argc; ++i)
        rawArgs << QString::fromLocal8Bit(argv[i]);
    ConfigBootstrap::applyAppIdentity(Environnement::getVersion());
    Logger::instance().bootstrap(rawArgs);
    Logger::install();
    StartupTrace::installCrashHandlers();

#if defined(Q_OS_LINUX) && !defined(Q_OS_WASM)
    // Force the xcb platform (via XWayland) under a native Wayland session.
    // QtWebEngine and embedded native child widgets are markedly more stable
    // on xcb than on the Wayland QPA plugin; native Wayland exposed several
    // event-routing / rendering issues. Only do this when we detect Wayland
    // and the user has not pinned a platform with QT_QPA_PLATFORM.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        const QByteArray sessionType = qgetenv("XDG_SESSION_TYPE");
        const bool isWayland = sessionType.compare("wayland", Qt::CaseInsensitive) == 0
                            || !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY");
        if (isWayland)
            qputenv("QT_QPA_PLATFORM", "xcb");
    }

    // Select the xdg-desktop-portal platform theme so the app tracks the
    // desktop's light/dark preference. Under xcb (which we force above on
    // Wayland) Qt does not auto-select a platform theme that exposes the
    // GNOME/KDE colour scheme, so QStyleHints::colorScheme() — read by
    // AppTheme::resolveMode() for "System" mode — reports Light regardless of
    // the desktop being in dark mode. The xdgdesktopportal theme reads the
    // standard org.freedesktop.appearance color-scheme over D-Bus and works
    // across desktops. Only set it when the user has not pinned a theme; with
    // no portal running it harmlessly falls back (System resolves to Light,
    // same as before). The plugin (and libQt6DBus) must be bundled in the
    // AppImage — see release-linux.yml.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME"))
        qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");

    // Use the PulseAudio audio backend rather than Qt's default PipeWire-native
    // one. On PipeWire systems the native backend enumerates only the ALSA
    // sinks, misses Bluetooth outputs entirely, and reports the wrong default —
    // so playback gets stuck on e.g. HDMI and never follows the user's
    // headphones. The PulseAudio backend (served by pipewire-pulse on modern
    // desktops, or PulseAudio proper) sees every sink incl. Bluetooth and tracks
    // the system default correctly. If PulseAudio isn't available Qt logs a
    // notice and falls back to its default backend, so this is safe to force.
    if (qEnvironmentVariableIsEmpty("QT_AUDIO_BACKEND"))
        qputenv("QT_AUDIO_BACKEND", "pulseaudio");
#endif

    // QtWebEngine requires the GUI thread and Chromium's GPU process to share
    // an OpenGL context. On Qt6 this is enforced strictly: without
    // AA_ShareOpenGLContexts (set BEFORE QApplication is constructed) an
    // embedded QWebEngineView can fail to create its GL surface and tear down
    // itself and its host widget — e.g. the workout dialog vanishing when the
    // web video player is shown. Qt5 was more lenient, which is why this
    // surfaced only after the Qt6 migration.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#ifdef Q_OS_LINUX
    // The workout window has GL content (video, web), so every widget repaint
    // is presented through a GL swap. Under XWayland that swap waits on vsync and
    // stalled the whole UI for ~150 ms every ~11 s (mini-graphs froze). The
    // repaints are already paced by their own timers, so don't block on vsync.
    {
        QSurfaceFormat format = QSurfaceFormat::defaultFormat();
        format.setSwapInterval(0);
        QSurfaceFormat::setDefaultFormat(format);
    }
#endif

    // The workout view has NATIVE children (QVideoWidget, QtWebEngine). By
    // default Qt makes every sibling of a native child native too, and the media
    // player's overlay hint label then stops compositing until a hide/show cycle.
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    std::unique_ptr<QApplication> appHolder;
    {
        StartupStep step("main: QApplication");
        appHolder = std::make_unique<QApplication>(argc, argv);
    }
    QApplication &app = *appHolder;
    if (app.arguments().contains(QLatin1String("--stall-check"), Qt::CaseInsensitive))
        StallWatchdog::start();

    // Bundled UI font: one family with a deliberate weight scale on every
    // platform, instead of whatever each OS defaults to. Inter (SIL OFL,
    // resources/fonts/INTER-LICENSE.txt) — chosen for its legible numerals
    // on the live workout displays.
    for (const char *fontFile : { ":/fonts/resources/fonts/Inter-Regular.ttf",
                                  ":/fonts/resources/fonts/Inter-SemiBold.ttf",
                                  ":/fonts/resources/fonts/Inter-Bold.ttf" }) {
        if (QFontDatabase::addApplicationFont(QLatin1String(fontFile)) == -1)
            qWarning() << "Could not load bundled font" << fontFile;
    }
    {
        QFont uiFont(QStringLiteral("Inter"));
        uiFont.setPointSize(10);
        app.setFont(uiFont);
    }

    // Do NOT auto-quit when the last window closes. On Qt6, QtWebEngine spins up
    // transient helper windows (e.g. when the embedded web video player is
    // shown); their lifecycle can momentarily leave zero top-level windows and
    // trigger quitOnLastWindowClosed, terminating the whole app while a workout
    // dialog is open. The app instead quits explicitly from MainWindow::closeEvent.
    app.setQuitOnLastWindowClosed(false);

#ifdef Q_OS_WIN
    // Windows 10 version 1809 (build 17763) is Qt 6.11's minimum supported
    // Windows; it also covers the WinRT Bluetooth LE APIs (1703+).
    if (QOperatingSystemVersion::current() <
            QOperatingSystemVersion(QOperatingSystemVersion::Windows, 10, 0, 17763)) {
        QMessageBox::critical(nullptr,
            QObject::tr("Unsupported Windows Version"),
            QObject::tr("MaximumTrainer requires Windows 10 version 1809 or later.\n"
                        "Windows 7, 8, and 8.1 are not supported (missing WinRT Bluetooth LE APIs).\n\n"
                        "Please upgrade your operating system."));
        return 1;
    }
#endif

#ifndef Q_OS_WASM
    // Show the splash screen during the initialisation phase.
    // QSplashScreen is not available in WebAssembly builds because there is
    // no native windowing system; the browser page itself serves that purpose.
    SplashScreen splash;
    splash.show();
    QApplication::processEvents();

    splash.setStatusMessage(QObject::tr("Loading configuration…"));
    splash.setProgress(10);
#endif // Q_OS_WASM

    // Configuration bootstrap and the global objects. Any exception escaping
    // here is reported to the user instead of leaving the splash up forever.
    std::unique_ptr<GlobalVars> globalVars;
    const auto loadConfiguration = [&globalVars]() {
        {
            StartupStep step("main: configuration bootstrap (migration + defaults)");
            ConfigBootstrap::run(Logger::instance().defaultLogFilePath());
        }
        {
            StartupStep step("main: apply logging preferences");
            Logger::instance().loadConfig();
        }
        {
            // Account, Settings, SoundPlayer and QNetworkAccessManager
            StartupStep step("main: GlobalVars");
            globalVars = std::make_unique<GlobalVars>();
        }
    };
    QString startupFailure;
#ifdef Q_OS_WASM
    loadConfiguration();    // WebAssembly builds have exceptions disabled
#else
    try {
        loadConfiguration();
    } catch (const std::exception &e) {
        startupFailure = QString::fromLocal8Bit(e.what());
    } catch (...) {
        startupFailure = QStringLiteral("unknown exception");
    }
#endif
    if (!startupFailure.isEmpty()) {
        const QString logPath = Logger::instance().isFileLoggingEnabled()
            ? QDir::toNativeSeparators(Logger::instance().logFilePath())
            : QObject::tr("(no log file could be written)");
        LOG_ERROR("Startup", QStringLiteral("Startup failed while loading configuration: %1").arg(startupFailure));
#ifndef Q_OS_WASM
        splash.hide();
#endif
        if (!QCoreApplication::arguments().contains(QLatin1String("--startup-check"), Qt::CaseInsensitive))
            QMessageBox::critical(nullptr,
                QObject::tr("MaximumTrainer could not start"),
                QObject::tr("MaximumTrainer failed while loading its configuration:\n%1\n\n"
                            "Details have been written to the log file:\n%2")
                    .arg(startupFailure, logPath));
        return 2;
    }

    if (Logger::hasDebugSwitch(QCoreApplication::arguments()))
        LOG_INFO("main", QStringLiteral("Debug mode enabled via command-line switch"));

    LOG_INFO("main", QStringLiteral("MaximumTrainer starting"));

    // MT_NO_NETWORK=1 forces offline mode, disabling all network activity.
    // See env_config.h for the full list of supported environment variables.
    if (qEnvironmentVariableIntValue(EnvConfig::NoNetwork) != 0) {
        if (auto *acct = qApp->property("Account").value<Account*>())
            acct->isOffline = true;
        LOG_INFO("main", QStringLiteral("Network disabled (MT_NO_NETWORK=1)"));
    }


#ifndef Q_OS_WASM
    splash.setStatusMessage(QObject::tr("Initializing user profile…"));
    splash.setProgress(30);
#endif // Q_OS_WASM

    {
        /// App Stylesheet — apply light theme as baseline; re-applied after login
        /// based on the user's saved theme preference (Light / Dark / System).
        StartupStep step("main: stylesheet / theme");
        Z_StyleSheet styleSheetDummy;
        const QString lightQss = styleSheetDummy.styleSheet();
        qApp->setProperty("lightStylesheet", lightQss);
        // Force the light palette for the baseline/login UI too: otherwise, under an
        // OS dark theme Qt hands the app a dark palette and the light stylesheet
        // (which only sets a few backgrounds) renders light text on light widgets.
        app.setPalette(AppTheme::lightPalette());
        app.setStyleSheet(lightQss);
    }

    // --screenshots [dir] / /screenshots [dir]: bypass login, capture UI
    // screenshots to [dir] (default: <appdir>/screenshots), then quit.
    const QStringList &cliArgs = QCoreApplication::arguments();
    [[maybe_unused]] const bool screenshotMode = cliArgs.contains(QLatin1String("--screenshots"), Qt::CaseInsensitive)
                             || cliArgs.contains(QLatin1String("/screenshots"),  Qt::CaseInsensitive);

#ifndef Q_OS_WASM
    // --zwift-click-test [nameFilter] [seconds]: headless test of Zwift Click v2
    // controller INPUT read over a direct BLE connection to the controller (its
    // own peripheral — never touches the trainer, so ERG/FTMS is unaffected).
    // Connects to the controller(s), then prints the mapped action for every
    // button press. Proves button reading on hardware before any UI wiring.
    if (cliArgs.contains(QLatin1String("--zwift-click-test"), Qt::CaseInsensitive)) {
        splash.hide();
        const int idx = cliArgs.indexOf(QLatin1String("--zwift-click-test"));
        QString nameFilter;
        int runSeconds = 120;
        if (idx >= 0 && idx + 1 < cliArgs.size()
            && !cliArgs.at(idx + 1).startsWith(QLatin1Char('-'))) {
            bool isNum = false;
            const int n = cliArgs.at(idx + 1).toInt(&isNum);
            if (isNum) runSeconds = n;            // first arg numeric ⇒ duration
            else       nameFilter = cliArgs.at(idx + 1);
        }
        if (idx >= 0 && idx + 2 < cliArgs.size()) {
            bool isNum = false;
            const int n = cliArgs.at(idx + 2).toInt(&isNum);
            if (isNum) runSeconds = n;
        }
        // --one: connect to only one controller, to test whether a single link
        // surfaces every button (the linked pair may echo both units' presses).
        const bool singleDevice = cliArgs.contains(QLatin1String("--one"), Qt::CaseInsensitive);
        auto *t = new ZwiftClickTest(&app);
        QObject::connect(t, &ZwiftClickTest::finished, &app, &QCoreApplication::quit);
        t->start(nameFilter, /*scanSeconds=*/8, runSeconds, singleDevice);
        return app.exec();
    }

    // --group-panel-shot out.png [riders]: render the group riders column with
    // fake riders (layout check without a relay or a workout window).
    if (cliArgs.contains(QLatin1String("--group-panel-shot"))) {
        splash.hide();
        const int idx = cliArgs.indexOf(QLatin1String("--group-panel-shot"));
        const QString outPath = idx + 1 < cliArgs.size() ? cliArgs.at(idx + 1) : QStringLiteral("group-panel.png");
        const int riders = idx + 2 < cliArgs.size() && cliArgs.at(idx + 2).toInt() > 0 ? cliArgs.at(idx + 2).toInt() : 20;
        return GroupSimTest::renderPanelPreview(outPath, riders) ? 0 : 1;
    }

    // --race-shot out.png [course] [seconds]: ride a bot race at 3 W/kg for the
    // given time and save the race view (layout check without a trainer).
    if (cliArgs.contains(QLatin1String("--race-shot"))) {
        splash.hide();
        const int idx = cliArgs.indexOf(QLatin1String("--race-shot"));
        const QString outPath = idx + 1 < cliArgs.size() ? cliArgs.at(idx + 1) : QStringLiteral("race.png");
        const QString courseId = idx + 2 < cliArgs.size() ? cliArgs.at(idx + 2) : QStringLiteral("longchamp");
        const double seconds = idx + 3 < cliArgs.size() ? cliArgs.at(idx + 3).toDouble() : 600.0;
        const auto course = RaceCourse::fromFile(QStringLiteral(":/courses/%1.json").arg(courseId));
        if (!course) return 1;
        if (qEnvironmentVariableIsSet("MT_PREVIEW_DARK")) qApp->setPalette(AppTheme::darkPalette());
        constexpr double kRiderKg = 72.0;
        RaceSimulation race(*course, QStringLiteral("You"), kRiderKg + RaceSimulation::kBotBikeMassKg, 0.30);
        for (const RaceBot &bot : RaceBot::defaultField(42)) race.addBot(bot);
        for (double t = 0.0; t < seconds && !race.youFinished(); t += 0.25) {
            race.setYourPower(3.0 * kRiderKg);
            race.step(0.25);
        }
        RaceView view(&race, kRiderKg);
        view.resize(1350, 640);
        return view.grab().save(outPath) ? 0 : 1;
    }

    // --group-sim [riders] [seconds] [--relay URL] [--join CODE]: headless
    // group-workout room check (default relay: a local `wrangler dev` on port
    // 8787). With --join, the simulated riders join a room hosted in the app.
    if (cliArgs.contains(QLatin1String("--group-sim"), Qt::CaseInsensitive)) {
        splash.hide();
        const int idx = cliArgs.indexOf(QLatin1String("--group-sim"));
        int riderCount = 20;
        int rideSeconds = 30;
        if (idx + 1 < cliArgs.size() && cliArgs.at(idx + 1).toInt() > 0) riderCount = cliArgs.at(idx + 1).toInt();
        if (idx + 2 < cliArgs.size() && cliArgs.at(idx + 2).toInt() > 0) rideSeconds = cliArgs.at(idx + 2).toInt();
        QUrl relayUrl(QStringLiteral("ws://127.0.0.1:8787"));
        const int relayIdx = cliArgs.indexOf(QLatin1String("--relay"));
        if (relayIdx >= 0 && relayIdx + 1 < cliArgs.size()) relayUrl = QUrl(cliArgs.at(relayIdx + 1));
        const int joinIdx = cliArgs.indexOf(QLatin1String("--join"));
        const QString joinCode = joinIdx >= 0 && joinIdx + 1 < cliArgs.size() ? cliArgs.at(joinIdx + 1) : QString();
        auto *groupSim = new GroupSimTest(relayUrl, riderCount, rideSeconds, joinCode, &app);
        QObject::connect(groupSim, &GroupSimTest::finished, &app, &QCoreApplication::exit);
        groupSim->start();
        return app.exec();
    }
#endif

    // Creates and shows the MainWindow once login has completed (or has been
    // bypassed in screenshot mode). Heap-allocated so it outlives this scope;
    // the app quits explicitly from MainWindow::closeEvent (we disabled
    // quitOnLastWindowClosed above).
    // Mutually-recursive: launching the main window wires Log Out back to
    // showLogin(), and a successful login launches the main window. Declared as
    // std::function so each can call the other (both live for the whole run).
    std::function<MainWindow *()> launchMainWindow;
    std::function<void()> showLogin;

    launchMainWindow = [&]() -> MainWindow * {
        // Re-apply theme based on user preference now that the Account is loaded.
        if (auto *account = qApp->property("Account").value<Account*>())
            AppTheme::apply(qApp, static_cast<AppTheme::Mode>(account->app_theme));

        auto *mainWin = new MainWindow();
        // Log Out tears this window down and returns to the login screen
        // in-process (no app relaunch).
        QObject::connect(mainWin, &MainWindow::logoutRequested, mainWin, [&showLogin, mainWin]() {
            mainWin->deleteLater();
            showLogin();
        });
#ifndef Q_OS_WASM
        splash.setProgress(100);
        splash.finish(mainWin);
#endif
        mainWin->show();
        // Bring the window to the front and give it focus. After the non-modal
        // login dialog is dismissed some window managers (notably on Linux) leave
        // the freshly-shown MainWindow stacked behind other windows; raise() +
        // activateWindow() forces it on top and focused.
        mainWin->raise();
        mainWin->activateWindow();
        // On X11/Wayland the window is not yet mapped when show() returns, so the
        // raise()/activateWindow() above can be a no-op and the window comes up
        // behind others. Re-issue them once the event loop has processed the map.
        QTimer::singleShot(0, mainWin, [mainWin]() {
            mainWin->raise();
            mainWin->activateWindow();
        });
        return mainWin;
    };

#ifndef Q_OS_WASM
    splash.setStatusMessage(QObject::tr("Applying theme…"));
    splash.setProgress(55);

    // --sensor-check [dir]: bypass login, run the headless BLE sensor-validation
    // pass (inject each sensor type → screenshot + capture trainer-control), quit.
    // --bench-workout [seconds]: CPU usage of a running simulated workout.
    if (cliArgs.contains(QLatin1String("--bench-workout"), Qt::CaseInsensitive)) {
        splash.hide();
        MainWindow *w = launchMainWindow();
        int seconds = 60;
        const int idx = cliArgs.indexOf(QLatin1String("--bench-workout"));
        if (idx >= 0 && idx + 1 < cliArgs.size() && cliArgs.at(idx + 1).toInt() > 0)
            seconds = cliArgs.at(idx + 1).toInt();
        QMetaObject::invokeMethod(w, "runWorkoutBenchmark", Qt::QueuedConnection, Q_ARG(int, seconds));
        return app.exec();
    }

    if (cliArgs.contains(QLatin1String("--sensor-check"), Qt::CaseInsensitive)) {
        splash.hide();
        MainWindow *w = launchMainWindow();
        QString outDir = QCoreApplication::applicationDirPath() + QLatin1String("/sensor-check");
        const int idx = cliArgs.indexOf(QLatin1String("--sensor-check"));
        if (idx >= 0 && idx + 1 < cliArgs.size() && !cliArgs.at(idx + 1).startsWith(QLatin1Char('-')))
            outDir = cliArgs.at(idx + 1);
        QMetaObject::invokeMethod(w, "runSensorCheck", Qt::QueuedConnection,
                                  Q_ARG(QString, outDir));
        return app.exec();
    }

    if (screenshotMode) {
        // Bypass login entirely and capture UI screenshots, then quit.
        splash.hide();
        MainWindow *w = launchMainWindow();

        // Determine output directory from optional positional argument after the flag.
        QString outDir = QCoreApplication::applicationDirPath() + QLatin1String("/screenshots");
        // QList::indexOf has no CaseSensitivity overload; flags are always lowercase.
        int flagIdx = cliArgs.indexOf(QLatin1String("--screenshots"));
        if (flagIdx < 0)
            flagIdx = cliArgs.indexOf(QLatin1String("/screenshots"));
        if (flagIdx >= 0 && flagIdx + 1 < cliArgs.size()) {
            const QString &next = cliArgs.at(flagIdx + 1);
            // Accept any argument that does not begin with '-' (Unix/Windows flag)
            // or '//' (Windows UNC path used as a flag form).  On Unix, absolute
            // paths start with '/' and must be accepted as valid output directories.
            const bool looksLikeFlag = next.startsWith(QLatin1Char('-'))
                                    || next == QLatin1String("/screenshots")
                                    || next == QLatin1String("/debug");
            if (!looksLikeFlag)
                outDir = next;
        }
        QMetaObject::invokeMethod(w, "startScreenshotMode", Qt::QueuedConnection,
                                  Q_ARG(QString, outDir));
        return app.exec();
    }

    splash.setStatusMessage(QObject::tr("Preparing login…"));
    splash.setProgress(80);
    // Hide the splash before showing the login dialog so the two windows do not
    // overlap on small displays.
    splash.hide();
#endif // Q_OS_WASM

    // Show the login dialog NON-MODALLY on the main event loop, on every
    // platform. The embedded Intervals.icu OAuth QWebEngineView reparents
    // widgets when its web contents initialise, which destroys and recreates
    // the dialog's native window. Inside a nested QDialog::exec() loop that
    // window-destroy calls QEventLoop::exit() on the modal loop, so exec()
    // returns Rejected and the whole app silently quits the moment the user
    // clicks "Sign in with Intervals.icu". Running on the main app.exec() loop
    // (as the WASM path always has) removes that nested loop and the hazard.
    // MainWindow is created only once the user has successfully logged in.
    showLogin = [&]() {
        auto *loginDlg = new DialogLogin(nullptr);
        QObject::connect(loginDlg, &QDialog::accepted, loginDlg, [&launchMainWindow, loginDlg]() {
            launchMainWindow();
            loginDlg->deleteLater();
        });
        QObject::connect(loginDlg, &QDialog::rejected, loginDlg, [loginDlg]() {
            loginDlg->deleteLater();
            qApp->quit();
        });
        loginDlg->show();
        // The login dialog is the first window the user sees; some Linux window
        // managers stack a freshly-shown, terminal-launched window behind others.
        // Force it to the front and focused, re-issuing once the event loop has
        // processed the map (raise()/activateWindow() right after show() can be a
        // no-op before the window is mapped). Clear any stale minimized state too.
        loginDlg->setWindowState((loginDlg->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        loginDlg->raise();
        loginDlg->activateWindow();
        QTimer::singleShot(0, loginDlg, [loginDlg]() {
            loginDlg->raise();
            loginDlg->activateWindow();
        });
    };
    {
        StartupStep step("main: login dialog shown");
        showLogin();
    }
    LOG_INFO("Startup", QStringLiteral("Startup complete: login dialog shown %1 ms after launch")
                            .arg(startupTimer.elapsed()));

    // --startup-check: CI smoke test. Quit once the login dialog has been up
    // for a moment, so every startup step above is exercised for real.
    if (cliArgs.contains(QLatin1String("--startup-check"), Qt::CaseInsensitive)) {
        QTimer::singleShot(1000, &app, []() {
            LOG_INFO("Startup", QStringLiteral("--startup-check: startup succeeded, exiting"));
            QCoreApplication::exit(0);
        });
    }

    return app.exec();
}

