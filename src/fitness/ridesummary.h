#ifndef RIDESUMMARY_H
#define RIDESUMMARY_H

#include <QStringList>
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
    /// Watts per heartbeat (NP / avg HR): fitness at the same ERG targets.
    bool      hasEfficiency   = false;
    double    efficiencyDelta = 0.0;
    int       currentFtpW     = 0;   ///< 0 when it cannot be derived
    int       previousFtpW    = 0;

    /// The most recent earlier ride in \a historyDir of the same workout as
    /// \a currentFile, at least 80 % as long (so an abandoned attempt is not
    /// used); empty when there is none.
    static QString previousRideFile(const QString &historyDir, const QString &currentFile,
                                    int currentDurationSec);
    /// Up to \a count such rides, newest first.
    static QStringList previousRideFiles(const QString &historyDir, const QString &currentFile,
                                         int currentDurationSec, int count);
    static RideComparison compare(const WorkoutHistorySummary &current,
                                  const WorkoutHistorySummary &previous);

    /// The FTP a ride was scored with, from TSS = duration x NP^2 / (FTP^2 x 36);
    /// 0 without power or TSS.
    static int impliedFtp(const WorkoutHistorySummary &ride);
    /// NP (or average power) per beat; 0 without both.
    static double efficiency(const WorkoutHistorySummary &ride);
};

/// Heart rate on the same workout over the last rides: at the same ERG targets
/// a lower heart rate means fitter.
struct RideTrend
{
    struct Ride {
        QDate  date;
        int    avgHrBpm   = 0;
        double efficiency = 0.0;
        bool   isCurrent  = false;
    };
    QVector<Ride> rides;              ///< oldest first, the current ride last
    int    bestIndex          = -1;   ///< lowest heart rate
    int    currentHrRank      = 0;    ///< 1 = lowest heart rate of all
    int    currentEfficiencyRank = 0; ///< 1 = most watts per beat
    int    currentPowerW      = 0;
    bool   valid              = false;   ///< at least two rides with heart rate

    bool currentIsBest() const { return bestIndex == rides.size() - 1; }
    static RideTrend build(const WorkoutHistorySummary &current,
                           const QList<WorkoutHistorySummary> &previousNewestFirst);
};

#endif // RIDESUMMARY_H
