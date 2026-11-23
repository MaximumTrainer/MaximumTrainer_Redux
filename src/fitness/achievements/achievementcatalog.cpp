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
        {"rides_500",     tr("Lifer"),               tr("Complete 500 workouts."),                  C::Milestones, K::RideCount, 500},
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
        {"ride_240",      tr("Ultra"),               tr("Ride 4 hours in one workout."),            C::Endurance, K::RideMinutes, 240},
        {"tss_100",       tr("Big Day"),             tr("Score 100 TSS or more in one workout."),   C::Endurance, K::RideTss, 100},
        {"tss_200",       tr("Epic Day"),            tr("Score 200 TSS or more in one workout."),   C::Endurance, K::RideTss, 200},
        {"kcal_1000",     tr("Furnace"),             tr("Burn 1,000 kcal in one workout."),         C::Endurance, K::RideKcal, 1000},
        {"kcal_2000",     tr("Inferno"),             tr("Burn 2,000 kcal in one workout."),         C::Endurance, K::RideKcal, 2000},
        {"ride_km_30",    tr("Thirty Up"),           tr("Ride 30 km in one workout."),              C::Endurance, K::RideKm, 30},
        {"ride_km_50",    tr("Half Century"),        tr("Ride 50 km in one workout."),              C::Endurance, K::RideKm, 50},
        {"ride_km_100",   tr("Indoor Century"),      tr("Ride 100 km in one workout."),             C::Endurance, K::RideKm, 100},
        {"week_tss_500",  tr("Big Week"),            tr("Score 500 TSS within 7 days."),            C::Endurance, K::WeekTss, 500},
        {"week_tss_800",  tr("Overload Week"),       tr("Score 800 TSS within 7 days."),            C::Endurance, K::WeekTss, 800},
        {"week_hours_10", tr("Volume Week"),         tr("Ride 10 hours within 7 days."),            C::Endurance, K::WeekHours, 10},
        // Consistency
        {"streak_3",      tr("On a Roll"),           tr("Ride 3 days in a row."),                   C::Consistency, K::StreakDays, 3},
        {"streak_7",      tr("Unstoppable"),         tr("Ride 7 days in a row."),                   C::Consistency, K::StreakDays, 7},
        {"streak_14",     tr("Fortnight on Fire"),   tr("Ride 14 days in a row."),                  C::Consistency, K::StreakDays, 14},
        {"goal_weeks_1",  tr("Goal Getter"),         tr("Reach your weekly riding goal."),          C::Consistency, K::GoalWeeks, 1},
        {"goal_weeks_4",  tr("Month on Target"),     tr("Reach your weekly goal 4 weeks in a row."), C::Consistency, K::GoalWeeks, 4},
        {"goal_weeks_12", tr("Quarter Strong"),      tr("Reach your weekly goal 12 weeks in a row."), C::Consistency, K::GoalWeeks, 12},
        {"goal_weeks_26", tr("Half-Year Hero"),      tr("Reach your weekly goal 26 weeks in a row."), C::Consistency, K::GoalWeeks, 26},
        {"week_rides_5",  tr("Five a Week"),         tr("Complete 5 workouts within 7 days."),      C::Consistency, K::WeekRides, 5},
        {"month_rides_20",tr("Habit Formed"),        tr("Complete 20 workouts within 30 days."),    C::Consistency, K::MonthRides, 20},
        {"weekend_double",tr("Weekend Warrior"),     tr("Ride on the Saturday and Sunday of the same weekend."), C::Consistency, K::WeekendDouble, 1},
        {"early_bird",    tr("Early Bird"),          tr("Start a workout before 7:00."),            C::Consistency, K::EarlyBird, 1},
        {"night_owl",     tr("Night Owl"),           tr("Start a workout at 21:00 or later."),      C::Consistency, K::NightOwl, 1},
        // Fitness
        {Events::FirstTest,    tr("Learning to Test"), tr("Complete an FTP or MAP test."),          C::Fitness, K::Event, 0},
        {Events::MapStep6,     tr("MAP Step 6"),       tr("Reach step 6 of the MAP test."),         C::Fitness, K::Event, 0},
        {Events::MapStep7,     tr("MAP Step 7"),       tr("Reach step 7 of the MAP test."),         C::Fitness, K::Event, 0},
        {Events::FtpRaised,    tr("Getting Stronger"), tr("Raise your FTP."),                       C::Fitness, K::Event, 0},
        {"wkg_3",              tr("Rising Climber"),   tr("Reach an FTP of 3.0 W/kg."),             C::Fitness, K::WattsPerKg, 3.0},
        {"wkg_4",              tr("Climber"),          tr("Reach an FTP of 4.0 W/kg."),             C::Fitness, K::WattsPerKg, 4.0},
        {"wkg_5",              tr("Mountain Goat"),    tr("Reach an FTP of 5.0 W/kg."),             C::Fitness, K::WattsPerKg, 5.0},
        // Power & cadence: earned the moment the effort is done (ERG holds the
        // target, so these come from free rides, slope and test intervals).
        {"sprint_600",    tr("Sprinter"),            tr("Hold 600 W for 5 seconds."),               C::Power, K::PeakPower, 600, 5},
        {"sprint_800",    tr("Rocket Legs"),         tr("Hold 800 W for 5 seconds."),               C::Power, K::PeakPower, 800, 5},
        {"sprint_1000",   tr("Kilowatt Club"),       tr("Hold 1,000 W for 5 seconds."),             C::Power, K::PeakPower, 1000, 5},
        {"sprint_1200",   tr("Track Sprinter"),      tr("Hold 1,200 W for 5 seconds."),             C::Power, K::PeakPower, 1200, 5},
        {"ftp150_1min",   tr("Over the Top"),        tr("Hold 150% of your FTP for 1 minute."),     C::Power, K::PeakFtpPercent, 1.5, 60},
        {"ftp120_5min",   tr("VO2 Smasher"),         tr("Hold 120% of your FTP for 5 minutes."),    C::Power, K::PeakFtpPercent, 1.2, 300},
        {"ftp100_20min",  tr("Threshold Hold"),      tr("Hold your FTP for 20 minutes."),           C::Power, K::PeakFtpPercent, 1.0, 1200},
        {"ftp90_60min",   tr("Hour of Power"),       tr("Hold 90% of your FTP for 1 hour."),        C::Power, K::PeakFtpPercent, 0.9, 3600},
        {"cadence_120",   tr("Spin Doctor"),         tr("Hold 120 rpm for 30 seconds."),            C::Power, K::PeakCadence, 120, 30},
        {"cadence_140",   tr("Washing Machine"),     tr("Hold 140 rpm for 10 seconds."),            C::Power, K::PeakCadence, 140, 10},
        {"cadence_100_10min", tr("High Revs"),       tr("Hold 100 rpm for 10 minutes."),            C::Power, K::PeakCadence, 100, 600},
        // Special
        {Events::TargetManiac, tr("Target Maniac"),    tr("Finish a workout with power, power balance, cadence and heart rate targets."), C::Special, K::Event, 0},
        {Events::GroupRide,    tr("Better Together"),  tr("Finish a group workout, as the host or as a guest."),               C::Special, K::Event, 0},
        // Courses: cross the finish line of each course
        {Events::courseFinished(QStringLiteral("longchamp")),    tr("Longchamp Regular"), tr("Finish the Longchamp loop course."), C::Courses, K::Event, 0},
        {Events::courseFinished(QStringLiteral("alpe-dhuez")),   tr("21 Hairpins"),       tr("Finish the Alpe d'Huez course."),    C::Courses, K::Event, 0},
        {Events::courseFinished(QStringLiteral("mont-ventoux")), tr("Giant of Provence"), tr("Finish the Mont Ventoux course."),   C::Courses, K::Event, 0},
    };
}

