#include "startup_trace.h"

#include <cstdlib>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static QString currentStepDescription()
{
    const char *step = StartupStep::current();
    return step ? QLatin1String(step) : QStringLiteral("normal operation (after startup)");
}

static void onTerminate()
{
    QString what = QStringLiteral("terminate");
    if (const std::exception_ptr ex = std::current_exception()) {
        try {
            std::rethrow_exception(ex);
        } catch (const std::exception &e) {
            what = QStringLiteral("unhandled exception (%1)").arg(QString::fromLocal8Bit(e.what()));
        } catch (...) {
            what = QStringLiteral("unhandled non-standard exception");
        }
    }
    Logger::instance().emergencyLog(
        QStringLiteral("fatal: %1 during %2").arg(what, currentStepDescription()));
    std::abort();
}

#ifdef Q_OS_WIN
static LONG WINAPI onUnhandledSehException(EXCEPTION_POINTERS *info)
{
    const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    Logger::instance().emergencyLog(
        QStringLiteral("fatal: unhandled exception 0x%1 during %2")
            .arg(static_cast<qulonglong>(code), 8, 16, QLatin1Char('0'))
            .arg(currentStepDescription()));
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void StartupTrace::installCrashHandlers()
{
    std::set_terminate(onTerminate);
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(onUnhandledSehException);
#endif
}
