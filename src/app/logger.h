/*
 * logger.h
 *
 * Unified, configurable cross-platform logging framework for MaximumTrainer.
 *
 * Log levels (ordered from most-verbose to most-silent):
 *   Verbose  – Low-level hardware / BLE packet traces
 *   Debug    – Developer diagnostics
 *   Info     – Normal operational messages  (default in release)
 *   Warn     – Recoverable anomalies
 *   Error    – Errors that affect functionality
 *   Off      – Suppress all output
 *
 * Platform sinks:
 *   Desktop (Win / macOS / Linux): stdout + optional rolling log file
 *   WebAssembly:  stdout  → browser console.log
 *                 stderr  → browser console.error  (Warn / Error)
 *
 * Configuration is persisted via QSettings (group "logging"):
 *   logging/level        (int, default 2 = Info)
 *   logging/file_enabled (bool, default true)
 *   logging/file_path    (string, default <AppData>/MaximumTrainer.log)
 *
 * Quick-start:
 *   // At the very top of main(), right after the app identity is set and
 *   // before QApplication / the splash screen:
 *   Logger::instance().bootstrap(args);   // file sink + startup banner
 *   Logger::install();          // route qWarning / qCritical through Logger
 *   // … later, once the configuration has been bootstrapped …
 *   Logger::instance().loadConfig();      // apply saved user preferences
 *
 *   // Anywhere in the codebase:
 *   LOG_INFO ("BtleHub",       "Device connected");
 *   LOG_WARN ("WorkoutDialog", "Unexpected lap transition");
 *   LOG_VERBOSE("BtleHub",    QStringLiteral("raw bytes: ") + hex);
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <QtCore>
#include <QMutex>
#include <QFile>

// ─────────────────────────────────────────────────────────────────────────────
// LogLevel
// ─────────────────────────────────────────────────────────────────────────────
enum class LogLevel : int {
    Verbose = 0,
    Debug   = 1,
    Info    = 2,
    Warn    = 3,
    Error   = 4,
    Off     = 5
};

// ─────────────────────────────────────────────────────────────────────────────
// Logger — process-wide singleton
// ─────────────────────────────────────────────────────────────────────────────
class Logger
{
public:
    /// Returns the process-wide singleton instance.
    static Logger& instance();

    // ── Runtime configuration ─────────────────────────────────────────────────

    /// Set the minimum level; messages below this level are silently discarded.
    void setLogLevel(LogLevel level);
    /// Current minimum level (lockless atomic read — safe to call on hot paths).
    LogLevel logLevel() const;

    /// Enable / disable the log-file sink (no-op on WASM).
    /// @p path   Path of the log file; uses <AppData>/MaximumTrainer.log if empty.
    void setFileLogging(bool enabled, const QString& path = QString());
    bool isFileLoggingEnabled() const;
    QString logFilePath() const;

    /// <AppDataLocation>/MaximumTrainer.log for the current app identity.
    static QString defaultLogFilePath();

    /// True when @p args contain the --debug / /debug switch.
    static bool hasDebugSwitch(const QStringList& args);

    // ── Startup bootstrap ─────────────────────────────────────────────────────

    /// Turn on the file sink before QApplication exists and write the startup
    /// banner.  Requires only the application identity to be set.
    /// @p preferredPath  log file to use; defaults to defaultLogFilePath().
    /// @p fallbackDir    directory used when @p preferredPath cannot be
    ///                   written; defaults to QStandardPaths::TempLocation.
    /// A --debug / /debug switch in @p args applies Debug level immediately
    /// and keeps it through loadConfig().  Never fails: without any writable
    /// location the stdout sink keeps working.
    /// @return the log file path in use (empty when no file could be opened).
    QString bootstrap(const QStringList& args,
                      const QString& preferredPath = QString(),
                      const QString& fallbackDir = QString());

    /// Write the startup banner (version, Qt, OS, arch, args, log path,
    /// settings location) at Info level.
    void writeBanner(const QStringList& args);

    /// Best-effort write of one Error line for crash / terminate handlers.
    /// Never blocks on the logger mutex.
    void emergencyLog(const QString& message);

    // ── Core logging API ──────────────────────────────────────────────────────

    /// Emit one formatted log entry if @p level >= current log level.
    void log(LogLevel level, const QString& module, const QString& message);

    // ── QSettings persistence ─────────────────────────────────────────────────

    /// Load settings from the "logging" QSettings group.
    /// Must be called after QCoreApplication org / app names have been set.
    /// Lines already written are kept: when the saved file_path differs from
    /// the current file, a "switching to <path>" line is written to the
    /// current file first, and a path that cannot be opened is ignored.
    void loadConfig();

    /// Save current settings to the "logging" QSettings group.
    void saveConfig() const;

    // ── Qt message-handler integration ───────────────────────────────────────

    /// Install Logger as the Qt message handler (intercepts qDebug /
    /// qWarning / qCritical / qFatal).  Call once in main() before
    /// QApplication::exec().
    static void install();

private:
    // Qt message handler callback registered via qInstallMessageHandler().
    static void qtMessageHandler(QtMsgType type,
                                 const QMessageLogContext& context,
                                 const QString& msg);

    Logger();
    ~Logger();
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

    // Write the already-formatted entry to all active sinks.
    void writeToSinks(const QString& formatted, LogLevel level);

    // Open (or re-open) m_logFile at m_filePath.  Returns true on success.
    bool openLogFile();

    // Close the current file and open @p path instead.  Caller must NOT hold
    // m_mutex.  Returns false (and leaves file logging off) on failure.
    bool openFileAt(const QString& path);

    // Format a single log entry:  "[timestamp] [LEVEL] [module] message"
    static QString formatEntry(LogLevel level,
                                const QString& module,
                                const QString& message);
    static const char* levelTag(LogLevel level);

    // Level is stored as an atomic int so logLevel() can be called without
    // acquiring the mutex (important for the macro short-circuit check).
    QAtomicInt  m_levelValue { static_cast<int>(LogLevel::Info) };

    // The mutex protects all state below (file handles, flags, path).
    mutable QMutex m_mutex;
    bool        m_fileEnabled { false };
    bool        m_debugSwitch { false };
    QString     m_filePath;
    QFile       m_logFile;
};

// ─────────────────────────────────────────────────────────────────────────────
// Convenience macros
//
// The level check in the macro short-circuits before any QString construction
// when the active log level would discard the message, keeping the hot path
// cost to a single atomic integer comparison.
// ─────────────────────────────────────────────────────────────────────────────
#define LOG_VERBOSE(mod, msg) \
    do { if (Logger::instance().logLevel() <= LogLevel::Verbose) \
             Logger::instance().log(LogLevel::Verbose, QLatin1String(mod), (msg)); } while (false)

#define LOG_DEBUG(mod, msg) \
    do { if (Logger::instance().logLevel() <= LogLevel::Debug) \
             Logger::instance().log(LogLevel::Debug, QLatin1String(mod), (msg)); } while (false)

#define LOG_INFO(mod, msg) \
    do { if (Logger::instance().logLevel() <= LogLevel::Info) \
             Logger::instance().log(LogLevel::Info, QLatin1String(mod), (msg)); } while (false)

#define LOG_WARN(mod, msg) \
    do { if (Logger::instance().logLevel() <= LogLevel::Warn) \
             Logger::instance().log(LogLevel::Warn, QLatin1String(mod), (msg)); } while (false)

#define LOG_ERROR(mod, msg) \
    do { if (Logger::instance().logLevel() <= LogLevel::Error) \
             Logger::instance().log(LogLevel::Error, QLatin1String(mod), (msg)); } while (false)

#endif // LOGGER_H
