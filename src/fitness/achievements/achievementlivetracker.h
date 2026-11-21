#ifndef ACHIEVEMENTLIVETRACKER_H
#define ACHIEVEMENTLIVETRACKER_H

#include <QList>
#include <QStringList>
#include <deque>

/// Rolling averages for the "peak" achievements (sprints, FTP-relative efforts,
/// cadence), fed one second of ride data at a time so each one is reported the
/// moment the effort is complete rather than after the ride is saved.
class AchievementLiveTracker
{
public:
    AchievementLiveTracker();

    /// Starts a new ride: forgets the previous samples and reports.
    void reset();

    /// Adds one second of riding (a negative value means no sensor data) and
    /// returns the keys whose effort was just completed, each once per ride.
    QStringList addSecond(double powerWatts, double cadenceRpm, double ftpWatts);

private:
    struct Window {
        QString key;
        int     kind = 0;
        double  target = 0.0;
        int     seconds = 1;
        std::deque<double> samples;
        double  sum = 0.0;
        bool    reported = false;
    };
    QList<Window> m_windows;
};

#endif // ACHIEVEMENTLIVETRACKER_H
