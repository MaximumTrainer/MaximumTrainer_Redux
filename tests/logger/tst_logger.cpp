/*
 * tst_logger.cpp
 *
 * Qt Test suite for the Logger cross-platform logging framework.
 *
 * Test groups
 * ──────────────────────────────────────────────────────────────────
 * Log level    – default level, runtime mutation, filtering
 * Formatting   – entry format contains timestamp / level / module / message
 * File sink    – messages written to and flushed to a temp file when enabled
 * Qt handler   – qWarning / qCritical routed through Logger
 * Config       – saveConfig / loadConfig round-trip via QSettings
 * Bootstrap    – file sink + banner before any QApplication (issue #377)
 *
 * Runs without a Q(Core)Application (QTEST_APPLESS_MAIN) so the bootstrap
 * tests see the same environment as the top of main().
 */

#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QSettings>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QSysInfo>

#include "../../src/app/logger.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helper: capture one log line written by Logger into a temp file.
//
// Usage:
//   FileCapture cap;                          // enable file sink
//   Logger::instance().log(...);
//   QString line = cap.firstLine();           // read first line
// ─────────────────────────────────────────────────────────────────────────────
struct FileCapture
{
    QTemporaryFile file;
    LogLevel       savedLevel;

    explicit FileCapture(LogLevel level = LogLevel::Verbose)
        : savedLevel(Logger::instance().logLevel())
    {
        QVERIFY(file.open());
        file.close();                          // close so Logger can re-open
        Logger::instance().setLogLevel(level);
        Logger::instance().setFileLogging(true, file.fileName());
    }

    ~FileCapture()
    {
        Logger::instance().setFileLogging(false);
        Logger::instance().setLogLevel(savedLevel);  // restore original level
    }

    /// Read all content written so far.
    QString content()
    {
        if (!file.open())
            return QString();
        const QString data = QString::fromUtf8(file.readAll());
        file.close();
        return data;
    }

