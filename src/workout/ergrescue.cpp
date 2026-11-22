#include "ergrescue.h"

#include <algorithm>

int ErgRescue::stallRpm(int cadenceTarget)
{
    if (cadenceTarget <= 0)
        return kStallRpm;
    return std::max(kMinStallRpm, std::min(kStallRpm, cadenceTarget - kBelowCadenceTarget));
}

ErgRescue::Action ErgRescue::addSecond(bool ergWanted, double cadence, double power,
                                       double targetWatts, int cadenceTarget, bool mayRelease)
{
    const int stall = stallRpm(cadenceTarget);
    if (!m_active) {
        if (!ergWanted || !mayRelease || cadence < 0) {
            m_lowSeconds = 0;
            return Action::None;
        }
        m_lowSeconds = cadence < stall ? m_lowSeconds + 1 : 0;
        if (m_lowSeconds < kSeconds)
            return Action::None;
        m_active = true;
        m_lowSeconds = 0;
        m_recoveredSeconds = 0;
        return Action::Release;
    }

    // The interval no longer wants ERG (rest, slope, end): nothing to hold back.
    if (!ergWanted) {
        reset();
        return Action::Resume;
    }
    const bool recovered = cadence >= stall + kResumeAboveStallRpm
                        && power >= kResumePowerShare * targetWatts;
    m_recoveredSeconds = recovered ? m_recoveredSeconds + 1 : 0;
    if (m_recoveredSeconds < kSeconds)
        return Action::None;
    reset();
    return Action::Resume;
}

void ErgRescue::release()
{
    m_active = true;
    m_lowSeconds = 0;
    m_recoveredSeconds = 0;
}

void ErgRescue::reset()
{
    m_active = false;
    m_lowSeconds = 0;
    m_recoveredSeconds = 0;
}
