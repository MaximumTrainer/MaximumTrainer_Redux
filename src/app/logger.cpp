/*
 * logger.cpp
 *
 * Implementation of the unified cross-platform logging framework.
 * See logger.h for the public API and usage documentation.
 */

#include "logger.h"

#include <QDateTime>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSysInfo>

#include <cstdio>

// ─────────────────────────────────────────────────────────────────────────────
// Static helpers
// ─────────────────────────────────────────────────────────────────────────────

static const char* const k_levelTags[] = {
    "VERBOSE", "DEBUG", "INFO", "WARN", "ERROR", "OFF"
};

const char* Logger::levelTag(LogLevel level)
{
    const int idx = static_cast<int>(level);
    if (idx < 0 || idx > static_cast<int>(LogLevel::Off))
        return "UNKNOWN";
    return k_levelTags[idx];
}

QString Logger::formatEntry(LogLevel level,
                             const QString& module,
                             const QString& message)
{
    return QStringLiteral("[%1] [%2] [%3] %4")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
             QLatin1String(levelTag(level)),
             module,
             message);
}

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────

Logger& Logger::instance()
{
    static Logger s_instance;
    return s_instance;
}

Logger::Logger() = default;

Logger::~Logger()
{
    QMutexLocker lock(&m_mutex);
    if (m_logFile.isOpen())
        m_logFile.close();
}

// ─────────────────────────────────────────────────────────────────────────────
// Runtime configuration
// ─────────────────────────────────────────────────────────────────────────────

void Logger::setLogLevel(LogLevel level)
{
    m_levelValue.storeRelease(static_cast<int>(level));
}

LogLevel Logger::logLevel() const
{
    return static_cast<LogLevel>(m_levelValue.loadAcquire());
}

void Logger::setFileLogging(bool enabled, const QString& path)
{
#ifdef Q_OS_WASM
    // File-system logging is not available in the WebAssembly runtime;
    // all output goes to the browser developer console via stdout / stderr.
    Q_UNUSED(enabled)
    Q_UNUSED(path)
#else
    QMutexLocker lock(&m_mutex);

    if (m_logFile.isOpen())
        m_logFile.close();

    m_fileEnabled = enabled;
    if (!path.isEmpty())
        m_filePath = path;

    if (m_fileEnabled)
        openLogFile();
#endif
}

bool Logger::isFileLoggingEnabled() const
{
    QMutexLocker lock(&m_mutex);
    return m_fileEnabled;
}

QString Logger::logFilePath() const
{
    QMutexLocker lock(&m_mutex);
    return m_filePath;
}

QString Logger::defaultLogFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/MaximumTrainer.log");
}

bool Logger::hasDebugSwitch(const QStringList& args)
{
    return args.contains(QStringLiteral("--debug"), Qt::CaseInsensitive)
        || args.contains(QStringLiteral("/debug"),  Qt::CaseInsensitive);
}

bool Logger::openLogFile()
{
    // Caller must hold m_mutex.
    if (m_filePath.isEmpty())
        m_filePath = defaultLogFilePath();

    // Ensure the parent directory exists for both the default path and any
    // user-configured custom path.  mkpath() is a no-op when the directory
    // already exists, so this is safe to call unconditionally and covers all
    // supported target OSes (Windows, macOS, Linux) via Qt's cross-platform
    // QDir API.  The WASM build never reaches this code (guarded by the
    // Q_OS_WASM check in setFileLogging()).
    const QString parentDir = QFileInfo(m_filePath).absolutePath();
    if (!QDir().mkpath(parentDir)) {
        fprintf(stderr, "[Logger] Cannot create log directory: %s — "
                "check that the path is valid and you have write permission.\n",
                qPrintable(parentDir));
        m_fileEnabled = false;
        return false;
    }

    m_logFile.setFileName(m_filePath);
    // WriteOnly ensures O_CREAT semantics on all platforms (Linux, macOS,
    // Windows) so the file is created if it does not already exist.
    // Append prevents truncation of existing content.
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        fprintf(stderr, "[Logger] Cannot open log file: %s\n",
                qPrintable(m_filePath));
        m_fileEnabled = false;
        return false;
    }
    return true;
}

