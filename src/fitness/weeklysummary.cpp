#include "weeklysummary.h"

#include "workouthistorysummary.h"

namespace WeeklySummary {

QDate mondayOf(const QDate &day)
{
    return day.addDays(1 - day.dayOfWeek());
}

QVector<Week> recentWeeks(const QList<WorkoutHistorySummary> &history, const QDate &today, int weeks)
{
    QVector<Week> result(weeks);
    const QDate thisMonday = mondayOf(today);
    for (int i = 0; i < weeks; ++i)
        result[i].monday = thisMonday.addDays(qint64(-7) * (weeks - 1 - i));

    for (const WorkoutHistorySummary &ride : history) {
        if (!ride.startTime.isValid())
            continue;
        const QDate monday = mondayOf(ride.startTime.toLocalTime().date());
        const qint64 weekIndex = result.first().monday.daysTo(monday) / 7;
        if (weekIndex < 0 || weekIndex >= weeks)
            continue;
        Week &week = result[weekIndex];
        ++week.rides;
        week.hours += ride.durationSec / 3600.0;
        week.tss   += ride.tss;
        week.km    += ride.totalDistanceKm;
    }
    return result;
}

GoalProgress goalProgress(const QList<WorkoutHistorySummary> &history, const QDate &today, double goalHours)
{
    GoalProgress progress;
    if (goalHours <= 0.0)
        return progress;
    // Five years is far beyond any realistic streak.
    constexpr int kWeeksBack = 260;
    const QVector<Week> weeks = recentWeeks(history, today, kWeeksBack);
    // A minute of slack, so 4:59:30 on a 5 h goal counts.
    const auto reaches = [goalHours](const Week &week) { return week.hours >= goalHours - 1.0 / 60.0; };

    progress.goalHours = goalHours;
    progress.hoursThisWeek = weeks.last().hours;
    progress.reached = reaches(weeks.last());
    for (qsizetype index = weeks.size() - 2; index >= 0 && reaches(weeks[index]); --index)
        ++progress.streakWeeks;
    if (progress.reached)
        ++progress.streakWeeks;
    return progress;
}

} // namespace WeeklySummary
