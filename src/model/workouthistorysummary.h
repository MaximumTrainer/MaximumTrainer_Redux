#ifndef WORKOUTHISTORYSUMMARY_H
#define WORKOUTHISTORYSUMMARY_H

#include <QString>
#include <QDateTime>

struct WorkoutHistorySummary
{
    QString   filePath;
    QDateTime startTime;
    QString   workoutName;
    int       durationSec    = 0;
    int       avgPowerW      = 0;
    int       normalizedPower= 0;
    int       avgHrBpm       = 0;
    int       avgCadence     = 0;
    double    tss            = 0.0;
    double    totalDistanceKm= 0.0;
    int       calories       = 0;
    bool      valid          = false;
};

#endif // WORKOUTHISTORYSUMMARY_H
