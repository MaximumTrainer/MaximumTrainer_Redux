#ifndef POWERBESTS_H
#define POWERBESTS_H

#include <QDateTime>
#include <QList>
#include <QString>
#include <QVector>

#include "workouthistorysummary.h"

/// Best average power per duration ("power curve"), per ride and across rides.
namespace PowerBests {

/// Highest average of \a power (one sample per second) over each of
/// \a durations; 0 where the series is shorter than the duration.
QVector<double> bestAverages(const QVector<int> &power, const QVector<int> &durations);

/// One ride's best averages over MMP_DURATIONS.
struct Ride {
    WorkoutHistorySummary summary;
    QVector<double> bests;
};

/// The best effort for one duration and the ride it came from.
struct Effort {
    double watts = 0.0;
    QDateTime date;
    QString workoutName;
    QString filePath;
};

/// For each of MMP_DURATIONS, the best effort among \a rides that started at
/// or after \a since (an invalid \a since means all time).
QVector<Effort> bestEfforts(const QList<Ride> &rides, const QDateTime &since = {});

/// Best averages for each ride in \a history, read from the activity files.
/// Results are cached on disk by file name, size and modification time, so
/// only new or changed rides are decoded. Safe to call from a worker thread.
QList<Ride> loadRides(const QList<WorkoutHistorySummary> &history);

/// Where the cache lives; tests point it at a temporary directory.
QString cacheFilePath();
void setCacheFilePathForTesting(const QString &path);

} // namespace PowerBests

#endif // POWERBESTS_H
