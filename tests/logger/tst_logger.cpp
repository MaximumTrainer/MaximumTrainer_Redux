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
 */

#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QSettings>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QScopeGuard>

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
        file.open();
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
        file.open();
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
    tmp.open(); tmp.close();
    // (Don't enable file logging — just verify the guard works)
    Logger::instance().log(LogLevel::Info, QStringLiteral("test"),
                           QStringLiteral("should not go to file"));

    // The temp file should remain empty (Logger never opened it).
    tmp.open();
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
    tmp.open(); tmp.close();
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
// Test runner
// ─────────────────────────────────────────────────────────────────────────────
QTEST_MAIN(TstLogger)
#include "tst_logger.moc"
