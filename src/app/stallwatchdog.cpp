#include "stallwatchdog.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTimer>

#include <atomic>
#include <chrono>
#include <thread>

#include "logger.h"

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <csignal>
#include <cstdlib>
#include <execinfo.h>
#include <pthread.h>
#define MT_STALL_STACKS 1
#endif

namespace {
QElapsedTimer g_clock;
std::atomic<qint64> g_lastBeatMs{-1};

#ifdef MT_STALL_STACKS
constexpr int kMaxFrames = 48;
pthread_t g_uiThread;
void *g_frames[kMaxFrames];
std::atomic<int> g_frameCount{0};

void captureUiStack(int)
{
    g_frameCount = backtrace(g_frames, kMaxFrames);
}

QString uiStackText()
{
    g_frameCount = 0;
    pthread_kill(g_uiThread, SIGUSR2);
    for (int waited = 0; waited < 50 && g_frameCount == 0; ++waited)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const int count = g_frameCount;
    if (count <= 0)
        return QStringLiteral("(no stack)");
    // Skip the signal handler and the signal trampoline.
    char **symbols = backtrace_symbols(g_frames, count);
    QString text;
    for (int index = 2; index < count; ++index)
        text += QStringLiteral("\n    ") + QString::fromLocal8Bit(symbols[index]);
    std::free(static_cast<void *>(symbols));
    return text;
}
#endif
} // namespace

void StallWatchdog::start(int thresholdMs)
{
#ifndef Q_OS_WASM
    new StallWatchdog(thresholdMs, QCoreApplication::instance());
#else
    Q_UNUSED(thresholdMs)
#endif
}

StallWatchdog::StallWatchdog(int thresholdMs, QObject *parent) : QObject(parent)
{
    g_clock.start();
#ifdef MT_STALL_STACKS
    g_uiThread = pthread_self();
    void *warmUp[1];
    backtrace(warmUp, 1);   // loads the unwinder now, not inside the handler
    std::signal(SIGUSR2, captureUiStack);
#endif
    auto *beat = new QTimer(this);
    beat->setTimerType(Qt::PreciseTimer);
    beat->setInterval(10);
    connect(beat, &QTimer::timeout, this, [] { g_lastBeatMs = g_clock.elapsed(); });
    beat->start();
    g_lastBeatMs = g_clock.elapsed();
    LOG_INFO("StallCheck", QStringLiteral("Watching the UI thread, reporting stalls over %1 ms").arg(thresholdMs));

#ifndef Q_OS_WASM
    std::thread([thresholdMs] {
        qint64 stalledSinceBeat = -1;
        for (;;) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            const qint64 lastBeat = g_lastBeatMs.load();
            const qint64 now = g_clock.elapsed();
            if (stalledSinceBeat < 0 && now - lastBeat > thresholdMs) {
                stalledSinceBeat = lastBeat;
                QString message = QStringLiteral("UI thread stalled at %1 ms").arg(lastBeat);
#ifdef MT_STALL_STACKS
                message += QStringLiteral(", stack:") + uiStackText();
#endif
                LOG_WARN("StallCheck", message);
            } else if (stalledSinceBeat >= 0 && lastBeat != stalledSinceBeat) {
                LOG_WARN("StallCheck", QStringLiteral("UI thread stall lasted %1 ms").arg(lastBeat - stalledSinceBeat));
                stalledSinceBeat = -1;
            }
        }
    }).detach();
#endif
}
