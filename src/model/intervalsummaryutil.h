#ifndef INTERVALSUMMARYUTIL_H
#define INTERVALSUMMARYUTIL_H

/// Power-adherence classification for the per-interval summary overlay.
/// Pure header-only utility — no Qt dependency — so it is trivially testable.
namespace IntervalSummaryUtil {

    enum class PowerAdherence {
        Met,      ///< avg power within ±5 % of target
        NearMiss, ///< avg power within ±10 % of target
        Missed    ///< avg power outside ±10 % of target
    };

    /// Classify whether avgPower met the targetPower.
    /// Returns Met if targetPower <= 0 (no power-based target).
    inline PowerAdherence classifyPowerAdherence(double avgPower, double targetPower)
    {
        if (targetPower <= 0.0)
            return PowerAdherence::Met;
        const double ratio = avgPower / targetPower;
        if (ratio >= 0.95 && ratio <= 1.05) return PowerAdherence::Met;
        if (ratio >= 0.90 && ratio <= 1.10) return PowerAdherence::NearMiss;
        return PowerAdherence::Missed;
    }

} // namespace IntervalSummaryUtil

#endif // INTERVALSUMMARYUTIL_H
