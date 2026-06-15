#ifndef VIRTUAL_GEAR_H
#define VIRTUAL_GEAR_H

#include <QtGlobal>
#include <QtMath>

/*
 * Virtual shifting gear model (#293). Single source of truth shared by the
 * in-app feature (WorkoutDialog) and the headless feel test (TrainerGearTest).
 *
 * The trainer is driven in ERG (FTMS Set Target Power), but the *target* is
 * recomputed from gear + live cadence so it feels like a real gear rather than
 * a flat power clamp: a harder gear and/or faster pedaling demand more watts.
 *
 * Header-only + pure so it is trivially unit-testable (tests/zwift).
 */
namespace VirtualGear {

constexpr int    Count        = 15;     // number of virtual gears
constexpr double RefCadence   = 85.0;   // cadence at which the base watts apply
constexpr double EasiestCoeff = 0.5;    // gear 1 multiplier
constexpr double HardestCoeff = 2.0;    // gear Count multiplier
constexpr double BaseFtpFrac  = 0.48;   // mid gear @ RefCadence ≈ 0.6×FTP
constexpr double DefaultFtp   = 200.0;  // used when the rider FTP is unknown

// Resistance coefficient for a gear, linear from EasiestCoeff..HardestCoeff.
inline double coeffForGear(int gear)
{
    const int g = qBound(1, gear, Count);
    return EasiestCoeff
         + (g - 1) / double(Count - 1) * (HardestCoeff - EasiestCoeff);
}

// Target watts for a gear at a given cadence and rider FTP. cadence<=0 (no
// reading yet) falls back to RefCadence. ftp<=0 falls back to DefaultFtp.
inline int targetWatts(int gear, double cadence, double ftp)
{
    const double effFtp = (ftp > 0.0) ? ftp : DefaultFtp;
    double cad = (cadence > 0.0) ? cadence : RefCadence;
    cad = qBound(40.0, cad, 120.0);
    // Aero-like: power ∝ cadence² so spinning faster in a gear costs more.
    const double cadFactor = (cad / RefCadence) * (cad / RefCadence);
    const double watts = BaseFtpFrac * effFtp * coeffForGear(gear) * cadFactor;
    return qBound(20, int(qRound(watts)), int(qRound(1.6 * effFtp)));
}

} // namespace VirtualGear

#endif // VIRTUAL_GEAR_H
