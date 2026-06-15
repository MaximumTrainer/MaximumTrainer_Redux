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

// ── Resistance-level shifting (FTMS 0x04) ────────────────────────────────────
// The authentic, Zwift-like feel: each gear is a fixed brake resistance that
// changes instantly (no ERG convergence) and lets power follow effort. The
// value is in the trainer's 0.1-unit resistance representation (2AD6 range).
// Mapped linearly across a usable middle band of a 0..100 range so the extremes
// aren't unrideable; clamped to the trainer's actual [min,max].
constexpr int EasiestResistance = 15;   // gear 1   (1.5)
constexpr int HardestResistance = 85;   // gear Count (8.5)

inline int resistanceLevel(int gear, int minLevel = 0, int maxLevel = 100)
{
    const int g = qBound(1, gear, Count);
    const double t = (g - 1) / double(Count - 1);
    const int raw = int(qRound(EasiestResistance + t * (HardestResistance - EasiestResistance)));
    return qBound(minLevel, raw, maxLevel);
}

} // namespace VirtualGear

#endif // VIRTUAL_GEAR_H
