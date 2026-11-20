#ifndef STALLWATCHDOG_H
#define STALLWATCHDOG_H

#include <QObject>

/// --stall-check: logs every time the UI thread stops responding for longer
/// than a threshold, with how long it lasted, and (Linux, macOS) the UI
/// thread's stack at the moment of the stall. Diagnostic only, off by default.
class StallWatchdog : public QObject
{
    Q_OBJECT

public:
    static void start(int thresholdMs = 150);

private:
    explicit StallWatchdog(int thresholdMs, QObject *parent);
};

#endif // STALLWATCHDOG_H