// Best total of \a valueOf over any \a days-long window of ride dates.
template <typename ValueOf>
double bestWindow(const QMap<QDate, QList<const WorkoutHistorySummary *>> &byDay, int days, ValueOf valueOf)
{
    double best = 0.0;
    for (auto end = byDay.cbegin(); end != byDay.cend(); ++end) {
        double total = 0.0;
        for (auto it = byDay.cbegin(); it != byDay.cend(); ++it)
            if (it.key() <= end.key() && it.key() > end.key().addDays(-days))
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
    case Category::Power:       return tr("Power & Cadence");
    case Category::Courses:     return tr("Courses");
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
        stats.maxRideKm      = std::max(stats.maxRideKm, ride.totalDistanceKm);
        if (!ride.startTime.isValid())
            continue;
        const QDateTime local = ride.startTime.toLocalTime();
        stats.earlyBird = stats.earlyBird || local.time().hour() < 7;
        stats.nightOwl  = stats.nightOwl  || local.time().hour() >= 21;
        byDay[local.date()].append(&ride);
    }

    const auto one = [](const WorkoutHistorySummary &) { return 1.0; };
    stats.bestWeekTss    = bestWindow(byDay, 7, [](const WorkoutHistorySummary &r) { return r.tss; });
    stats.bestWeekRides  = qRound(bestWindow(byDay, 7, one));
    stats.bestWeekHours  = bestWindow(byDay, 7, [](const WorkoutHistorySummary &r) { return r.durationSec / 3600.0; });
    stats.bestMonthRides = qRound(bestWindow(byDay, 30, one));
    for (auto it = byDay.cbegin(); it != byDay.cend(); ++it)
        if (it.key().dayOfWeek() == Qt::Saturday && byDay.contains(it.key().addDays(1)))
            stats.weekendDouble = true;

    int run = 0;
    QDate previous;
    for (auto it = byDay.cbegin(); it != byDay.cend(); ++it) {
        run = (previous.isValid() && previous.addDays(1) == it.key()) ? run + 1 : 1;
        stats.bestStreakDays = std::max(stats.bestStreakDays, run);
        previous = it.key();
    }
    return stats;
}

