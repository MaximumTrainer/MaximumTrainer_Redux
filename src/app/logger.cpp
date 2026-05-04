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

bool Logger::openLogFile()
{
    // Caller must hold m_mutex.
    if (m_filePath.isEmpty()) {
        const QString dir =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        m_filePath = dir + QStringLiteral("/MaximumTrainer.log");
    }

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

    const int clamped = qBound(static_cast<int>(LogLevel::Verbose),
                               level,
                               static_cast<int>(LogLevel::Off));
    setLogLevel(static_cast<LogLevel>(clamped));

    if (fileOn)
        setFileLogging(true, fp);
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
