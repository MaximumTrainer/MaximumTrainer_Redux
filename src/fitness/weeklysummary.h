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

} // namespace WeeklySummary

#endif // WEEKLYSUMMARY_H