int goalStreakWeeks(const QList<WorkoutHistorySummary> &history, double goalHours)
{
    if (goalHours <= 0.0)
        return 0;
    QMap<QDate, double> hoursByMonday;
    for (const WorkoutHistorySummary &ride : history) {
        if (!ride.startTime.isValid())
            continue;
        const QDate day = ride.startTime.toLocalTime().date();
        hoursByMonday[day.addDays(1 - day.dayOfWeek())] += ride.durationSec / 3600.0;
    }
    int best = 0;
    int run = 0;
    QDate previous;
    for (auto it = hoursByMonday.cbegin(); it != hoursByMonday.cend(); ++it) {
        if (it.value() < goalHours - 1.0 / 60.0) {
            run = 0;
            previous = QDate();
            continue;
        }
        run = (previous.isValid() && previous.addDays(7) == it.key()) ? run + 1 : 1;
        best = std::max(best, run);
        previous = it.key();
    }
    return best;
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
    case Kind::RideKm:      return stats.maxRideKm;
    case Kind::WeekTss:     return stats.bestWeekTss;
    case Kind::WeekRides:   return stats.bestWeekRides;
    case Kind::StreakDays:  return stats.bestStreakDays;
    case Kind::GoalWeeks:   return stats.bestGoalWeeks;
    case Kind::WeekHours:   return stats.bestWeekHours;
    case Kind::MonthRides:  return stats.bestMonthRides;
    case Kind::WeekendDouble: return stats.weekendDouble ? 1 : 0;
    case Kind::WattsPerKg:  return stats.ftpWattsPerKg;
    case Kind::EarlyBird:   return stats.earlyBird ? 1 : 0;
    case Kind::NightOwl:    return stats.nightOwl ? 1 : 0;
    case Kind::Event:
    case Kind::PeakPower:
    case Kind::PeakFtpPercent:
    case Kind::PeakCadence: return 0;
    }
    return 0;
}

bool isLive(const Definition &definition)
{
    return definition.kind == Kind::PeakPower || definition.kind == Kind::PeakFtpPercent
           || definition.kind == Kind::PeakCadence;
}

bool isYesNo(const Definition &definition)
{
    switch (definition.kind) {
    case Kind::Event: case Kind::EarlyBird: case Kind::NightOwl:
    case Kind::WeekendDouble:
    case Kind::PeakPower: case Kind::PeakFtpPercent: case Kind::PeakCadence:
        return true;
    default:
        return false;
    }
}

bool reached(const Definition &definition, const HistoryStats &stats)
{
    return definition.kind != Kind::Event && !isLive(definition)
           && progress(definition, stats) >= definition.target;
}

} // namespace AchievementCatalog
