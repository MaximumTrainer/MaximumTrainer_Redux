#include "ridesummary.h"

#include <QtGlobal>

int RideSummary::zoneTotalSeconds() const
{
    int total = 0;
    for (const int seconds : zoneSeconds)
        total += seconds;
    return total;
}

QVector<int> RideSummary::zoneBounds(int ftpWatts, const QVector<int> &zoneUpperWatts)
{
    const int ftp = qMax(1, ftpWatts);
    QVector<int> bounds;
    for (const int watts : zoneUpperWatts) {
        if (watts > 0 && watts < 3 * ftp)
            bounds.append(watts);
    }
    if (bounds.isEmpty()) {
        for (const int pct : {55, 75, 90, 105, 120, 150})
            bounds.append(qRound(ftp * pct / 100.0));
    }
    return bounds;
}

int RideSummary::powerZone(int watts, const QVector<int> &bounds)
{
    for (int zone = 0; zone < bounds.size(); ++zone) {
        if (watts <= bounds.at(zone))
            return zone;
    }
    return int(bounds.size());
}

RideSummary RideSummary::build(const WorkoutHistorySummary &summary, const WorkoutHistoryDetail &detail,
                               int ftpWatts, const QVector<int> &zoneUpperWatts)
{
    RideSummary ride;
    const QVector<int> bounds = zoneBounds(ftpWatts, zoneUpperWatts);
    ride.zoneSeconds.fill(0, bounds.size() + 1);
    ride.valid           = summary.valid;
    ride.durationSec     = summary.durationSec;
    ride.avgPowerW       = summary.avgPowerW;
    ride.normalizedPower = summary.normalizedPower;
    ride.tss             = summary.tss;
    ride.avgHrBpm        = summary.avgHrBpm;
    ride.avgCadence      = summary.avgCadence;
    ride.calories        = summary.calories;
    ride.distanceKm      = summary.totalDistanceKm;
    if (ftpWatts > 0 && summary.normalizedPower > 0)
        ride.intensityFactor = summary.normalizedPower / double(ftpWatts);

    // Each record counts for the time until the next one (1 s at the end);
    // gaps from a pause are capped so they do not land in a zone.
    constexpr int kMaxGapSec = 5;
    const auto &records = detail.records;
    for (int i = 0; i < records.size(); ++i) {
        const ActivityRecordPoint &point = records.at(i);
        ride.maxHrBpm = qMax(ride.maxHrBpm, point.heartRate);
        if (point.power < 0)
            continue;
        ride.hasPower = true;
        ride.maxPowerW = qMax(ride.maxPowerW, point.power);
        const int gap = i + 1 < records.size() ? records.at(i + 1).elapsedSec - point.elapsedSec : 1;
        ride.zoneSeconds[powerZone(point.power, bounds)] += qBound(0, gap, kMaxGapSec);
    }
    return ride;
}
