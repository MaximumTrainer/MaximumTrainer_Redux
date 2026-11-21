#ifndef MANAGERACHIEVEMENT_H
#define MANAGERACHIEVEMENT_H

#include <QObject>

#include "achievement.h"
#include "achievementcatalog.h"
#include "achievementlivetracker.h"
#include "workout.h"

/// Awards the local achievements (AchievementCatalog) and saves them
/// (AchievementStore). History-based ones are evaluated from the ride history,
/// so earlier rides count; live ones fire during the ride; event ones are
/// recorded when they happen. Each newly earned one is announced once.
class ManagerAchievement : public QObject
{
    Q_OBJECT
public:
    explicit ManagerAchievement(QObject *parent = nullptr);

    /// Awards everything the ride history has reached. With \a announce false
    /// (startup catch-up) nothing is announced, they are just saved.
    void evaluateHistory(bool announce = true);

    /// Awards an event achievement (AchievementCatalog::Events).
    void recordEvent(const QString &key);

    /// Ride-history totals from the last evaluateHistory(), for progress bars.
    const AchievementCatalog::HistoryStats &historyStats() const { return m_stats; }

public slots:
    /// FTP test, MAP test and Target Maniac checks at the end of a workout.
    void workoutCompleted(Workout workout);
    void checkMAPAchievement(int lastStepCompleted);
    /// Live checks during a ride: elapsed minutes and current TSS.
    void checkLiveMinutes(int elapsedMinutes);
    void checkLiveTss(double tss);
    /// One recorded second of the solo rider (negative = no sensor data).
    void checkLiveSecond(double powerWatts, double cadenceRpm, double ftpWatts);
    void checkLiveDistance(double meters);
    /// Clears the rolling peak-effort windows; call when a workout opens.
    void startLiveRide();

signals:
    void achievementCompleted(Achievement achievement);
    /// Anything was earned (announced or not); the achievements page refreshes.
    void achievementsChanged();

private:
    void award(const QString &key, bool announce);

    AchievementCatalog::HistoryStats m_stats;
    AchievementLiveTracker m_liveTracker;
};

Q_DECLARE_METATYPE(ManagerAchievement*)

#endif // MANAGERACHIEVEMENT_H
