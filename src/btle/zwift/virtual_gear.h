#ifndef VIRTUAL_GEAR_H
#define VIRTUAL_GEAR_H

#include <QtGlobal>
#include <QtMath>

/*
 * Virtual shifting gear model (#293). Single source of truth for the in-app
 * shifting feature (WorkoutDialog), driven by the ▲/▼ keys / on-screen arrows.
 *
 * The trainer is driven in ERG (FTMS Set Target Power), but the *target* is
 * recomputed from gear + live cadence so it feels like a real gear rather than
 * a flat power clamp: a harder gear and/or faster pedaling demand more watts.
 *
 * Header-only + pure so it is trivially unit-testable (tests/zwift).
 */
namespace VirtualGear {

constexpr int    Count            = 30;  // virtual gears
constexpr int    DefaultStartGear = 8;   // easy gear the first slope interval opens in
constexpr double RefCadence   = 85.0;   // cadence at which the base watts apply
constexpr double EasiestCoeff = 0.10;   // gear 1 multiplier (granny gear ≈ 0.05×FTP)
constexpr double HardestCoeff = 3.0;    // gear Count multiplier (top gear ≈ 1.44×FTP @ ref cadence)
constexpr double BaseFtpFrac  = 0.48;   // mid gear @ RefCadence ≈ 0.6×FTP
constexpr double DefaultFtp   = 200.0;  // used when the rider FTP is unknown

// How far gear is from the easiest (0) to the hardest (1). Steps grow with the
// gear, so the easy end has many close gears for Z1/Z2 and for climbs.
inline double gearPosition(int gear)
{
    const double t = (qBound(1, gear, Count) - 1) / double(Count - 1);
    return t * (0.38 + 0.62 * t);
}

// How much harder (or easier) the road makes every gear: the force against you
// on this grade over the force on the flat at RefSpeedMps. gradePercent is what
// the trainer should simulate (the road grade times the trainer difficulty).
constexpr double RefSpeedMps = 8.3;   // 30 km/h
inline double gradeFactor(double gradePercent, double massKg)
{
    const double flat = massKg * 9.8067 * 0.005 + 0.5 * 1.275 * 0.35 * RefSpeedMps * RefSpeedMps;
    const double hill = massKg * 9.8067 * qSin(qAtan(gradePercent / 100.0));
    return qBound(0.4, (flat + hill) / flat, 4.0);
}

// Resistance coefficient for a gear, from EasiestCoeff to HardestCoeff.
inline double coeffForGear(int gear)
{
    return EasiestCoeff + gearPosition(gear) * (HardestCoeff - EasiestCoeff);
}

// Target watts for a gear at a given cadence and rider FTP. cadence<=0 (no
// reading yet) falls back to RefCadence. ftp<=0 falls back to DefaultFtp.
inline int targetWatts(int gear, double cadence, double ftp, double roadFactor = 1.0)
{
    const double effFtp = (ftp > 0.0) ? ftp : DefaultFtp;
    double cad = (cadence > 0.0) ? cadence : RefCadence;
    cad = qBound(40.0, cad, 120.0);
    // Aero-like: power ∝ cadence² so spinning faster in a gear costs more.
    const double cadFactor = (cad / RefCadence) * (cad / RefCadence);
    const double watts = BaseFtpFrac * effFtp * coeffForGear(gear) * cadFactor * roadFactor;
    // Top gear at race cadence can demand up to ~2×FTP (short grind/surge).
    return qBound(20, int(qRound(watts)), int(qRound(2.0 * effFtp)));
}

// ── Resistance-level shifting (FTMS 0x04) ────────────────────────────────────
// The authentic, Zwift-like feel: each gear is a fixed brake resistance that
// changes instantly (no ERG convergence) and lets power follow effort. The
// value is in the trainer's 0.1-unit resistance representation (2AD6 range).
// Mapped across a usable middle band of a 0..100 range so the extremes
// aren't unrideable; clamped to the trainer's actual [min,max].
constexpr int EasiestResistance = 2;    // gear 1   (0.2) - barely any
constexpr int HardestResistance = 95;   // gear Count (9.5) - hard grind

inline int resistanceLevel(int gear, double roadFactor = 1.0, int minLevel = 0, int maxLevel = 100)
{
    const double level = EasiestResistance + gearPosition(gear) * (HardestResistance - EasiestResistance);
    const int raw = int(qRound(level * roadFactor));
    return qBound(minLevel, raw, maxLevel);
}

} // namespace VirtualGear

#endif // VIRTUAL_GEAR_H
