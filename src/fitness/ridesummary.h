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

    /// \a zoneBoundsPct come from PowerZones::boundsPct (empty means Coggan).
    static RideSummary build(const WorkoutHistorySummary &summary, const WorkoutHistoryDetail &detail,
                             int ftpWatts, const QVector<int> &zoneBoundsPct = {});
};

/// How this ride compares with the last ride of the same workout.
struct RideComparison
{
    bool      valid           = false;
    QDateTime previousStart;
    bool      hasPower        = false;
    bool      hasHeartRate    = false;
    int       npDeltaW        = 0;
    int       avgPowerDeltaW  = 0;
    int       hrDeltaBpm      = 0;
    double    tssDelta        = 0.0;

    /// The most recent earlier ride in \a historyDir of the same workout as
    /// \a currentFile, at least 80 % as long (so an abandoned attempt is not
    /// used); empty when there is none.
    static QString previousRideFile(const QString &historyDir, const QString &currentFile,
                                    int currentDurationSec);
    static RideComparison compare(const WorkoutHistorySummary &current,
                                  const WorkoutHistorySummary &previous);
};

#endif // RIDESUMMARY_H
