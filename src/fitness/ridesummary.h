#ifndef RIDESUMMARY_H
#define RIDESUMMARY_H

#include <QVector>

#include "workouthistorydetail.h"
#include "workouthistorysummary.h"

/// What the end-of-workout panel shows about the ride just finished, built from
/// the same FIT readers as History so the numbers match it.
struct RideSummary
{
    int    durationSec     = 0;
    int    avgPowerW       = 0;
    int    normalizedPower = 0;
    int    maxPowerW       = 0;
    double intensityFactor = 0.0;
    double tss             = 0.0;
    int    avgHrBpm        = 0;
    int    maxHrBpm        = 0;
    int    avgCadence      = 0;
    int    calories        = 0;
    double distanceKm      = 0.0;
    QVector<int> zoneSeconds;   ///< seconds per power zone, Z1 first
    bool   hasPower        = false;
    bool   valid           = false;

    int zoneTotalSeconds() const;

    /// \a zoneUpperWatts are the athlete's zone upper bounds (Intervals.icu);
    /// empty means the standard 7 Coggan zones from \a ftpWatts.
    static RideSummary build(const WorkoutHistorySummary &summary, const WorkoutHistoryDetail &detail,
                             int ftpWatts, const QVector<int> &zoneUpperWatts = {});
    /// Upper bounds actually used: \a zoneUpperWatts without its open-ended
    /// top value (Intervals.icu ends with 999 % of FTP), or Coggan's
    /// 55/75/90/105/120/150 % of FTP. N bounds make N + 1 zones.
    static QVector<int> zoneBounds(int ftpWatts, const QVector<int> &zoneUpperWatts);
    /// Zone index for \a watts: the first bound it does not exceed.
    static int powerZone(int watts, const QVector<int> &bounds);
};

#endif // RIDESUMMARY_H