bool Logger::openFileAt(const QString& path)
{
#ifdef Q_OS_WASM
    Q_UNUSED(path)
    return false;
#else
    QMutexLocker lock(&m_mutex);
    if (m_logFile.isOpen())
        m_logFile.close();
    m_filePath = path;
    m_fileEnabled = true;
    return openLogFile();
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Startup bootstrap
// ─────────────────────────────────────────────────────────────────────────────

QString Logger::bootstrap(const QStringList& args,
                          const QString& preferredPath,
                          const QString& fallbackDir)
{
    if (hasDebugSwitch(args)) {
        m_debugSwitch = true;
        setLogLevel(LogLevel::Debug);
    }

#ifndef Q_OS_WASM
    const QString primary = preferredPath.isEmpty() ? defaultLogFilePath()
                                                    : preferredPath;
    if (!openFileAt(primary)) {
        const QString dir = fallbackDir.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            : fallbackDir;
        const QString fallback = dir + QStringLiteral("/MaximumTrainer.log");
        if (openFileAt(fallback)) {
            log(LogLevel::Warn, QStringLiteral("Logger"),
                QStringLiteral("Cannot write log file at %1 — falling back to %2")
                    .arg(QDir::toNativeSeparators(primary),
                         QDir::toNativeSeparators(fallback)));
        } else {
            log(LogLevel::Error, QStringLiteral("Logger"),
                QStringLiteral("Cannot write log file at %1 or %2 — logging to stdout only")
                    .arg(QDir::toNativeSeparators(primary),
                         QDir::toNativeSeparators(fallback)));
        }
    }
#else
    Q_UNUSED(preferredPath)
    Q_UNUSED(fallbackDir)
#endif

    writeBanner(args);
    return isFileLoggingEnabled() ? logFilePath() : QString();
}

void Logger::writeBanner(const QStringList& args)
{
    const QString mod = QStringLiteral("Startup");
    log(LogLevel::Info, mod,
        QStringLiteral("==== %1 %2 starting ====")
            .arg(QCoreApplication::applicationName(),
                 QCoreApplication::applicationVersion()));
    log(LogLevel::Info, mod,
        QStringLiteral("Qt runtime %1 (built against %2)")
            .arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR)));
    log(LogLevel::Info, mod,
        QStringLiteral("OS: %1 (kernel %2 %3)")
            .arg(QSysInfo::prettyProductName(),
                 QSysInfo::kernelType(), QSysInfo::kernelVersion()));
    log(LogLevel::Info, mod,
        QStringLiteral("CPU architecture: %1 (build %2)")
            .arg(QSysInfo::currentCpuArchitecture(), QSysInfo::buildAbi()));
    log(LogLevel::Info, mod,
        QStringLiteral("Command line: %1").arg(args.join(QLatin1Char(' '))));

    const QString logPath = isFileLoggingEnabled()
        ? QDir::toNativeSeparators(logFilePath())
        : QStringLiteral("<none — stdout only>");
    log(LogLevel::Info, mod, QStringLiteral("Log file: %1").arg(logPath));

    const QSettings settings;
    QString backend = QStringLiteral("native");
    if (settings.format() == QSettings::IniFormat)
        backend = QStringLiteral("ini");
#if defined(Q_OS_WIN)
    else if (settings.format() == QSettings::NativeFormat)
        backend = QStringLiteral("registry");
#elif defined(Q_OS_MACOS)
    else if (settings.format() == QSettings::NativeFormat)
        backend = QStringLiteral("plist");
#endif
    log(LogLevel::Info, mod,
        QStringLiteral("Settings: %1 at %2")
            .arg(backend, QDir::toNativeSeparators(settings.fileName())));
    log(LogLevel::Info, mod,
        QStringLiteral("Log level: %1%2")
            .arg(QLatin1String(levelTag(logLevel())),
                 m_debugSwitch ? QStringLiteral(" (--debug)") : QString()));
}

