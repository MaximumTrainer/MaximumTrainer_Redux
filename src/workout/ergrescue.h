#ifndef ERGRESCUE_H
#define ERGRESCUE_H

/// Gets the rider out of an ERG stall. In ERG the trainer holds the power
/// whatever the cadence, so a rider who slows down has to push harder and
/// slows further until the pedals stop. When cadence stays under the stall
/// threshold, ERG is released (the trainer goes to free ride); it comes back
/// once the rider spins again and makes a fair share of the target.
class ErgRescue
{
public:
    enum class Action { None, Release, Resume };

    static constexpr int kStallRpm = 40;          ///< no cadence target
    static constexpr int kBelowCadenceTarget = 15; ///< low-cadence drills: under their target by this much
    static constexpr int kMinStallRpm = 20;
    static constexpr int kResumeAboveStallRpm = 20;
    static constexpr double kResumePowerShare = 0.5;
    static constexpr int kSeconds = 3;

    /// Cadence under which ERG counts as stalling; \a cadenceTarget <= 0 = none.
    static int stallRpm(int cadenceTarget);

    /// One second of riding. \a ergWanted: ERG would own this interval (power
    /// target, trainer control on). Negative \a cadence = no cadence data.
    /// With \a mayRelease false (spinning up after a start) a stall is not
    /// acted on, but a release already in progress still waits for recovery.
    Action addSecond(bool ergWanted, double cadence, double power, double targetWatts, int cadenceTarget,
                     bool mayRelease = true);
    /// Releases ERG now (the workout was paused); it comes back on recovery.
    void release();

    bool active() const { return m_active; }
    void reset();

private:
    bool m_active = false;
    int m_lowSeconds = 0;
    int m_recoveredSeconds = 0;
};

#endif // ERGRESCUE_H
