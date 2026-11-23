#ifndef CYCLINGPHYSICS_H
#define CYCLINGPHYSICS_H

/*
 * Power -> speed, the standard flat-road cycling power balance:
 *
 *     F_drive   = P / v
 *     F_resist  = Crr*m*g  +  1/2*rho*CdA*v^2      (grade omitted for now)
 *     a         = (F_drive - F_resist) / m
 *     v        += a*dt
 */
namespace CyclingPhysics {

constexpr double kGravity    = 9.8067;
constexpr double kCrr        = 0.005;
constexpr double kAirDensity = 1.275;

inline double stepSpeed(double vMps, double powerW, double weightKg,
                        double cda, double dtSec)
{
    const double v = (vMps < 0.5) ? 0.5 : vMps;   // floor avoids P/v blow-up at rest
    const double fResist = kCrr * kGravity * weightKg
                         + 0.5 * kAirDensity * cda * v * v;
    const double fDrive  = powerW / v;
    const double accel   = (fDrive - fResist) / weightKg;
    const double next    = vMps + accel * dtSec;
    return next < 0.0 ? 0.0 : next;
}

} // namespace CyclingPhysics

#endif // CYCLINGPHYSICS_H