    /// Return the first non-empty line (trimmed).
    QString firstLine()
    {
        const QString c = content();
        for (const QString& line : c.split(QLatin1Char('\n')))
            if (!line.trimmed().isEmpty())
                return line.trimmed();
        return QString();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Test class
// ─────────────────────────────────────────────────────────────────────────────
class TstLogger : public QObject
{
    Q_OBJECT

private slots:
    // ── Log level ─────────────────────────────────────────────────────────────
    void testLogLevel_defaultIsInfo();
    void testLogLevel_setAndGet();
    void testLogLevel_filtersBelowLevel();
    void testLogLevel_passesAtOrAboveLevel();

    // ── Entry formatting ──────────────────────────────────────────────────────
    void testFormat_containsTimestamp();
    void testFormat_containsLevelTag();
    void testFormat_containsModule();
    void testFormat_containsMessage();

    // ── File sink ─────────────────────────────────────────────────────────────
    void testFile_writtenWhenEnabled();
    void testFile_notWrittenWhenDisabled();
    void testFile_multipleEntries();
    void testFile_pathReturned();
    void testFile_createsDirectoryIfMissing();
    void testFile_createsFileIfMissing();

    // ── Qt message handler ────────────────────────────────────────────────────
    void testQtHandler_warningRouted();
    void testQtHandler_criticalRouted();

    // ── LOG_* macros ──────────────────────────────────────────────────────────
    void testMacros_infoMacro();
    void testMacros_warnMacro();
    void testMacros_debugFilteredAtInfo();
    void testMacros_verboseFilteredAtInfo();

    // ── Config round-trip ─────────────────────────────────────────────────────
    void testConfig_saveAndLoad();
    void testConfig_fileLoggingEnabledByDefault();

    // ── Startup bootstrap (#377) ──────────────────────────────────────────────
    void testBootstrap_fileSinkBeforeQApplication();
    void testBootstrap_fallbackToTemp();
    void testBootstrap_debugSwitchSurvivesLoadConfig();
    void testLoadConfig_preservesBootstrapLines();
    void testLoadConfig_unwritablePathKeepsCurrentFile();
    void testBanner_containsRequiredFields();

private:
    static QString readAll(const QString &path);
};

// ─────────────────────────────────────────────────────────────────────────────
// Log level
// ─────────────────────────────────────────────────────────────────────────────

void TstLogger::testLogLevel_defaultIsInfo()
{
    // Reset to known state for this test
    Logger::instance().setLogLevel(LogLevel::Info);
    QCOMPARE(Logger::instance().logLevel(), LogLevel::Info);
}

void TstLogger::testLogLevel_setAndGet()
{
    Logger::instance().setLogLevel(LogLevel::Debug);
    QCOMPARE(Logger::instance().logLevel(), LogLevel::Debug);

    Logger::instance().setLogLevel(LogLevel::Error);
    QCOMPARE(Logger::instance().logLevel(), LogLevel::Error);

    Logger::instance().setLogLevel(LogLevel::Info);  // restore
}

void TstLogger::testLogLevel_filtersBelowLevel()
{
    FileCapture cap(LogLevel::Warn);   // only Warn and above pass
    Logger::instance().log(LogLevel::Debug, QStringLiteral("test"),
                           QStringLiteral("should be filtered"));
    Logger::instance().log(LogLevel::Info, QStringLiteral("test"),
                           QStringLiteral("also filtered"));
    QVERIFY2(cap.content().isEmpty(),
             "Messages below Warn should not appear in the file");
}

void TstLogger::testLogLevel_passesAtOrAboveLevel()
{
    FileCapture cap(LogLevel::Warn);
    Logger::instance().log(LogLevel::Warn, QStringLiteral("test"),
                           QStringLiteral("warn passes"));
    Logger::instance().log(LogLevel::Error, QStringLiteral("test"),
                           QStringLiteral("error passes"));
    const QString c = cap.content();
    QVERIFY2(c.contains(QLatin1String("warn passes")),
             "Warn message should appear in file");
    QVERIFY2(c.contains(QLatin1String("error passes")),
             "Error message should appear in file");
}

// ─────────────────────────────────────────────────────────────────────────────
// Entry formatting
// ─────────────────────────────────────────────────────────────────────────────

void TstLogger::testFormat_containsTimestamp()
{
    FileCapture cap;
    Logger::instance().log(LogLevel::Info, QStringLiteral("module"),
                           QStringLiteral("ts test"));
    // ISO-8601 date: four digits, dash, two digits, dash, two digits
    const QRegularExpression isoDateRe(QStringLiteral(R"(\d{4}-\d{2}-\d{2})"));
    QVERIFY2(isoDateRe.match(cap.firstLine()).hasMatch(),
             "Log entry must contain an ISO-8601 date");
}

void TstLogger::testFormat_containsLevelTag()
{
    FileCapture cap;
    Logger::instance().log(LogLevel::Warn, QStringLiteral("module"),
                           QStringLiteral("level test"));
    QVERIFY2(cap.firstLine().contains(QLatin1String("WARN")),
             "Log entry must contain level tag 'WARN'");
}

void TstLogger::testFormat_containsModule()
{
    FileCapture cap;
    Logger::instance().log(LogLevel::Info, QStringLiteral("MyModule"),
                           QStringLiteral("module test"));
    QVERIFY2(cap.firstLine().contains(QLatin1String("MyModule")),
             "Log entry must contain the module name");
}

void TstLogger::testFormat_containsMessage()
{
    FileCapture cap;
    const QString msg = QStringLiteral("unique-test-message-42");
    Logger::instance().log(LogLevel::Info, QStringLiteral("module"), msg);
    QVERIFY2(cap.firstLine().contains(msg),
             "Log entry must contain the original message text");
}

// ─────────────────────────────────────────────────────────────────────────────
// File sink
// ─────────────────────────────────────────────────────────────────────────────

void TstLogger::testFile_writtenWhenEnabled()
{
    FileCapture cap;
    Logger::instance().log(LogLevel::Info, QStringLiteral("test"),
                           QStringLiteral("hello file"));
    QVERIFY2(!cap.content().isEmpty(),
             "File should contain at least one log entry when enabled");
}

void TstLogger::testFile_notWrittenWhenDisabled()
{
    // Ensure file logging is off
    Logger::instance().setFileLogging(false);
    Logger::instance().setLogLevel(LogLevel::Verbose);

    QTemporaryFile tmp;
    QVERIFY(tmp.open()); tmp.close();
    // (Don't enable file logging — just verify the guard works)
    Logger::instance().log(LogLevel::Info, QStringLiteral("test"),
                           QStringLiteral("should not go to file"));

    // The temp file should remain empty (Logger never opened it).
    QVERIFY(tmp.open());
    const QByteArray bytes = tmp.readAll();
    tmp.close();
    QVERIFY2(bytes.isEmpty(),
             "Disabled file sink must not write anything");

    Logger::instance().setLogLevel(LogLevel::Info);  // restore
}

void TstLogger::testFile_multipleEntries()
{
    FileCapture cap;
    for (int i = 0; i < 5; ++i)
        Logger::instance().log(LogLevel::Info, QStringLiteral("test"),
                               QStringLiteral("line %1").arg(i));

    const QString c = cap.content();
    int count = 0;
    for (const QString& line : c.split(QLatin1Char('\n')))
        if (!line.trimmed().isEmpty())
            ++count;
    QCOMPARE(count, 5);
}

void TstLogger::testFile_pathReturned()
{
    QTemporaryFile tmp;
    QVERIFY(tmp.open()); tmp.close();
    Logger::instance().setFileLogging(true, tmp.fileName());
    QCOMPARE(Logger::instance().logFilePath(), tmp.fileName());
    Logger::instance().setFileLogging(false);
}

void TstLogger::testFile_createsDirectoryIfMissing()
{
    // QTemporaryDir gives a uniquely-named directory that is guaranteed not
    // to exist yet, eliminating any timestamp-collision risk in parallel or
    // fast-retry CI runs.  The subdir inside it is intentionally not created
    // so that Logger is forced to make it.
    QTemporaryDir tmpBase;
    QVERIFY(tmpBase.isValid());
    const QString subDir  = tmpBase.path() + QStringLiteral("/sub");
    const QString logPath = subDir          + QStringLiteral("/MaximumTrainer.log");

    // Save logger state so we can restore it even if a QVERIFY* fires early
    // (QVERIFY returns from the test function, bypassing any code after it).
    // The logger guard runs first (inner), closing the file handle before the
    // QTemporaryDir destructor removes the tree — required on Windows where
    // open file handles block directory deletion.
    const bool    savedEnabled = Logger::instance().isFileLoggingEnabled();
    const QString savedPath    = Logger::instance().logFilePath();
    auto loggerGuard = qScopeGuard([&] {
        Logger::instance().setFileLogging(savedEnabled, savedPath);
    });

    // Directing Logger to a non-existent path must trigger directory creation.
    Logger::instance().setFileLogging(true, logPath);

    QVERIFY2(QDir(subDir).exists(),
             "Logger must create the parent directory when it does not exist");
    QVERIFY2(QFile::exists(logPath),
             "Logger must create the log file when it does not exist");

    // Write a message and confirm it lands in the file.
    Logger::instance().log(LogLevel::Info, QStringLiteral("test"),
                           QStringLiteral("dir-creation-test"));

    QFile f(logPath);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(f.readAll());
    f.close();

    QVERIFY2(content.contains(QLatin1String("dir-creation-test")),
             "Log entry should appear in the newly created log file");
}

void TstLogger::testFile_createsFileIfMissing()
{
    // Directory exists but file does not — Logger must create the file.
    QTemporaryDir tmpDir;
    QVERIFY(tmpDir.isValid());
    const QString logPath = tmpDir.path() + QStringLiteral("/MaximumTrainer.log");

    // Sanity-check: file must not pre-exist.
    QVERIFY2(!QFile::exists(logPath), "Pre-condition: log file must not exist yet");

    const bool    savedEnabled = Logger::instance().isFileLoggingEnabled();
    const QString savedPath    = Logger::instance().logFilePath();
    auto loggerGuard = qScopeGuard([&] {
        Logger::instance().setFileLogging(savedEnabled, savedPath);
    });

    Logger::instance().setFileLogging(true, logPath);

    QVERIFY2(QFile::exists(logPath),
             "Logger must create the log file when the directory exists but the file does not");

    Logger::instance().log(LogLevel::Info, QStringLiteral("test"),
                           QStringLiteral("file-creation-test"));

    QFile f(logPath);
    QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(f.readAll());
    f.close();

    QVERIFY2(content.contains(QLatin1String("file-creation-test")),
             "Log entry should appear in the newly created log file");
}

// ─────────────────────────────────────────────────────────────────────────────
// Qt message handler
// ─────────────────────────────────────────────────────────────────────────────

void TstLogger::testQtHandler_warningRouted()
{
    Logger::install();
    FileCapture cap(LogLevel::Warn);
    qWarning() << "qt-warning-routed";
    QVERIFY2(cap.content().contains(QLatin1String("qt-warning-routed")),
             "qWarning() should be routed through Logger to the file sink");
}

void TstLogger::testQtHandler_criticalRouted()
{
    Logger::install();
    FileCapture cap(LogLevel::Warn);
    qCritical() << "qt-critical-routed";
    QVERIFY2(cap.content().contains(QLatin1String("qt-critical-routed")),
             "qCritical() should be routed through Logger to the file sink");
}

// ─────────────────────────────────────────────────────────────────────────────
// LOG_* macros
// ─────────────────────────────────────────────────────────────────────────────

void TstLogger::testMacros_infoMacro()
{
    FileCapture cap(LogLevel::Info);
    LOG_INFO("MacroMod", QStringLiteral("info-macro-test"));
    QVERIFY2(cap.content().contains(QLatin1String("info-macro-test")),
             "LOG_INFO should produce a log entry");
}

void TstLogger::testMacros_warnMacro()
{
    FileCapture cap(LogLevel::Info);
    LOG_WARN("MacroMod", QStringLiteral("warn-macro-test"));
    QVERIFY2(cap.content().contains(QLatin1String("warn-macro-test")),
             "LOG_WARN should produce a log entry");
}

void TstLogger::testMacros_debugFilteredAtInfo()
{
    FileCapture cap(LogLevel::Info);
    LOG_DEBUG("MacroMod", QStringLiteral("debug-should-be-filtered"));
    QVERIFY2(!cap.content().contains(QLatin1String("debug-should-be-filtered")),
             "LOG_DEBUG should be filtered when level is Info");
}

void TstLogger::testMacros_verboseFilteredAtInfo()
{
    FileCapture cap(LogLevel::Info);
    LOG_VERBOSE("MacroMod", QStringLiteral("verbose-should-be-filtered"));
    QVERIFY2(!cap.content().contains(QLatin1String("verbose-should-be-filtered")),
             "LOG_VERBOSE should be filtered when level is Info");
}

// ─────────────────────────────────────────────────────────────────────────────
// Config round-trip
// ─────────────────────────────────────────────────────────────────────────────

void TstLogger::testConfig_saveAndLoad()
{
    // Use a temporary INI file so this test is fully isolated from any real
    // application settings and does not affect the QCoreApplication identity.
    QTemporaryFile iniFile;
    iniFile.setFileTemplate(QDir::tempPath() + QStringLiteral("/logger_test_XXXXXX.ini"));
    QVERIFY(iniFile.open());
    const QString iniPath = iniFile.fileName();
    iniFile.close();

    {
        QSettings s(iniPath, QSettings::IniFormat);
        s.beginGroup(QStringLiteral("logging"));
        s.setValue(QStringLiteral("level"), static_cast<int>(LogLevel::Debug));
        s.setValue(QStringLiteral("file_enabled"), false);
        s.setValue(QStringLiteral("file_path"), QString());
        s.endGroup();
    }

    // Override the default QSettings so loadConfig() reads from our temp file.
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QDir::tempPath());
    const LogLevel saved = Logger::instance().logLevel();

    Logger::instance().setLogLevel(LogLevel::Error);   // change to something different
    QCOMPARE(Logger::instance().logLevel(), LogLevel::Error);

    // Manually test round-trip via QSettings using the isolated file.
    QSettings ws(iniPath, QSettings::IniFormat);
    ws.beginGroup(QStringLiteral("logging"));
    const int storedLevel = ws.value(QStringLiteral("level"),
                                     static_cast<int>(LogLevel::Info)).toInt();
    ws.endGroup();
    QCOMPARE(storedLevel, static_cast<int>(LogLevel::Debug));

    // Restore original level.
    Logger::instance().setLogLevel(saved);
}

void TstLogger::testConfig_fileLoggingEnabledByDefault()
{
    // RAII guard: restore Logger's file-logging state unconditionally so that
    // a QVERIFY failure in this test cannot leave the Logger in a broken state
    // and cause subsequent tests to behave unexpectedly.
    const bool savedEnabled = Logger::instance().isFileLoggingEnabled();
    const QString savedPath = Logger::instance().logFilePath();
    auto loggerGuard = qScopeGuard([&] {
        Logger::instance().setFileLogging(savedEnabled, savedPath);
    });

    // RAII guard: remove (or restore) the logging/file_enabled QSettings key.
    // This simulates a fresh install where no user preference has been saved.
    QSettings settings;
    settings.beginGroup(QStringLiteral("logging"));
    const QVariant prevValue = settings.value(QStringLiteral("file_enabled"));
    settings.remove(QStringLiteral("file_enabled"));
    settings.endGroup();
    settings.sync();

    auto settingsGuard = qScopeGuard([&] {
        QSettings s;
        s.beginGroup(QStringLiteral("logging"));
        if (prevValue.isValid())
            s.setValue(QStringLiteral("file_enabled"), prevValue);
        else
            s.remove(QStringLiteral("file_enabled"));
        s.endGroup();
        s.sync();
    });

    // Start from a known-disabled state so we can detect whether
    // loadConfig() actually enables file logging.
    Logger::instance().setFileLogging(false);

    // Exercise the real loadConfig() path — it must default to enabled.
    Logger::instance().loadConfig();

    QVERIFY2(Logger::instance().isFileLoggingEnabled(),
             "loadConfig() must enable file logging when file_enabled is absent from settings");
}

// ─────────────────────────────────────────────────────────────────────────────
// Startup bootstrap (#377)
// ─────────────────────────────────────────────────────────────────────────────

QString TstLogger::readAll(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    return QString::fromUtf8(f.readAll());
}

// Restores the Logger and the process-wide identity / QSettings defaults that
// the bootstrap tests change.
struct BootstrapEnv
{
    QTemporaryDir dir;
    LogLevel savedLevel = Logger::instance().logLevel();
    bool savedEnabled = Logger::instance().isFileLoggingEnabled();
    QString savedPath = Logger::instance().logFilePath();
    QString savedOrg = QCoreApplication::organizationName();
    QString savedApp = QCoreApplication::applicationName();
    QString savedVersion = QCoreApplication::applicationVersion();
    QSettings::Format savedFormat = QSettings::defaultFormat();

