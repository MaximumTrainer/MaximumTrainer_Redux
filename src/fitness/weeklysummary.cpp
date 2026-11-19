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

} // namespace WeeklySummary
