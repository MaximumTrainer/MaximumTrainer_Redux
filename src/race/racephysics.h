#ifndef RACEPHYSICS_H
#define RACEPHYSICS_H

#include "cyclingphysics.h"

#include <QVector>

#include <algorithm>
#include <cmath>

/*
 * Power -> speed on a graded road with drafting:
 *
 *     F_resist = m*g*(Crr*cos(theta) + sin(theta)) + 1/2*rho*CdA*(1 - draft)*v^2
 *     a        = (P/v - F_resist) / m
 *
 * draft is the fraction of air drag a rider saves by sitting behind others.
 */
namespace RacePhysics {

constexpr double kMaxDraftReduction = 0.40;
constexpr double kWheelDraftReduction = 0.30;   // right on a wheel, ~1 m back
constexpr double kDraftRangeM = 10.0;
constexpr double kExtraRiderBonus = 0.03;       // each further rider ahead in the line

inline double stepSpeed(double vMps, double powerW, double massKg, double cda,
                        double grade, double draftReduction, double dtSec)
{
    using namespace CyclingPhysics;
    const double v = (vMps < 0.5) ? 0.5 : vMps;   // floor avoids P/v blow-up at rest
    const double theta = std::atan(grade);
    const double fGravityRolling = massKg * kGravity * (kCrr * std::cos(theta) + std::sin(theta));
    const double fAir = 0.5 * kAirDensity * cda * (1.0 - draftReduction) * v * v;
    const double accel = (powerW / v - fGravityRolling - fAir) / massKg;
    const double next = vMps + accel * dtSec;
    return next < 0.0 ? 0.0 : next;
}

/// Air-drag reduction for a rider, given the gaps (m) to the riders ahead.
/// Best wheel: 30 % at 1 m or closer, fading to 0 at 10 m. Each further rider
/// ahead within range adds a little, capped at 40 %.
inline double draftReduction(const QVector<double> &gapsAheadM)
{
    double best = 0.0;
    int riders = 0;
    for (const double gap : gapsAheadM) {
        if (gap <= 0.0 || gap > kDraftRangeM)
            continue;
        ++riders;
        const double fade = gap <= 1.0 ? 1.0 : 1.0 - (gap - 1.0) / (kDraftRangeM - 1.0);
        best = std::max(best, kWheelDraftReduction * fade);
    }
    if (riders == 0)
        return 0.0;
    return std::min(kMaxDraftReduction, best + kExtraRiderBonus * (riders - 1));
}

/// The power saved by drafting, as a share of the power needed to ride at
/// \a vMps without it. This is what riders see ("saving 24 %"): large on the
/// flat, small on a climb where air drag matters less. Gravity helping on a
/// descent is left out of the total, so the saving never exceeds the drag
/// reduction itself.
inline double powerSaving(double vMps, double massKg, double cda, double grade, double draftReduction)
{
    using namespace CyclingPhysics;
    if (vMps <= 0.0 || draftReduction <= 0.0)
        return 0.0;
    const double theta = std::atan(grade);
    const double pAir = 0.5 * kAirDensity * cda * vMps * vMps * vMps;
    const double pRolling = massKg * kGravity * kCrr * std::cos(theta) * vMps;
    const double pClimbing = std::max(0.0, massKg * kGravity * std::sin(theta) * vMps);
    return draftReduction * pAir / (pAir + pRolling + pClimbing);
}

} // namespace RacePhysics

#endif // RACEPHYSICS_H