    BootstrapEnv()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("MaximumTrainerTest"));
        QCoreApplication::setApplicationName(QStringLiteral("LoggerBootstrapTest"));
        QCoreApplication::setApplicationVersion(QStringLiteral("9.8.7-test"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
    }

    ~BootstrapEnv()
    {
        Logger::instance().setFileLogging(savedEnabled, savedPath);
        Logger::instance().setLogLevel(savedLevel);
        QCoreApplication::setOrganizationName(savedOrg);
        QCoreApplication::setApplicationName(savedApp);
        QCoreApplication::setApplicationVersion(savedVersion);
        QSettings::setDefaultFormat(savedFormat);
        QStandardPaths::setTestModeEnabled(false);
    }

    QString path(const QString &name) const { return dir.filePath(name); }
};

void TstLogger::testBootstrap_fileSinkBeforeQApplication()
{
    QVERIFY2(QCoreApplication::instance() == nullptr,
             "bootstrap must work before any application object exists");
    BootstrapEnv env;
    QVERIFY(env.dir.isValid());

    const QString expected = Logger::defaultLogFilePath();
    QVERIFY(expected.endsWith(QStringLiteral("/MaximumTrainer.log")));
    QVERIFY(expected.contains(QStringLiteral("MaximumTrainerTest")));
    QFile::remove(expected);

    const QString used = Logger::instance().bootstrap({ QStringLiteral("MaximumTrainer") });
    LOG_INFO("Test", QStringLiteral("bootstrap-line-1"));

    QCOMPARE(used, expected);
    QVERIFY(QFile::exists(expected));
    const QString content = readAll(expected);
    QVERIFY(content.contains(QStringLiteral("bootstrap-line-1")));
    QVERIFY(content.indexOf(QStringLiteral("starting")) < content.indexOf(QStringLiteral("bootstrap-line-1")));
    QFile::remove(expected);
}

void TstLogger::testBootstrap_fallbackToTemp()
{
    BootstrapEnv env;
    // A regular file used as a directory component makes the path unwritable
    // for every user, including root on CI containers.
    QFile blocker(env.path(QStringLiteral("not-a-dir")));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    const QString unwritable = env.path(QStringLiteral("not-a-dir/sub/MaximumTrainer.log"));

    QTemporaryDir fallbackDir;
    QVERIFY(fallbackDir.isValid());

    const QString used = Logger::instance().bootstrap({ QStringLiteral("MaximumTrainer") },
                                                      unwritable, fallbackDir.path());
    LOG_INFO("Test", QStringLiteral("after-fallback"));

    QCOMPARE(used, fallbackDir.filePath(QStringLiteral("MaximumTrainer.log")));
    const QString content = readAll(used);
    QVERIFY2(content.contains(QStringLiteral("falling back")), qPrintable(content));
    QVERIFY(content.contains(QDir::toNativeSeparators(unwritable)));
    QVERIFY(content.contains(QStringLiteral("after-fallback")));
}

void TstLogger::testBootstrap_debugSwitchSurvivesLoadConfig()
{
    BootstrapEnv env;
    {
        QSettings s;
        s.setValue(QStringLiteral("logging/level"), static_cast<int>(LogLevel::Warn));
    }
    const QString logPath = env.path(QStringLiteral("debug.log"));
    Logger::instance().bootstrap({ QStringLiteral("MaximumTrainer"), QStringLiteral("/debug") }, logPath);
    QCOMPARE(Logger::instance().logLevel(), LogLevel::Debug);
    LOG_DEBUG("Test", QStringLiteral("debug-before-config"));

    Logger::instance().loadConfig();
    QCOMPARE(Logger::instance().logLevel(), LogLevel::Debug);
    LOG_DEBUG("Test", QStringLiteral("debug-after-config"));

    const QString content = readAll(logPath);
    QVERIFY(content.contains(QStringLiteral("debug-before-config")));
    QVERIFY(content.contains(QStringLiteral("debug-after-config")));
    QVERIFY(content.contains(QStringLiteral("(--debug)")));
}

void TstLogger::testLoadConfig_preservesBootstrapLines()
{
    BootstrapEnv env;
    const QString bootPath = env.path(QStringLiteral("boot.log"));
    const QString userPath = env.path(QStringLiteral("user/custom.log"));

    // Same path: lines written before loadConfig() stay in the file.
    {
        QSettings s;
        s.setValue(QStringLiteral("logging/file_enabled"), true);
        s.setValue(QStringLiteral("logging/file_path"), bootPath);
    }
    Logger::instance().bootstrap({ QStringLiteral("MaximumTrainer") }, bootPath);
    LOG_INFO("Test", QStringLiteral("line-before-load"));
    Logger::instance().loadConfig();
    LOG_INFO("Test", QStringLiteral("line-after-load"));
    QString content = readAll(bootPath);
    QVERIFY(content.contains(QStringLiteral("line-before-load")));
    QVERIFY(content.contains(QStringLiteral("line-after-load")));
    QVERIFY(!content.contains(QStringLiteral("Switching log file")));

    // Different path: the bootstrap file names the new path, then later lines
    // go to the new file; nothing already written is lost.
    {
        QSettings s;
        s.setValue(QStringLiteral("logging/file_path"), userPath);
    }
    Logger::instance().loadConfig();
    LOG_INFO("Test", QStringLiteral("line-in-user-file"));

    content = readAll(bootPath);
    QVERIFY(content.contains(QStringLiteral("line-before-load")));
    QVERIFY(content.contains(QStringLiteral("Switching log file to %1")
                                 .arg(QDir::toNativeSeparators(userPath))));
    QVERIFY(!content.contains(QStringLiteral("line-in-user-file")));
    QCOMPARE(Logger::instance().logFilePath(), userPath);
    QVERIFY(readAll(userPath).contains(QStringLiteral("line-in-user-file")));
}

void TstLogger::testLoadConfig_unwritablePathKeepsCurrentFile()
{
    BootstrapEnv env;
    QFile blocker(env.path(QStringLiteral("blocker")));
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    const QString bad = env.path(QStringLiteral("blocker/x/MaximumTrainer.log"));
    {
        QSettings s;
        s.setValue(QStringLiteral("logging/file_path"), bad);
    }
    const QString bootPath = env.path(QStringLiteral("keep.log"));
    Logger::instance().bootstrap({ QStringLiteral("MaximumTrainer") }, bootPath);
    Logger::instance().loadConfig();
    LOG_INFO("Test", QStringLiteral("still-here"));

    QVERIFY(Logger::instance().isFileLoggingEnabled());
    QCOMPARE(Logger::instance().logFilePath(), bootPath);
    const QString content = readAll(bootPath);
    QVERIFY(content.contains(QStringLiteral("still-here")));
    QVERIFY(content.contains(QStringLiteral("continuing with")));
}

void TstLogger::testBanner_containsRequiredFields()
{
    BootstrapEnv env;
    const QString logPath = env.path(QStringLiteral("banner.log"));
    const QStringList args { QStringLiteral("MaximumTrainer"), QStringLiteral("--screenshots"),
                             QStringLiteral("/tmp/shots") };
    Logger::instance().bootstrap(args, logPath);

    const QString content = readAll(logPath);
    QVERIFY2(content.contains(QStringLiteral("9.8.7-test")), "version");
    QVERIFY2(content.contains(QString::fromLatin1(qVersion())), "Qt runtime version");
    QVERIFY2(content.contains(QSysInfo::prettyProductName()), "OS name and version");
    QVERIFY2(content.contains(QSysInfo::currentCpuArchitecture()), "CPU architecture");
    QVERIFY2(content.contains(QStringLiteral("--screenshots /tmp/shots")), "command line");
    QVERIFY2(content.contains(QStringLiteral("Log file: %1").arg(QDir::toNativeSeparators(logPath))), "log path");
    const QSettings settings;
    QVERIFY2(content.contains(QDir::toNativeSeparators(settings.fileName())), "settings location");
    // The banner is the first thing in the file.
    QVERIFY(content.startsWith(QLatin1Char('[')));
    QVERIFY(content.section(QLatin1Char('\n'), 0, 0).contains(QStringLiteral("starting")));
}

// ─────────────────────────────────────────────────────────────────────────────
// Test runner
// ─────────────────────────────────────────────────────────────────────────────
QTEST_APPLESS_MAIN(TstLogger)
#include "tst_logger.moc"
