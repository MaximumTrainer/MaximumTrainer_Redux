#include "webengine_safe_mode.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <cstdio>

#ifdef Q_OS_WIN
#include <io.h>
#include <windows.h>
#endif

#include "logger.h"

namespace WebEngineSafeMode {

static void forgetRemembered(QSettings &settings)
{
    settings.remove(QLatin1String(EnabledKey));
    settings.remove(QLatin1String(EnabledVersionKey));
}

Decision decide(QSettings &settings, const QStringList &args, const QByteArray &envValue,
                const QString &appVersion)
{
    Decision decision;
    const bool pending = settings.value(QLatin1String(PendingKey), false).toBool();
    bool remembered = settings.value(QLatin1String(EnabledKey), false).toBool();

    if (remembered && settings.value(QLatin1String(EnabledVersionKey)).toString() != appVersion) {
        forgetRemembered(settings);
        remembered = false;
        decision.retriedAfterUpdate = true;
    }

    if (pending) {
        decision.crashedInSafeMode = remembered;
        if (!remembered) {
            settings.setValue(QLatin1String(EnabledKey), true);
            settings.setValue(QLatin1String(EnabledVersionKey), appVersion);
            remembered = true;
            decision.reason = Reason::PreviousCrash;
        }
    }

    const QByteArray env = envValue.trimmed();
    if (env == "0") {
        forgetRemembered(settings);
        decision.enabled = false;
        decision.reason = Reason::ForcedOff;
    } else if (!env.isEmpty()) {
        decision.enabled = true;
        decision.reason = Reason::Environment;
    } else if (args.contains(QLatin1String(Switch), Qt::CaseInsensitive)) {
        decision.enabled = true;
        decision.reason = Reason::CommandLine;
    } else if (remembered) {
        decision.enabled = true;
        if (decision.reason != Reason::PreviousCrash)
            decision.reason = Reason::Remembered;
    }
    settings.sync();
    return decision;
}

QByteArray chromiumFlags(const Decision &decision, const QByteArray &existingFlags)
{
    QByteArray existing = existingFlags.trimmed();
    if (!decision.enabled || existing.contains("--disable-gpu"))
        return existing;
    return existing.isEmpty() ? QByteArray(ChromiumFlags)
                              : existing + ' ' + QByteArray(ChromiumFlags);
}

bool wantsChromiumLog(const Decision &decision, const QByteArray &logEnvValue)
{
    return logEnvValue.trimmed() == "1"
        || decision.reason == Reason::PreviousCrash
        || decision.crashedInSafeMode;
}

QByteArray withChromiumLogging(const QByteArray &existingFlags)
{
    QByteArray existing = existingFlags.trimmed();
    if (existing.contains("--enable-logging"))
        return existing;
    return existing.isEmpty() ? QByteArray(ChromiumLogFlags)
                              : existing + ' ' + QByteArray(ChromiumLogFlags);
}

#ifdef Q_OS_WIN
static HANDLE s_chromiumLogHandle = INVALID_HANDLE_VALUE;

/// On Windows, QtWebEngine sends Chromium's log to OutputDebugString rather
/// than stderr.  With no debugger attached, OutputDebugString raises
/// DBG_PRINTEXCEPTION_C (the wide variant falls back to it), so a vectored
/// handler sees every message first and copies it to the log file.
static LONG WINAPI copyDebugOutput(EXCEPTION_POINTERS *info)
{
    const EXCEPTION_RECORD *record = info->ExceptionRecord;
    if (record->ExceptionCode == DBG_PRINTEXCEPTION_C && record->NumberParameters >= 2) {
        const auto length = static_cast<DWORD>(record->ExceptionInformation[0]);
        const char *text = reinterpret_cast<const char *>(record->ExceptionInformation[1]);
        if (text && length > 1) {
            DWORD written = 0;
            WriteFile(s_chromiumLogHandle, text, length - 1, &written, nullptr);  // without the NUL
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

/// Points stderr, at both the C runtime and (on Windows) the process level, at
/// @p path, and on Windows also copies OutputDebugString there.  Unbuffered, so
/// the lines written before a crash are kept.
static bool redirectStderr(const QString &path)
{
#ifdef Q_OS_WIN
    // _wfreopen_s would open the file without read sharing, locking the log
    // while the app runs.
#pragma warning(suppress: 4996)
    if (!_wfreopen(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(path).utf16()), L"w", stderr))
        return false;
    s_chromiumLogHandle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stderr)));
    SetStdHandle(STD_ERROR_HANDLE, s_chromiumLogHandle);
    AddVectoredExceptionHandler(0, copyDebugOutput);
#else
    if (!freopen(QFile::encodeName(path).constData(), "w", stderr))
        return false;
#endif
    setvbuf(stderr, nullptr, _IONBF, 0);
    return true;
}

