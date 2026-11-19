#include "targetalert.h"

bool TargetAlert::ready(qint64 lastMs, qint64 nowMs) const
{
    return lastMs < 0 || nowMs - lastMs >= kCooldownMs;
}

TargetAlert::Breach TargetAlert::check(double value, double target, double range,
                                       bool alertLow, bool alertHigh, qint64 nowMs)
{
    if (alertLow && value < target - range && ready(m_lastLowMs, nowMs)) {
        m_lastLowMs = nowMs;
        return Breach::Low;
    }
    if (alertHigh && value > target + range && ready(m_lastHighMs, nowMs)) {
        m_lastHighMs = nowMs;
        return Breach::High;
    }
    return Breach::None;
}
