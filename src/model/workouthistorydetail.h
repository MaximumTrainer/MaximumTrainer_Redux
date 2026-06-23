#ifndef WORKOUTHISTORYDETAIL_H
#define WORKOUTHISTORYDETAIL_H

#include <QVector>

/// One per-second (or per-record) sample re-read from a stored activity FIT file.
/// Missing channels are left at their default (a negative sentinel for "absent").
struct ActivityRecordPoint
{
    int    elapsedSec = 0;
    int    power      = -1;
    int    heartRate  = -1;
    int    cadence    = -1;
    double distanceKm = -1.0;
};

/// One lap / interval summary re-read from a stored activity FIT file.
struct ActivityLap
{
    int    durationSec     = 0;
    int    avgPowerW       = 0;
    int    maxPowerW       = 0;
    int    normalizedPower = 0;
    int    avgHrBpm        = 0;
    int    maxHrBpm        = 0;
    int    avgCadence      = 0;
    double distanceKm      = 0.0;
};

/// Full drill-down detail for a single stored activity: the time-series records
/// (for the power/HR/cadence graph) plus per-lap summaries.
struct WorkoutHistoryDetail
{
    QVector<ActivityRecordPoint> records;
    QVector<ActivityLap>         laps;
    bool                         valid = false;

    bool hasPower()     const { for (const auto &r : records) if (r.power     >= 0) return true; return false; }
    bool hasHeartRate() const { for (const auto &r : records) if (r.heartRate >= 0) return true; return false; }
    bool hasCadence()   const { for (const auto &r : records) if (r.cadence   >= 0) return true; return false; }
};

#endif // WORKOUTHISTORYDETAIL_H
