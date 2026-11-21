#ifndef WEEKLYSUMMARY_H
#define WEEKLYSUMMARY_H

#include <QDate>
#include <QList>
#include <QVector>

struct WorkoutHistorySummary;

/// Training totals per calendar week (Monday to Sunday), for the History tab.
namespace WeeklySummary {

struct Week {
    QDate  monday;
    int    rides = 0;
    double hours = 0.0;
    double tss   = 0.0;
    double km    = 0.0;
};

/// The week holding \a day (starting on its Monday).
QDate mondayOf(const QDate &day);

/// The \a weeks most recent weeks ending with the week of \a today, oldest
/// first, including weeks without rides.
QVector<Week> recentWeeks(const QList<WorkoutHistorySummary> &history, const QDate &today, int weeks);

/// This week against a weekly riding-time goal.
struct GoalProgress {
    double goalHours = 0.0;      ///< 0 = no goal set.
    double hoursThisWeek = 0.0;
    bool   reached = false;
    /// Weeks in a row that reached the goal: the finished weeks before this
    /// one, plus this week once it reaches it. A week still in progress does
    /// not break the streak.
    int    streakWeeks = 0;
};

GoalProgress goalProgress(const QList<WorkoutHistorySummary> &history, const QDate &today, double goalHours);

} // namespace WeeklySummary

#endif // WEEKLYSUMMARY_H