void Logger::emergencyLog(const QString& message)
{
    const QByteArray utf8 =
        formatEntry(LogLevel::Error, QStringLiteral("Fatal"), message).toUtf8();
    fprintf(stderr, "%s\n", utf8.constData());
    fflush(stderr);
#ifndef Q_OS_WASM
    if (m_mutex.tryLock(200)) {
        if (m_fileEnabled && m_logFile.isOpen()) {
            m_logFile.write(utf8);
            m_logFile.write("\n", 1);
            m_logFile.flush();
        }
        m_mutex.unlock();
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// QSettings persistence
// ─────────────────────────────────────────────────────────────────────────────

void Logger::loadConfig()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("logging"));

    const int level = settings.value(QStringLiteral("level"),
                                     static_cast<int>(LogLevel::Info)).toInt();
    const bool fileOn = settings.value(QStringLiteral("file_enabled"),
                                       true).toBool();
    const QString fp  = settings.value(QStringLiteral("file_path"),
                                       QString()).toString();
    settings.endGroup();

    int clamped = qBound(static_cast<int>(LogLevel::Verbose),
                         level,
                         static_cast<int>(LogLevel::Off));
    if (m_debugSwitch)
        clamped = qMin(clamped, static_cast<int>(LogLevel::Debug));
    setLogLevel(static_cast<LogLevel>(clamped));

#ifndef Q_OS_WASM
    if (!fileOn) {
        if (isFileLoggingEnabled())
            log(LogLevel::Info, QStringLiteral("Logger"),
                QStringLiteral("File logging disabled by user preference"));
        setFileLogging(false);
        return;
    }

    const QString currentPath = logFilePath();
    const bool currentOpen = isFileLoggingEnabled();
    if (fp.isEmpty() || (currentOpen && QFileInfo(fp) == QFileInfo(currentPath))) {
        if (!currentOpen)
            setFileLogging(true);
        return;
    }

    if (currentOpen)
        log(LogLevel::Info, QStringLiteral("Logger"),
            QStringLiteral("Switching log file to %1 (logging/file_path)")
                .arg(QDir::toNativeSeparators(fp)));
    if (openFileAt(fp))
        return;

    // The saved path is unusable; keep (or restore) the file we already had.
    openFileAt(currentPath.isEmpty() ? defaultLogFilePath() : currentPath);
    log(LogLevel::Warn, QStringLiteral("Logger"),
        QStringLiteral("Cannot write log file at %1 — continuing with %2")
            .arg(QDir::toNativeSeparators(fp),
                 QDir::toNativeSeparators(logFilePath())));
#else
    Q_UNUSED(fileOn)
    Q_UNUSED(fp)
#endif
}

void Logger::saveConfig() const
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("logging"));
    settings.setValue(QStringLiteral("level"),
                      m_levelValue.loadAcquire());

    QMutexLocker lock(&m_mutex);
    settings.setValue(QStringLiteral("file_enabled"), m_fileEnabled);
    settings.setValue(QStringLiteral("file_path"),    m_filePath);
    settings.endGroup();
}

// ─────────────────────────────────────────────────────────────────────────────
// Core log function
// ─────────────────────────────────────────────────────────────────────────────

void Logger::log(LogLevel level, const QString& module, const QString& message)
{
    if (level < logLevel())
        return;

    const QString entry = formatEntry(level, module, message);
    writeToSinks(entry, level);
}

void Logger::writeToSinks(const QString& formatted, LogLevel level)
{
    const QByteArray utf8 = formatted.toUtf8();

#ifdef Q_OS_WASM
    // On WebAssembly:
    //   stdout  → browser console.log  (visible in DevTools "Console" tab)
    //   stderr  → browser console.error (shown in red)
    if (level >= LogLevel::Warn) {
        fprintf(stderr, "%s\n", utf8.constData());
    } else {
        fprintf(stdout, "%s\n", utf8.constData());
    }
#else
    // Desktop: always write to stdout; additionally to stderr for errors.
    fprintf(stdout, "%s\n", utf8.constData());
    if (level >= LogLevel::Error)
        fprintf(stderr, "%s\n", utf8.constData());

    // Optional file sink — write then flush immediately.
    QMutexLocker lock(&m_mutex);
    if (m_fileEnabled && m_logFile.isOpen()) {
        m_logFile.write(utf8);
        m_logFile.write("\n", 1);
        m_logFile.flush();
    }
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Qt message-handler integration
// ─────────────────────────────────────────────────────────────────────────────

void Logger::install()
{
    qInstallMessageHandler(Logger::qtMessageHandler);
}

void Logger::qtMessageHandler(QtMsgType type,
                               const QMessageLogContext& context,
                               const QString& msg)
{
    LogLevel level;
    switch (type) {
    case QtDebugMsg:    level = LogLevel::Debug;  break;
    case QtInfoMsg:     level = LogLevel::Info;   break;
    case QtWarningMsg:  level = LogLevel::Warn;   break;
    case QtCriticalMsg: level = LogLevel::Error;  break;
    case QtFatalMsg:    level = LogLevel::Error;  break;
    default:            level = LogLevel::Info;   break;
    }

    // Derive a short module name from the source file (e.g. "btle_hub.cpp").
    QString module;
    if (context.file && *context.file) {
        module = QString::fromUtf8(context.file);
        const int slash = qMax(module.lastIndexOf(QLatin1Char('/')),
                               module.lastIndexOf(QLatin1Char('\\')));
        if (slash >= 0)
            module = module.mid(slash + 1);
    } else {
        module = QStringLiteral("Qt");
    }

    Logger::instance().log(level, module, msg);

    if (type == QtFatalMsg)
        abort();
}
