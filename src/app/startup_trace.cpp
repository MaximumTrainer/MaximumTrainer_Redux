#include "startup_trace.h"

#include <cstdlib>
#include <iterator>

#ifdef Q_OS_WIN
#include <QDir>
#include <QFileInfo>
#include <windows.h>
#include <dbghelp.h>
#include <dxgi.h>
#include <psapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "dbghelp.lib")
#endif
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
static DWORD s_mainThreadId = 0;
static std::atomic<bool> s_sehReported { false };

/// "Qt6WebEngineCore.dll+0x1a2b3c" for a code address, so a crash names the
/// DLL that faulted (the GPU driver, an injected hook, a Qt module…).
static QString describeAddress(const void *address)
{
    HMODULE module = nullptr;
    if (!address
        || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                   | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCWSTR>(address), &module))
        return QStringLiteral("0x%1 (no module)")
            .arg(reinterpret_cast<quintptr>(address), 0, 16);

    wchar_t path[MAX_PATH];
    const DWORD len = GetModuleFileNameW(module, path, MAX_PATH);
    const QString name = len ? QFileInfo(QString::fromWCharArray(path, int(len))).fileName()
                             : QStringLiteral("?");
    const quintptr offset = reinterpret_cast<quintptr>(address) - reinterpret_cast<quintptr>(module);
    return QStringLiteral("%1+0x%2").arg(name).arg(offset, 0, 16);
}

/// Writes MaximumTrainer-crash.dmp next to the log (the latest crash only, so
/// repeated crashes do not fill the disk).  Returns the path, or "" on failure.
static QString writeMiniDump(EXCEPTION_POINTERS *info)
{
    const QString logPath = Logger::instance().logFilePath();
    const QString dir = logPath.isEmpty() ? QDir::tempPath() : QFileInfo(logPath).absolutePath();
    const QString dumpPath = QDir::toNativeSeparators(dir + QStringLiteral("/MaximumTrainer-crash.dmp"));

    HANDLE file = CreateFileW(reinterpret_cast<LPCWSTR>(dumpPath.utf16()), GENERIC_WRITE, 0,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return QString();

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = info;
    exceptionInfo.ClientPointers = FALSE;
    // Memory referenced from the stacks and the thread/handle tables make the
    // dump debuggable while keeping it at a few MB (no full heap).
    const auto type = static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory
                                                 | MiniDumpWithThreadInfo
                                                 | MiniDumpWithProcessThreadData
                                                 | MiniDumpWithUnloadedModules
                                                 | MiniDumpWithHandleData);
    const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type,
                                      info ? &exceptionInfo : nullptr, nullptr, nullptr);
    CloseHandle(file);
    return ok ? dumpPath : QString();
}

