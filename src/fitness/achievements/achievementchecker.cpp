#include "achievementchecker.h"

#include "interval.h"

AchievementChecker::AchievementChecker()
{

}

AchievementChecker::~AchievementChecker()
{

}


bool AchievementChecker::checkTargetManiac(Workout workout) {


    bool powerTarget = false;
    bool powerBalanceTarget = false;
    bool cadenceTarget = false;
    bool heartRateTarget = false;

    for (const Interval &interval : workout.getLstInterval()) {


        if (interval.getPowerStepType() != Interval::NONE)
            powerTarget = true;

        if (interval.getRightPowerTarget() != -1)
            powerBalanceTarget = true;

        if (interval.getCadenceStepType() != Interval::NONE)
            cadenceTarget = true;

        if (interval.getHRStepType() != Interval::NONE)
            heartRateTarget = true;
    }

    if (powerTarget && powerBalanceTarget && cadenceTarget && heartRateTarget) {
        return true;
    }
    return false;

}
