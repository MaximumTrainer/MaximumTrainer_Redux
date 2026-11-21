#ifndef MANAGERACHIEVEMENT_H
#define MANAGERACHIEVEMENT_H

#include <QObject>

#include "achievement.h"
#include "achievementcatalog.h"
#include "achievementlivetracker.h"
#include "personalbesttracker.h"
#include "powerbests.h"
#include "workout.h"
#include "workouthistorysummary.h"

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
    /// The rides read by the last evaluateHistory() (startup and after each ride).
    const QList<WorkoutHistorySummary> &history() const { return m_history; }

    /// Riding hours per week the rider aims for (0 = none); setting it emits weeklyGoalChanged.
    static double weeklyGoalHours();
    void setWeeklyGoalHours(double hours);

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
    /// Clears the rolling peak-effort windows and loads the power bests the
    /// ride is compared with (in the background); call when a workout opens.
    void startLiveRide();
    /// Reports a power best still in progress; call when the ride ends.
    void finishLiveRide();

public:
    /// The rider's best power per tracked duration (5 s to 20 min), all time
    /// and over the last 90 days. Worked out in the background after each
    /// evaluateHistory(); empty until then.
    struct PersonalRecord {
        int seconds = 0;
        PowerBests::Effort allTime;
        PowerBests::Effort recent;
    };
    const QVector<PersonalRecord> &personalRecords() const { return m_personalRecords; }

signals:
    void achievementCompleted(Achievement achievement);
    /// Anything was earned (announced or not); the achievements page refreshes.
    void achievementsChanged();
    void weeklyGoalChanged();
    void personalRecordsChanged();

private:
    void award(const QString &key, bool announce);
    void announceBests(const QVector<PersonalBestTracker::Event> &events);
    void refreshPersonalRecords();

    AchievementCatalog::HistoryStats m_stats;
    QList<WorkoutHistorySummary> m_history;
    AchievementLiveTracker m_liveTracker;
    PersonalBestTracker m_bestTracker;
    int m_bestLoadGeneration = 0;
    QVector<PersonalRecord> m_personalRecords;
    int m_recordsGeneration = 0;
};

Q_DECLARE_METATYPE(ManagerAchievement*)

#endif // MANAGERACHIEVEMENT_H