Decision applyAtStartup(const QStringList &args)
{
    QSettings settings;
    const Decision decision = decide(settings, args, qgetenv(EnvVar),
                                     QCoreApplication::applicationVersion());

    if (decision.retriedAfterUpdate && decision.reason != Reason::PreviousCrash)
        LOG_INFO("Startup", QStringLiteral(
            "WebEngine safe graphics was on after an earlier crash; the app has been updated "
            "since, so the GPU is tried again"));

    if (decision.crashedInSafeMode)
        LOG_ERROR("Startup", QStringLiteral(
            "QtWebEngine crashed during the previous launch even with safe graphics on; "
            "the GPU is probably not the cause"));
    if (decision.reason == Reason::PreviousCrash)
        LOG_WARN("Startup", QStringLiteral(
            "QtWebEngine crashed during the previous launch; starting it with safe graphics "
            "(set %1=0 to turn this off)").arg(QLatin1String(EnvVar)));

    if (decision.enabled) {
        const QByteArray flags = chromiumFlags(decision, qgetenv("QTWEBENGINE_CHROMIUM_FLAGS"));
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);
        LOG_INFO("Startup", QStringLiteral("WebEngine safe graphics: on (%1), QTWEBENGINE_CHROMIUM_FLAGS=%2")
                                .arg(reasonName(decision.reason), QString::fromLatin1(flags)));
    } else {
        LOG_INFO("Startup", QStringLiteral("WebEngine safe graphics: off (%1)")
                                .arg(reasonName(decision.reason)));
    }

    const QString appLog = Logger::instance().logFilePath();
    if (wantsChromiumLog(decision, qgetenv(LogEnvVar)) && !appLog.isEmpty()) {
        const QString chromiumLog = QFileInfo(appLog).absolutePath() + QLatin1Char('/')
                                    + QLatin1String(ChromiumLogFile);
        if (redirectStderr(chromiumLog)) {
            qputenv("QTWEBENGINE_CHROMIUM_FLAGS", withChromiumLogging(qgetenv("QTWEBENGINE_CHROMIUM_FLAGS")));
            LOG_INFO("Startup", QStringLiteral("WebEngine log: %1").arg(QDir::toNativeSeparators(chromiumLog)));
        } else {
            LOG_WARN("Startup", QStringLiteral("WebEngine log: could not open %1").arg(chromiumLog));
        }
    }
    return decision;
}

void markInitBegin(QSettings &settings)
{
    settings.setValue(QLatin1String(PendingKey), true);
    settings.sync();
}

void markInitDone(QSettings &settings)
{
    settings.remove(QLatin1String(PendingKey));
    settings.sync();
}

QString reasonName(Reason reason)
{
    switch (reason) {
    case Reason::Off:           return QStringLiteral("default");
    case Reason::PreviousCrash: return QStringLiteral("previous launch crashed in WebEngine init");
    case Reason::Remembered:    return QStringLiteral("remembered from an earlier crash");
    case Reason::CommandLine:   return QStringLiteral("--safe-graphics");
    case Reason::Environment:   return QStringLiteral("%1=1").arg(QLatin1String(EnvVar));
    case Reason::ForcedOff:     return QStringLiteral("%1=0").arg(QLatin1String(EnvVar));
    }
    return QString();
}

} // namespace WebEngineSafeMode
