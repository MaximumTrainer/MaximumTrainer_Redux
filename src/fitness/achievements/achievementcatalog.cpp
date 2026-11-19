#include "achievementcatalog.h"

#include <QCoreApplication>
#include <QDate>
#include <QMap>
#include <algorithm>

#include "workouthistorysummary.h"

namespace AchievementCatalog {

namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("AchievementCatalog", text);
}

QList<Definition> buildList()
{
    using C = Category;
    using K = Kind;
    return {
        // Milestones
        {"first_ride",    tr("First Pedal Strokes"), tr("Complete your first workout."),            C::Milestones, K::RideCount, 1},
        {"rides_10",      tr("Getting Started"),     tr("Complete 10 workouts."),                   C::Milestones, K::RideCount, 10},
        {"rides_50",      tr("Committed"),           tr("Complete 50 workouts."),                   C::Milestones, K::RideCount, 50},
        {"rides_100",     tr("Century of Sessions"), tr("Complete 100 workouts."),                  C::Milestones, K::RideCount, 100},
        {"rides_250",     tr("Indoor Legend"),       tr("Complete 250 workouts."),                  C::Milestones, K::RideCount, 250},
        {"hours_10",      tr("10 Hours In"),         tr("Ride 10 hours in total."),                 C::Milestones, K::TotalHours, 10},
        {"hours_50",      tr("50 Hours In"),         tr("Ride 50 hours in total."),                 C::Milestones, K::TotalHours, 50},
        {"hours_100",     tr("100 Hours In"),        tr("Ride 100 hours in total."),                C::Milestones, K::TotalHours, 100},
        {"hours_500",     tr("500 Hours In"),        tr("Ride 500 hours in total."),                C::Milestones, K::TotalHours, 500},
        {"km_100",        tr("First 100 km"),        tr("Ride 100 km in total."),                   C::Milestones, K::TotalKm, 100},
        {"km_1000",       tr("1,000 km Club"),       tr("Ride 1,000 km in total."),                 C::Milestones, K::TotalKm, 1000},
        {"km_5000",       tr("5,000 km Club"),       tr("Ride 5,000 km in total."),                 C::Milestones, K::TotalKm, 5000},
        // Endurance
        {"ride_60",       tr("Endurance Starter"),   tr("Ride 1 hour in one workout."),             C::Endurance, K::RideMinutes, 60},
        {"ride_120",      tr("Endurance Intermediate"), tr("Ride 2 hours in one workout."),         C::Endurance, K::RideMinutes, 120},
        {"ride_180",      tr("Endurance Master"),    tr("Ride 3 hours in one workout."),            C::Endurance, K::RideMinutes, 180},
        {"tss_100",       tr("Big Day"),             tr("Score 100 TSS or more in one workout."),   C::Endurance, K::RideTss, 100},
        {"kcal_1000",     tr("Furnace"),             tr("Burn 1,000 kcal in one workout."),         C::Endurance, K::RideKcal, 1000},
        {"week_tss_500",  tr("Big Week"),            tr("Score 500 TSS within 7 days."),            C::Endurance, K::WeekTss, 500},
        // Consistency
        {"streak_3",      tr("On a Roll"),           tr("Ride 3 days in a row."),                   C::Consistency, K::StreakDays, 3},
        {"streak_7",      tr("Unstoppable"),         tr("Ride 7 days in a row."),                   C::Consistency, K::StreakDays, 7},
        {"week_rides_5",  tr("Five a Week"),         tr("Complete 5 workouts within 7 days."),      C::Consistency, K::WeekRides, 5},
        {"early_bird",    tr("Early Bird"),          tr("Start a workout before 7:00."),            C::Consistency, K::EarlyBird, 1},
        {"night_owl",     tr("Night Owl"),           tr("Start a workout at 21:00 or later."),      C::Consistency, K::NightOwl, 1},
        // Fitness
        {Events::FirstTest,    tr("Learning to Test"), tr("Complete an FTP or MAP test."),          C::Fitness, K::Event, 0},
        {Events::MapStep6,     tr("MAP Step 6"),       tr("Reach step 6 of the MAP test."),         C::Fitness, K::Event, 0},
        {Events::MapStep7,     tr("MAP Step 7"),       tr("Reach step 7 of the MAP test."),         C::Fitness, K::Event, 0},
        {Events::FtpRaised,    tr("Getting Stronger"), tr("Raise your FTP."),                       C::Fitness, K::Event, 0},
        // Special
        {Events::TargetManiac, tr("Target Maniac"),    tr("Finish a workout with power, power balance, cadence and heart rate targets."), C::Special, K::Event, 0},
        {Events::GroupRide,    tr("Better Together"),  tr("Finish a group workout."),               C::Special, K::Event, 0},
    };
}