static QString applicationDirectory()
{
    wchar_t path[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return len ? QFileInfo(QString::fromWCharArray(path, int(len))).absolutePath() : QString();
}

constexpr int MaxStackFrames = 48;

// The helpers below run inside __try, so they only use plain C types: a fault
// while walking a corrupted stack must not take the crash report down with it.
static int captureStack(const CONTEXT *crashContext, DWORD64 *pcs, int maxFrames)
{
    int count = 0;
#if defined(_M_X64) || defined(_M_ARM64)
    __try {
        CONTEXT context = *crashContext;
        STACKFRAME64 frame = {};
#if defined(_M_X64)
        const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
        frame.AddrPC.Offset = context.Rip;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrFrame.Offset = context.Rbp;
#else
        const DWORD machine = IMAGE_FILE_MACHINE_ARM64;
        frame.AddrPC.Offset = context.Pc;
        frame.AddrStack.Offset = context.Sp;
        frame.AddrFrame.Offset = context.Fp;
#endif
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;
        while (count < maxFrames
               && StackWalk64(machine, GetCurrentProcess(), GetCurrentThread(), &frame, &context,
                              nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)
               && frame.AddrPC.Offset != 0)
            pcs[count++] = frame.AddrPC.Offset;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
#else
    Q_UNUSED(crashContext);
    Q_UNUSED(pcs);
    Q_UNUSED(maxFrames);
#endif
    return count;
}

struct FrameSymbol {
    char name[256];
    DWORD64 displacement;
    char file[MAX_PATH];
    DWORD line;
    bool hasName;
    bool hasLine;
};

static void symbolize(DWORD64 address, FrameSymbol *out)
{
    out->hasName = false;
    out->hasLine = false;
    __try {
        alignas(SYMBOL_INFO) char buffer[sizeof(SYMBOL_INFO) + sizeof(out->name)] = {};
        SYMBOL_INFO *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = sizeof(out->name) - 1;
        DWORD64 displacement = 0;
        if (SymFromAddr(GetCurrentProcess(), address, &displacement, symbol)) {
            strncpy_s(out->name, symbol->Name, _TRUNCATE);
            out->displacement = displacement;
            out->hasName = true;
        }
        IMAGEHLP_LINE64 line = {};
        line.SizeOfStruct = sizeof(line);
        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(GetCurrentProcess(), address, &lineDisplacement, &line)) {
            strncpy_s(out->file, line.FileName, _TRUNCATE);
            out->line = line.LineNumber;
            out->hasLine = true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

/// One "#N module+0xoffset function+0xNN (file:line)" line per frame.  Our own
/// frames resolve through MaximumTrainer.pdb, which ships next to the exe; Qt
/// and system DLLs fall back to their exported names.
static void logStackTrace(const CONTEXT *crashContext)
{
    if (!crashContext)
        return;
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES
                  | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
    // An explicit search path keeps dbghelp off any symbol server from
    // _NT_SYMBOL_PATH: no network access while the process is dying.
    const QString searchPath = QDir::toNativeSeparators(applicationDirectory());
    const bool symbolsLoaded = SymInitializeW(GetCurrentProcess(),
                                              reinterpret_cast<PCWSTR>(searchPath.utf16()), TRUE);

    DWORD64 pcs[MaxStackFrames];
    const int count = captureStack(crashContext, pcs, MaxStackFrames);
    Logger::instance().emergencyLog(QStringLiteral("stack (%1 frames):").arg(count));
    for (int i = 0; i < count; ++i) {
        // A return address points after the call; look up the call itself.
        const DWORD64 lookup = i == 0 ? pcs[i] : pcs[i] - 1;
        QString line = QStringLiteral("  #%1 %2").arg(i).arg(describeAddress(reinterpret_cast<const void *>(pcs[i])));
        if (symbolsLoaded) {
            FrameSymbol symbol;
            symbolize(lookup, &symbol);
            if (symbol.hasName)
                line += QStringLiteral(" %1+0x%2").arg(QString::fromLocal8Bit(symbol.name))
                                                   .arg(symbol.displacement, 0, 16);
            if (symbol.hasLine)
                line += QStringLiteral(" (%1:%2)").arg(QFileInfo(QString::fromLocal8Bit(symbol.file)).fileName())
                                                   .arg(symbol.line);
        }
        Logger::instance().emergencyLog(line);
    }
    if (symbolsLoaded)
        SymCleanup(GetCurrentProcess());
}

static bool isGraphicsDriverFile(const QString &fileName)
{
    static const char *const prefixes[] = { "nv", "ati", "amd", "igd", "igc", "igx" };
    for (const char *prefix : prefixes)
        if (fileName.startsWith(QLatin1String(prefix)))
            return true;
    return false;
}

/// Lists the DLLs that are neither ours nor Windows': overlays (Discord,
/// RivaTuner, OBS…), antivirus hooks and the GPU driver are the usual suspects
/// when Chromium crashes on one machine only.
static void logForeignModules()
{
    HMODULE modules[1024];
    DWORD bytesNeeded = 0;
    if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &bytesNeeded))
        return;
    const int count = int(qMin<DWORD>(bytesNeeded / sizeof(HMODULE), DWORD(std::size(modules))));

    const QString appDir = QDir::toNativeSeparators(applicationDirectory()).toLower() + QLatin1Char('\\');
    wchar_t windowsPath[MAX_PATH];
    const UINT windowsLen = GetWindowsDirectoryW(windowsPath, MAX_PATH);
    const QString windowsDir = QString::fromWCharArray(windowsPath, int(windowsLen)).toLower() + QLatin1Char('\\');

    int logged = 0;
    for (int i = 0; i < count && logged < 60; ++i) {
        wchar_t path[MAX_PATH];
        const DWORD len = GetModuleFileNameW(modules[i], path, MAX_PATH);
        if (!len)
            continue;
        const QString modulePath = QString::fromWCharArray(path, int(len));
        const QString lower = modulePath.toLower();
        if (lower.startsWith(appDir))
            continue;
        const bool driver = lower.contains(QLatin1String("\\driverstore\\"))
                            || (lower.startsWith(windowsDir)
                                && isGraphicsDriverFile(QFileInfo(lower).fileName()));
        if (lower.startsWith(windowsDir) && !driver)
            continue;
        Logger::instance().emergencyLog(QStringLiteral("%1 module: %2")
            .arg(driver ? QStringLiteral("driver") : QStringLiteral("third-party"), modulePath));
        ++logged;
    }
    if (logged == 0)
        Logger::instance().emergencyLog(QStringLiteral("no driver or third-party modules loaded"));
}

static LONG WINAPI onUnhandledSehException(EXCEPTION_POINTERS *info)
{
    if (s_sehReported.exchange(true))
        return EXCEPTION_CONTINUE_SEARCH;

    const EXCEPTION_RECORD *record = info ? info->ExceptionRecord : nullptr;
    const DWORD code = record ? record->ExceptionCode : 0;
    Logger::instance().emergencyLog(
        QStringLiteral("fatal: unhandled exception 0x%1 during %2")
            .arg(static_cast<qulonglong>(code), 8, 16, QLatin1Char('0'))
            .arg(currentStepDescription()));

    if (record) {
        QString detail = QStringLiteral("fault at %1 on %2 thread %3")
            .arg(describeAddress(record->ExceptionAddress),
                 GetCurrentThreadId() == s_mainThreadId ? QStringLiteral("main")
                                                        : QStringLiteral("worker"))
            .arg(GetCurrentThreadId());
        if (code == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            const ULONG_PTR kind = record->ExceptionInformation[0];
            detail += QStringLiteral(" (%1 of address 0x%2)")
                .arg(kind == 1 ? QStringLiteral("write") : kind == 8 ? QStringLiteral("execute")
                                                                     : QStringLiteral("read"))
                .arg(static_cast<qulonglong>(record->ExceptionInformation[1]), 0, 16);
        }
        Logger::instance().emergencyLog(detail);
    }

    logStackTrace(info ? info->ContextRecord : nullptr);
    logForeignModules();

    const QString dump = writeMiniDump(info);
    Logger::instance().emergencyLog(dump.isEmpty()
        ? QStringLiteral("crash dump could not be written")
        : QStringLiteral("crash dump written to %1").arg(dump));
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void StartupTrace::installCrashHandlers()
{
    std::set_terminate(onTerminate);
#ifdef Q_OS_WIN
    s_mainThreadId = GetCurrentThreadId();
    SetUnhandledExceptionFilter(onUnhandledSehException);
#endif
}

void StartupTrace::logGraphicsAdapters()
{
#ifdef Q_OS_WIN
    using CreateFactoryFn = HRESULT(WINAPI *)(REFIID, void **);
    HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
    const auto createFactory = dxgi ? reinterpret_cast<CreateFactoryFn>(
        reinterpret_cast<void *>(GetProcAddress(dxgi, "CreateDXGIFactory1"))) : nullptr;
    IDXGIFactory1 *factory = nullptr;
    if (!createFactory
        || FAILED(createFactory(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory)))) {
        LOG_WARN("Startup", QStringLiteral("GPU: DXGI unavailable, adapters unknown"));
        return;
    }

    IDXGIAdapter1 *adapter = nullptr;
    for (UINT index = 0; factory->EnumAdapters1(index, &adapter) != DXGI_ERROR_NOT_FOUND; ++index) {
        DXGI_ADAPTER_DESC1 desc;
        if (SUCCEEDED(adapter->GetDesc1(&desc))) {
            QString driver = QStringLiteral("unknown");
            LARGE_INTEGER version;
            if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &version)))
                driver = QStringLiteral("%1.%2.%3.%4")
                             .arg(HIWORD(version.HighPart)).arg(LOWORD(version.HighPart))
                             .arg(HIWORD(version.LowPart)).arg(LOWORD(version.LowPart));
            LOG_INFO("Startup", QStringLiteral("GPU %1: %2 (vendor 0x%3, device 0x%4), driver %5, %6 MB dedicated%7")
                         .arg(index)
                         .arg(QString::fromWCharArray(desc.Description))
                         .arg(desc.VendorId, 4, 16, QLatin1Char('0'))
                         .arg(desc.DeviceId, 4, 16, QLatin1Char('0'))
                         .arg(driver)
                         .arg(quint64(desc.DedicatedVideoMemory) / (1024 * 1024))
                         .arg((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? QStringLiteral(", software")
                                                                       : QString()));
        }
        adapter->Release();
    }
    factory->Release();
#endif
}

Q_NEVER_INLINE void StartupTrace::crashForTesting()
{
    LOG_WARN("Startup", QStringLiteral("MT_CRASH_TEST set: crashing on purpose"));
    volatile int *invalid = nullptr;
    *invalid = 1; // NOLINT(clang-analyzer-core.NullDereference): deliberate crash for the crash-report tooling
}
