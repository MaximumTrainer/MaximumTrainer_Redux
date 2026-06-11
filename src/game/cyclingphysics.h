#ifndef CYCLINGPHYSICS_H
#define CYCLINGPHYSICS_H

#include <utility>

#include "ghostreplay.h"

/*
 * Power → speed, the standard flat-road cycling power balance:
 *
 *     F_drive   = P / v
 *     F_resist  = Crr·m·g  +  ½·ρ·CdA·v²        (grade omitted for now)
 *     a         = (F_drive − F_resist) / m
 *     v        += a·dt
 *
 * Both the live player and the opponent integrate through this same function,
 * so the race is pure effort-vs-effort. (The app's Clock has its own virtual-
 * speed variant in src/ui/components/clock.cpp; we can reconcile to a single
 * shared model when this lands in the workout view.)
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

/*
 * PowerSource — anything that yields applied power (watts) over elapsed ride
 * time. The opponent can be your recorded ride (RecordedPowerSource) or a
 * "computer" pacer (ConstantPowerSource); a WorkoutPacerSource (target interval
 * watts) is the natural third implementation when this reaches the workout view.
 */
class PowerSource
{
public:
    virtual ~PowerSource() = default;
    virtual double powerAt(double elapsedSec) const = 0;
    virtual double totalTimeSec() const { return 0.0; }
    /// Total distance of the route, if finite (a recorded ride). 0 = open-ended
    /// (a constant pacer has no finish line).
    virtual double totalDistanceM() const { return 0.0; }
    /// Pedalling cadence (rpm) at \a elapsedSec, for the leg animation.
    virtual double cadenceAt(double elapsedSec) const { Q_UNUSED(elapsedSec); return 85.0; }
};

class ConstantPowerSource : public PowerSource
{
public:
    explicit ConstantPowerSource(double watts) : m_watts(watts) {}
    double powerAt(double) const override { return m_watts; }
    double watts() const { return m_watts; }
private:
    double m_watts;
};

// Externally-updated power: the workout-target pacer. The dialog pushes the
// current interval's target watts so the pacer follows progressive intervals
// (ramps up/down with the workout) instead of sitting at a flat value.
class LivePowerSource : public PowerSource
{
public:
    explicit LivePowerSource(double watts = 150.0) : m_watts(watts) {}
    void setWatts(double w) { m_watts = w; }
    double powerAt(double) const override { return m_watts; }
private:
    double m_watts;
};

class RecordedPowerSource : public PowerSource
{
public:
    explicit RecordedPowerSource(GhostReplay replay) : m_replay(std::move(replay)) {}
    double powerAt(double t) const override { return m_replay.powerAt(t); }
    double totalTimeSec() const override { return m_replay.totalTimeSec(); }
    double totalDistanceM() const override { return m_replay.totalDistanceM(); }
    double cadenceAt(double t) const override { return m_replay.cadenceAt(t); }
private:
    GhostReplay m_replay;
};

#endif // CYCLINGPHYSICS_H
