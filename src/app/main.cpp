#include "mainwindow.h"

#include <QDebug>
#include <QMessageBox>
#include <QTimer>
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

#ifndef Q_OS_WASM
#include <QQuickWidget>
#include <QQmlContext>
#include <QUrl>
#include <QPainter>
#include <QImage>
#include "retroracecontroller.h"
#endif





int main(int argc, char *argv[]) {

    // Install the Logger as the Qt message handler as early as possible so
    // that qWarning() / qCritical() calls from Qt internals and third-party
    // libraries are captured.  Default log level is Info; loadConfig() below
    // will override it from QSettings once the application identity is set.
    Logger::install();

    // Spike/dev: don't serve a stale compiled copy of the game scene from the
    // QML disk cache — Qt6 does not reliably invalidate it for QML embedded in a
    // qrc resource, so edited scenes can silently keep rendering the old version.
    qputenv("QML_DISABLE_DISK_CACHE", "1");

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

    QApplication app(argc, argv);

    // Do NOT auto-quit when the last window closes. On Qt6, QtWebEngine spins up
    // transient helper windows (e.g. when the embedded web video player is
    // shown); their lifecycle can momentarily leave zero top-level windows and
    // trigger quitOnLastWindowClosed, terminating the whole app while a workout
    // dialog is open. The app instead quits explicitly from MainWindow::closeEvent.
    app.setQuitOnLastWindowClosed(false);

#ifdef Q_OS_WIN
    // Windows 10 version 1703 (Creators Update, build 15063) is the minimum
    // required for the WinRT Bluetooth LE APIs used by Qt Bluetooth.
    if (QOperatingSystemVersion::current() <
            QOperatingSystemVersion(QOperatingSystemVersion::Windows, 10, 0, 15063)) {
        QMessageBox::critical(nullptr,
            QObject::tr("Unsupported Windows Version"),
            QObject::tr("MaximumTrainer requires Windows 10 version 1703 (Creators Update) or later.\n"
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

    //initialize global object (Account, Settings, SoundPlayer and QNetworkAccessManager)
    GlobalVars myVars;

    // Now that the application identity has been established by GlobalVars
    // (org "MaximumTrainer", app "MaximumTrainer_Redux"), load logging preferences from
    // QSettings so the user's level / file-path choices take effect for the
    // rest of the session.
    Logger::instance().loadConfig();

    // --debug (Unix style) and /debug (Windows style) both enable verbose
    // diagnostic output for this session, overriding any saved QSettings level.
    {
        const QStringList args = QCoreApplication::arguments();
        const bool debugMode = args.contains(QStringLiteral("--debug"), Qt::CaseInsensitive)
                            || args.contains(QStringLiteral("/debug"),  Qt::CaseInsensitive);
        if (debugMode) {
            Logger::instance().setLogLevel(LogLevel::Debug);
            Logger::instance().setFileLogging(true);
            LOG_INFO("main", QStringLiteral("Debug mode enabled via command-line switch"));
        }
    }

    LOG_INFO("main", QStringLiteral("MaximumTrainer starting"));

    // MT_NO_NETWORK=1 forces offline mode, disabling all network activity.
    // See env_config.h for the full list of supported environment variables.
    if (qEnvironmentVariableIntValue(EnvConfig::NoNetwork) != 0) {
        if (auto *acct = qApp->property("Account").value<Account*>())
            acct->isOffline = true;
        LOG_INFO("main", QStringLiteral("Network disabled (MT_NO_NETWORK=1)"));
    }

//    QtMediaPlayer player;
//    player.setMinimumSize(QSize(500,300));
//    player.show();

//    WebBrowserV2 player;
//    player.setMinimumSize(QSize(500,300));
//    player.show();

#ifndef Q_OS_WASM
    splash.setStatusMessage(QObject::tr("Initializing user profile…"));
    splash.setProgress(30);
#endif // Q_OS_WASM

    /// App Stylesheet — apply light theme as baseline; re-applied after login
    /// based on the user's saved theme preference (Light / Dark / System).
    Z_StyleSheet styleSheetDummy;
    const QString lightQss = styleSheetDummy.styleSheet();
    qApp->setProperty("lightStylesheet", lightQss);
    // Force the light palette for the baseline/login UI too: otherwise, under an
    // OS dark theme Qt hands the app a dark palette and the light stylesheet
    // (which only sets a few backgrounds) renders light text on light widgets.
    app.setPalette(AppTheme::lightPalette());
    app.setStyleSheet(lightQss);

    // --screenshots [dir] / /screenshots [dir]: bypass login, capture UI
    // screenshots to [dir] (default: <appdir>/screenshots), then quit.
    const QStringList &cliArgs = QCoreApplication::arguments();
    const bool screenshotMode = cliArgs.contains(QLatin1String("--screenshots"), Qt::CaseInsensitive)
                             || cliArgs.contains(QLatin1String("/screenshots"),  Qt::CaseInsensitive);

#ifndef Q_OS_WASM
    // --retrorace [file.fit] [--shot out.png]: standalone spike of the retro
    // ghost-race view. Skips login/MainWindow entirely. With --shot it grabs one
    // frame to a PNG and quits (headless validation); otherwise it stays open.
    if (cliArgs.contains(QLatin1String("--retrorace"))) {
        splash.hide();

        const int raceIdx = cliArgs.indexOf(QLatin1String("--retrorace"));
        QString fitPath;
        if (raceIdx >= 0 && raceIdx + 1 < cliArgs.size()
            && !cliArgs.at(raceIdx + 1).startsWith(QLatin1Char('-')))
            fitPath = cliArgs.at(raceIdx + 1);

        auto *controller = new RetroRaceController(&app);
        // --bot [watts] forces a computer pacer; otherwise load your last ride
        // and fall back to a pacer when there is no history to race.
        const int botIdx = cliArgs.indexOf(QLatin1String("--bot"));
        if (botIdx >= 0) {
            double watts = 180.0;
            if (botIdx + 1 < cliArgs.size()) {
                bool ok = false;
                const double parsed = cliArgs.at(botIdx + 1).toDouble(&ok);
                if (ok) watts = parsed;
            }
            controller->useComputerPacer(watts);
            // Give the standalone spike a target so the effort glow is testable.
            controller->setTargetPower(watts, watts * 0.06);
            controller->setTargetCadence(90, 6);
        } else if (!controller->loadGhost(fitPath)) {
            controller->useComputerPacer(180.0);
        }

        auto *view = new QQuickWidget();
        view->setAttribute(Qt::WA_DeleteOnClose);
        view->rootContext()->setContextProperty(QStringLiteral("race"), controller);
        view->setResizeMode(QQuickWidget::SizeRootObjectToView);
        view->setSource(QUrl(QStringLiteral("qrc:/game/qml/RetroRace.qml")));
        view->resize(960, 540);
        view->setWindowTitle(QStringLiteral("MaximumTrainer — Retro Ghost Race (spike)"));
        view->show();
        controller->start();
        controller->beginRace();

        // --sign: simulate a workout feeding an upcoming-interval countdown so
        // the roadside interval sign can be exercised standalone.
        if (cliArgs.contains(QLatin1String("--sign"))) {
            const int sIdx = cliArgs.indexOf(QLatin1String("--sign"));
            bool holdOk = false;
            const double hold = (sIdx + 1 < cliArgs.size())
                ? cliArgs.at(sIdx + 1).toDouble(&holdOk) : 0.0;
            auto *secs = new double(holdOk ? hold : 24.0);
            auto *st = new QTimer(view);
            st->setInterval(1000);
            QObject::connect(st, &QTimer::timeout, view, [controller, secs, holdOk]() {
                controller->setNextInterval(275, 90, *secs);
                if (!holdOk) { *secs -= 1.0; if (*secs < 0.0) *secs = 24.0; }
            });
            controller->setNextInterval(275, 90, *secs);
            st->start();
        }
        // --finish [secs]: simulate the workout finish countdown so the
        // approaching finish line can be exercised standalone.
        if (cliArgs.contains(QLatin1String("--finish"))) {
            const int fIdx = cliArgs.indexOf(QLatin1String("--finish"));
            bool holdOk = false;
            const double hold = (fIdx + 1 < cliArgs.size())
                ? cliArgs.at(fIdx + 1).toDouble(&holdOk) : 0.0;
            auto *fsecs = new double(holdOk ? hold : 50.0);
            auto *ft = new QTimer(view);
            ft->setInterval(1000);
            QObject::connect(ft, &QTimer::timeout, view, [controller, fsecs, holdOk]() {
                controller->setFinishIn(*fsecs);
                if (!holdOk) { *fsecs -= 1.0; if (*fsecs < 0.0) *fsecs = 50.0; }
            });
            controller->setFinishIn(*fsecs);
            ft->start();
        }

        const int shotIdx = cliArgs.indexOf(QLatin1String("--shot"));
        if (shotIdx >= 0 && shotIdx + 1 < cliArgs.size()) {
            const QString out = cliArgs.at(shotIdx + 1);
            QTimer::singleShot(1500, view, [view, out]() {
                view->grabFramebuffer().save(out);
                QApplication::quit();
            });
        }
        // --filmstrip out.png: grab a sequence of frames and composite them into
        // one contact-sheet PNG (a poor-man's video for headless motion review).
        const int stripIdx = cliArgs.indexOf(QLatin1String("--filmstrip"));
        if (stripIdx >= 0 && stripIdx + 1 < cliArgs.size()) {
            const QString out = cliArgs.at(stripIdx + 1);
            bool intvOk = false;
            const int intv = (stripIdx + 2 < cliArgs.size())
                ? cliArgs.at(stripIdx + 2).toInt(&intvOk) : 0;
            const int cols = 2, rows = 3, cellW = 640, cellH = 360;
            auto *sheet = new QImage(cols * cellW, rows * cellH, QImage::Format_RGB32);
            sheet->fill(Qt::black);
            auto *painter = new QPainter(sheet);
            painter->setPen(Qt::yellow);
            auto *counter = new int(0);
            const int total = cols * rows;
            auto *t = new QTimer(view);
            t->setInterval(intvOk && intv > 0 ? intv : 350);
            QObject::connect(t, &QTimer::timeout, view, [=]() {
                const QImage f = view->grabFramebuffer();
                const int i = *counter;
                const int cx = (i % cols) * cellW, cy = (i / cols) * cellH;
                painter->drawImage(QRect(cx, cy, cellW, cellH), f);
                painter->drawText(cx + 6, cy + 18, QString::number(i));
                if (++(*counter) >= total) {
                    painter->end();
                    sheet->save(out);
                    t->stop();
                    QApplication::quit();
                }
            });
            QTimer::singleShot(500, t, [t]() { t->start(); });
        }
        return app.exec();
    }
#endif

    // Creates and shows the MainWindow once login has completed (or has been
    // bypassed in screenshot mode). Heap-allocated so it outlives this scope;
    // the app quits explicitly from MainWindow::closeEvent (we disabled
    // quitOnLastWindowClosed above).
    auto launchMainWindow = [&]() -> MainWindow * {
        // Re-apply theme based on user preference now that the Account is loaded.
        if (auto *account = qApp->property("Account").value<Account*>())
            AppTheme::apply(qApp, static_cast<AppTheme::Mode>(account->app_theme));

        auto *mainWin = new MainWindow();
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
    auto *loginDlg = new DialogLogin(nullptr);
    QObject::connect(loginDlg, &QDialog::accepted, loginDlg, [loginDlg, launchMainWindow]() {
        // getGotUpdate(): the dialog redirected the user to a new-version
        // download and rejected itself, so accepted() never fires in that case.
        launchMainWindow();
        loginDlg->deleteLater();
    });
    QObject::connect(loginDlg, &QDialog::rejected, loginDlg, [loginDlg]() {
        // Login refused, or redirected to a version download (getGotUpdate()).
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

    return app.exec();
}




