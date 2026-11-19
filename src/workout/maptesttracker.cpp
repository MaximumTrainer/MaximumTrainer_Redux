#include "maptesttracker.h"

#include <QtMath>

bool MapTestTracker::recordSecond(bool scored, double averagePower, double targetPower, double targetRange)
{
    m_offTargetThisSecond = scored && averagePower < targetPower - targetRange;
    if (m_offTargetThisSecond) {
        ++m_secondsOffTarget;
        ++m_consecutiveOffTarget;
    } else {
        m_consecutiveOffTarget = 0;
    }
    return m_secondsOffTarget >= kMaxSecondsOffTarget || m_consecutiveOffTarget >= kMaxConsecutiveOffTarget;
}

int MapTestTracker::completeStep()
{
    m_secondsOffTarget = 0;
    return m_step++;
}

double MapTestTracker::mapWatts(double failedStepTargetWatts, int secondsCompletedInStep)
{
    return (failedStepTargetWatts - 30) + (secondsCompletedInStep / double(kStepSeconds) * failedStepTargetWatts / 10.0);
}

int MapTestTracker::estimatedFtp(double mapWatts)
{
    return qRound(mapWatts * 0.75);
}
