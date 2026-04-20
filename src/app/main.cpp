#include "mainwindow.h"

#include <QDebug>
#include <QMessageBox>
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

#ifdef GC_HAVE_VLCQT
#include "myvlcplayer.h"
#endif




int main(int argc, char *argv[]) {

    // Install the Logger as the Qt message handler as early as possible so
    // that qWarning() / qCritical() calls from Qt internals and third-party
    // libraries are captured.  Default log level is Info; loadConfig() below
    // will override it from QSettings once the application identity is set.
    Logger::install();

    QApplication app(argc, argv);

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
    // (org "Max++ inc.", app "MaximumTrainer"), load logging preferences from
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

//    MyVlcPlayer player;
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
    app.setStyleSheet(lightQss);

    // --screenshots [dir] / /screenshots [dir]: bypass login, capture UI
    // screenshots to [dir] (default: <appdir>/screenshots), then quit.
    const QStringList &cliArgs = QCoreApplication::arguments();
    const bool screenshotMode = cliArgs.contains(QLatin1String("--screenshots"), Qt::CaseInsensitive)
                             || cliArgs.contains(QLatin1String("/screenshots"),  Qt::CaseInsensitive);

#ifndef Q_OS_WASM
    splash.setStatusMessage(QObject::tr("Applying theme…"));
    splash.setProgress(55);

    if (!screenshotMode) {
        splash.setStatusMessage(QObject::tr("Preparing login…"));
        splash.setProgress(80);
        // Hide the splash before showing the modal login dialog so the two
        // windows do not overlap on small displays.
        splash.hide();

        DialogLogin login;

        if (login.exec() != QDialog::Accepted) {
            return 0; // Login refused
        }
        if (login.getGotUpdate()) {
            return 0; // Executed DialogLogin and redirected to download new version
        }
    } else {
        splash.hide();
    }
#endif // Q_OS_WASM

    // Re-apply theme based on user preference now that the Account is loaded.
    {
        auto *account = qApp->property("Account").value<Account*>();
        if (account)
            AppTheme::apply(&app, static_cast<AppTheme::Mode>(account->app_theme));
    }

#ifdef Q_OS_WASM
    // WASM has no persistent server login. Initialize the global Account with
    // safe offline/guest defaults — identical to DialogLogin::loginOffline().
    {
        Account *wasmAccount = qApp->property("Account").value<Account*>();
        if (wasmAccount) {
            wasmAccount->isOffline            = true;
            wasmAccount->id                   = 0;
            wasmAccount->email                = QStringLiteral("local@offline");
            wasmAccount->email_clean          = QStringLiteral("offline_user");
            wasmAccount->display_name         = QObject::tr("Local User");
            wasmAccount->first_name           = QObject::tr("Local");
            wasmAccount->last_name            = QObject::tr("User");
            wasmAccount->subscription_type_id = 1;
            // Load any previously saved local preferences (folder paths, etc.).
            // XmlUtil gracefully returns if the save file does not yet exist.
            XmlUtil::parseLocalSaveFile(wasmAccount);
        }
    }
#endif // Q_OS_WASM

    MainWindow w;

#ifndef Q_OS_WASM
    splash.setProgress(100);
    splash.finish(&w);
#endif // Q_OS_WASM

    w.show();

    if (screenshotMode) {
        // Determine output directory from optional positional argument after the flag.
        QString outDir = QCoreApplication::applicationDirPath() + QLatin1String("/screenshots");
        // QList::indexOf has no CaseSensitivity overload; flags are always lowercase.
        int flagIdx = cliArgs.indexOf(QLatin1String("--screenshots"));
        if (flagIdx < 0)
            flagIdx = cliArgs.indexOf(QLatin1String("/screenshots"));
        if (flagIdx >= 0 && flagIdx + 1 < cliArgs.size()) {
            const QString &next = cliArgs.at(flagIdx + 1);
            if (!next.startsWith(QLatin1Char('-')) && !next.startsWith(QLatin1Char('/')))
                outDir = next;
        }
        QMetaObject::invokeMethod(&w, "startScreenshotMode", Qt::QueuedConnection,
                                  Q_ARG(QString, outDir));
    }

    return app.exec();
}