// Best total of \a valueOf over any 7-day window of ride dates.
template <typename ValueOf>
double bestWeek(const QMap<QDate, QList<const WorkoutHistorySummary *>> &byDay, ValueOf valueOf)
{
    double best = 0.0;
    for (auto end = byDay.cbegin(); end != byDay.cend(); ++end) {
        double total = 0.0;
        for (auto it = byDay.cbegin(); it != byDay.cend(); ++it)
            if (it.key() <= end.key() && it.key() > end.key().addDays(-7))
                for (const WorkoutHistorySummary *ride : it.value())
                    total += valueOf(*ride);
        best = std::max(best, total);
    }
    return best;
}
} // namespace

const QList<Definition> &all()
{
    static const QList<Definition> list = buildList();
    return list;
}

const Definition *find(const QString &key)
{
    for (const Definition &definition : all())
        if (definition.key == key)
            return &definition;
    return nullptr;
}

QString categoryName(Category category)
{
    switch (category) {
    case Category::Milestones:  return tr("Milestones");
    case Category::Endurance:   return tr("Endurance");
    case Category::Consistency: return tr("Consistency");
    case Category::Fitness:     return tr("Fitness");
    case Category::Special:     return tr("Special");
    }
    return QString();
}

HistoryStats statsFrom(const QList<WorkoutHistorySummary> &history)
{
    HistoryStats stats;
    QMap<QDate, QList<const WorkoutHistorySummary *>> byDay;
    for (const WorkoutHistorySummary &ride : history) {
        ++stats.rides;
        stats.hours += ride.durationSec / 3600.0;
        stats.km    += ride.totalDistanceKm;
        stats.longestMinutes = std::max(stats.longestMinutes, ride.durationSec / 60);
        stats.maxRideTss     = std::max(stats.maxRideTss, ride.tss);
        stats.maxRideKcal    = std::max(stats.maxRideKcal, ride.calories);
        if (!ride.startTime.isValid())
            continue;
        const QDateTime local = ride.startTime.toLocalTime();
        stats.earlyBird = stats.earlyBird || local.time().hour() < 7;
        stats.nightOwl  = stats.nightOwl  || local.time().hour() >= 21;
        byDay[local.date()].append(&ride);
    }

    stats.bestWeekTss   = bestWeek(byDay, [](const WorkoutHistorySummary &r) { return r.tss; });
    stats.bestWeekRides = qRound(bestWeek(byDay, [](const WorkoutHistorySummary &) { return 1.0; }));

    int run = 0;
    QDate previous;
    for (auto it = byDay.cbegin(); it != byDay.cend(); ++it) {
        run = (previous.isValid() && previous.addDays(1) == it.key()) ? run + 1 : 1;
        stats.bestStreakDays = std::max(stats.bestStreakDays, run);
        previous = it.key();
    }
    return stats;
}

double progress(const Definition &definition, const HistoryStats &stats)
{
    switch (definition.kind) {
    case Kind::RideCount:   return stats.rides;
    case Kind::TotalHours:  return stats.hours;
    case Kind::TotalKm:     return stats.km;
    case Kind::RideMinutes: return stats.longestMinutes;
    case Kind::RideTss:     return stats.maxRideTss;
    case Kind::RideKcal:    return stats.maxRideKcal;
    case Kind::WeekTss:     return stats.bestWeekTss;
    case Kind::WeekRides:   return stats.bestWeekRides;
    case Kind::StreakDays:  return stats.bestStreakDays;
    case Kind::EarlyBird:   return stats.earlyBird ? 1 : 0;
    case Kind::NightOwl:    return stats.nightOwl ? 1 : 0;
    case Kind::Event:       return 0;
    }
    return 0;
}

bool reached(const Definition &definition, const HistoryStats &stats)
{
    return definition.kind != Kind::Event && progress(definition, stats) >= definition.target;
}

} // namespace AchievementCatalog
