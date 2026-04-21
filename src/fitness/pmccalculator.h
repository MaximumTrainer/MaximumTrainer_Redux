#ifndef PMCCALCULATOR_H
#define PMCCALCULATOR_H

#include <QDate>
#include <QList>

#include "workouthistorysummary.h"

/// One day's entry on the Performance Management Chart.
struct PmcPoint
{
    QDate  date;
    double tss = 0.0;   ///< Total Training Stress for the day
    double ctl = 0.0;   ///< Chronic Training Load  (42-day EMA, "fitness")
    double atl = 0.0;   ///< Acute Training Load    (7-day EMA, "fatigue")
    double tsb = 0.0;   ///< Training Stress Balance = CTL[prev] - ATL[prev] ("form")
};

/// Computes CTL, ATL and TSB from a list of workout summaries.
///
/// Uses the standard exponential moving averages:
///   CTL[d] = CTL[d-1] + (TSS[d] - CTL[d-1]) * (1 - exp(-1/42))
///   ATL[d] = ATL[d-1] + (TSS[d] - ATL[d-1]) * (1 - exp(-1/7))
///   TSB[d] = CTL[d-1] - ATL[d-1]
class PmcCalculator
{
public:
    /// Compute the PMC curve from \a history.
    ///
    /// The returned list spans from the earliest activity date (or \a from,
    /// whichever is earlier) to \a to (inclusive), one entry per calendar day.
    ///
    /// \param history   Parsed workout summaries (any order).
    /// \param to        Last date to include.  Defaults to today.
    /// \param leadInDays Number of days before the first activity to start the
    ///                  computation (with TSS=0) so that CTL/ATL have time to
    ///                  warm up.  Default: 42.
    static QList<PmcPoint> compute(const QList<WorkoutHistorySummary> &history,
                                   const QDate &to = QDate::currentDate(),
                                   int leadInDays = 42);

private:
    static constexpr double kCtlTc = 42.0;
    static constexpr double kAtlTc =  7.0;
};

#endif // PMCCALCULATOR_H
