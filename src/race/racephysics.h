#ifndef RACEPHYSICS_H
#define RACEPHYSICS_H

#include "cyclingphysics.h"

#include <QVector>

#include <algorithm>
#include <functional>
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

// One wheel at ~1 m: 38 % less drag (Belloli et al. 2016, pedalling riders in a
// wind tunnel; Kyle 1979, coast-down). Sheltered in a group of three or four:
// about 60 %, deep in a peloton over 90 % (Blocken et al.). How it fades out to 10 m is our choice.
constexpr double kMaxDraftReduction = 0.70;   // only a big pack gets here
constexpr double kWheelDraftReduction = 0.38;
constexpr double kDraftRangeM = 10.0;
constexpr double kExtraRiderBonus = 0.07;       // each further rider ahead, at full strength

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

/// The watts needed to hold \a vMps on this road (negative when coasting
/// would already be faster).
inline double steadyPower(double vMps, double massKg, double cda, double grade, double draftReduction)
{
    using namespace CyclingPhysics;
    const double theta = std::atan(grade);
    return vMps * (massKg * kGravity * (kCrr * std::cos(theta) + std::sin(theta))
                   + 0.5 * kAirDensity * cda * (1.0 - draftReduction) * vMps * vMps);
}

/// The watts behind a change of speed over \a dtSec (the inverse of stepSpeed);
/// 0 when the rider is coasting or braking.
inline double powerForStep(double vMps, double nextMps, double massKg, double cda, double grade,
                           double draftReduction, double dtSec)
{
    const double watts = steadyPower(std::max(vMps, 0.5), massKg, cda, grade, draftReduction)
        + massKg * (nextMps - vMps) / dtSec * std::max(vMps, 0.5);
    return std::max(0.0, watts);
}

/// How much of the draft a wheel \a gapM ahead gives: all of it at 1 m or
/// closer, fading in a straight line to none at 10 m.
inline double draftFade(double gapM)
{
    if (gapM <= 0.0 || gapM > kDraftRangeM)
        return 0.0;
    return gapM <= 1.0 ? 1.0 : 1.0 - (gapM - 1.0) / (kDraftRangeM - 1.0);
}

/// Air-drag reduction for a rider, given the gaps (m) to the riders ahead.
/// The closest wheel gives up to 38 % (at 1 m or closer, none at 10 m). Each
/// further rider ahead adds up to 7 %, scaled the same way by its distance,
/// so a line of riders right in front shelters you more. Capped at 70 %.
inline double draftReduction(const QVector<double> &gapsAheadM)
{
    QVector<double> fades;
    for (const double gap : gapsAheadM)
        if (const double fade = draftFade(gap); fade > 0.0)
            fades.append(fade);
    if (fades.isEmpty())
        return 0.0;
    std::sort(fades.begin(), fades.end(), std::greater<>());
    double reduction = kWheelDraftReduction * fades.constFirst();
    for (qsizetype index = 1; index < fades.size(); ++index)
        reduction += kExtraRiderBonus * fades.at(index);
    return std::min(kMaxDraftReduction, reduction);
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
