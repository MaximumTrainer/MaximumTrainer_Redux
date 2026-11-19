#ifndef MAPTESTTRACKER_H
#define MAPTESTTRACKER_H

/// The MAP (maximal aerobic power) ramp test: each 3-minute step adds power
/// until the rider fails, i.e. spends 20 seconds of a step, or 10 seconds in a
/// row, below the target range.
class MapTestTracker
{
public:
    static constexpr int kStepSeconds = 180;
    static constexpr int kMaxSecondsOffTarget = 20;
    static constexpr int kMaxConsecutiveOffTarget = 10;

    /// Records one second of a test step; only counts when the second is
    /// scored (the first and last seconds of a step are not). Returns true
    /// when the rider has failed the step, which ends the test.
    bool recordSecond(bool scored, double averagePower, double targetPower, double targetRange);

    /// Called when a step completes; returns the completed step number.
    int completeStep();

    int step() const { return m_step; }
    int secondsOffTarget() const { return m_secondsOffTarget; }
    int consecutiveOffTarget() const { return m_consecutiveOffTarget; }
    bool offTargetThisSecond() const { return m_offTargetThisSecond; }

    /// MAP from the failed step's target and how far into it the rider got.
    static double mapWatts(double failedStepTargetWatts, int secondsCompletedInStep);
    /// FTP estimate from MAP (75%).
    static int estimatedFtp(double mapWatts);

private:
    int m_step = 1;
    int m_secondsOffTarget = 0;
    int m_consecutiveOffTarget = 0;
    bool m_offTargetThisSecond = false;
};

#endif // MAPTESTTRACKER_H
