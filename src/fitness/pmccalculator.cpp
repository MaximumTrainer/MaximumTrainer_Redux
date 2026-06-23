#include "pmccalculator.h"

#include <QHash>
#include <cmath>

QList<PmcPoint> PmcCalculator::compute(const QList<WorkoutHistorySummary> &history,
                                       const QDate &to,
                                       int leadInDays)
{
    if (history.isEmpty())
        return {};

    // Build a date → TSS map (sum multiple activities on the same day).
    // Skip entries with an invalid start time (e.g. activities whose FIT file
    // had no session data) — a null QDate would otherwise poison `earliest` and
    // make the day-by-day loop below never terminate.
    QHash<QDate, double> tssByDate;
    QDate earliest;
    for (const WorkoutHistorySummary &s : history) {
        const QDate d = s.startTime.date();
        if (!d.isValid())
            continue;
        tssByDate[d] += s.tss;
        if (!earliest.isValid() || d < earliest)
            earliest = d;
    }

    if (!earliest.isValid())
        return {};

    // Start the computation leadInDays before the first activity so the EMAs
    // have time to warm up from zero.
    const QDate from = earliest.addDays(-leadInDays);

    const double kCtlDecay = 1.0 - std::exp(-1.0 / kCtlTc);
    const double kAtlDecay = 1.0 - std::exp(-1.0 / kAtlTc);

    double ctl = 0.0;
    double atl = 0.0;

    QList<PmcPoint> result;
    result.reserve(from.daysTo(to) + 1);

    for (QDate d = from; d <= to; d = d.addDays(1)) {
        const double tss = tssByDate.value(d, 0.0);

        PmcPoint p;
        p.date = d;
        p.tss  = tss;
        p.tsb  = ctl - atl;   // yesterday's CTL - ATL

        ctl += (tss - ctl) * kCtlDecay;
        atl += (tss - atl) * kAtlDecay;

        p.ctl  = ctl;
        p.atl  = atl;

        result.append(p);
    }

    return result;
}
