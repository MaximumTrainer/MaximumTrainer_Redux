#ifndef STARTUP_TRACE_H
#define STARTUP_TRACE_H

#include <QElapsedTimer>
#include <atomic>
#include <exception>

#include "logger.h"

///
/// Logs "<label> … begin" when constructed and "<label> … done (N ms)" when
/// destroyed, under the "Startup" module.  The begin line is flushed before the
/// step runs, so after a hang or crash the last line of the log names the step
/// that was in progress.  @p label must be a string literal (it is kept by
/// pointer for the crash handlers).
///
class StartupStep
{
public:
    explicit StartupStep(const char *label)
        : m_label(label),
          m_previous(s_current.exchange(label)),
          m_uncaughtOnEntry(std::uncaught_exceptions())
    {
        Logger::instance().log(LogLevel::Info, QStringLiteral("Startup"),
                               QStringLiteral("%1 … begin").arg(QLatin1String(m_label)));
        m_timer.start();
    }

    ~StartupStep()
    {
        const qint64 ms = m_timer.elapsed();
        if (std::uncaught_exceptions() > m_uncaughtOnEntry)
            Logger::instance().log(LogLevel::Error, QStringLiteral("Startup"),
                                   QStringLiteral("%1 … FAILED after %2 ms (exception)")
                                       .arg(QLatin1String(m_label)).arg(ms));
        else
            Logger::instance().log(LogLevel::Info, QStringLiteral("Startup"),
                                   QStringLiteral("%1 … done (%2 ms)")
                                       .arg(QLatin1String(m_label)).arg(ms));
        s_current.store(m_previous);
    }

    /// Label of the innermost step in progress, or nullptr outside any step.
    static const char *current() { return s_current.load(); }

    StartupStep(const StartupStep &) = delete;
    StartupStep &operator=(const StartupStep &) = delete;

private:
    const char *m_label;
    const char *m_previous;
    int m_uncaughtOnEntry;
    QElapsedTimer m_timer;
    static inline std::atomic<const char *> s_current { nullptr };
};

namespace StartupTrace {
/// std::set_terminate on every platform, plus SetUnhandledExceptionFilter on
/// Windows: write one final "fatal: … during <step>" line before the process
/// dies.
void installCrashHandlers();
}

#endif // STARTUP_TRACE_H
