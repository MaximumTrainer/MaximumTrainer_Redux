#ifndef ACHIEVEMENTCATALOG_H
#define ACHIEVEMENTCATALOG_H

#include <QList>
#include <QString>

struct WorkoutHistorySummary;

/// The local achievement list and the rules to earn them. Most achievements are
/// derived from the ride history (so rides done before this feature count);
/// "event" ones are recorded when the moment happens (an FTP test, a MAP step),
/// and "peak" ones are checked every second of a ride (AchievementLiveTracker).
namespace AchievementCatalog {

enum class Category { Milestones, Endurance, Consistency, Fitness, Power, Special };

enum class Kind {
    RideCount,       ///< number of rides
    TotalHours,      ///< total ride time, hours
    TotalKm,         ///< total distance, km
    RideMinutes,     ///< one ride of at least N minutes
    RideTss,         ///< one ride with TSS >= N
    RideKcal,        ///< one ride with >= N kcal
    RideKm,          ///< one ride of at least N km
    WeekTss,         ///< >= N TSS within any 7 days
    WeekRides,       ///< >= N rides within any 7 days
    WeekHours,       ///< >= N hours within any 7 days
    MonthRides,      ///< >= N rides within any 30 days
    WeekendDouble,   ///< rides on the Saturday and Sunday of one weekend
    NewYear,         ///< a ride on January 1
    Christmas,       ///< a ride on December 25
    WattsPerKg,      ///< FTP of at least N W/kg (from the profile)
    StreakDays,      ///< rides on N consecutive days
    EarlyBird,       ///< a ride starting before 7:00
    NightOwl,        ///< a ride starting at 21:00 or later
    Event,           ///< recorded when it happens (tests, FTP raise, group ride)
    PeakPower,       ///< average of at least N watts over `seconds`, live
    PeakFtpPercent,  ///< average of at least N x FTP over `seconds`, live
    PeakCadence,     ///< average of at least N rpm over `seconds`, live
};

struct Definition {
    QString  key;          ///< stable id, used for storage
    QString  name;
    QString  description;
    Category category;
    Kind     kind;
    double   target;       ///< threshold for the kind (unused for Event)
    int      seconds = 0;  ///< effort length for the Peak kinds
};

/// Every achievement, in display order.
const QList<Definition> &all();
const Definition *find(const QString &key);
QString categoryName(Category category);

/// Ride history reduced to what the rules need.
struct HistoryStats {
    int    rides          = 0;
    double hours          = 0.0;
    double km             = 0.0;
    int    longestMinutes = 0;
    double maxRideTss     = 0.0;
    int    maxRideKcal    = 0;
    double maxRideKm      = 0.0;
    double bestWeekTss    = 0.0;
    int    bestWeekRides  = 0;
    double bestWeekHours  = 0.0;
    int    bestMonthRides = 0;
    bool   weekendDouble  = false;
    bool   newYear        = false;
    bool   christmas      = false;
    double ftpWattsPerKg  = 0.0;   ///< filled in from the profile, not the history
    int    bestStreakDays = 0;
    bool   earlyBird      = false;
    bool   nightOwl       = false;
};

HistoryStats statsFrom(const QList<WorkoutHistorySummary> &history);

/// True for the yes/no kinds that have no meaningful progress bar.
bool isYesNo(const Definition &definition);

/// True for the Peak kinds, earned only from live ride data (never from history).
bool isLive(const Definition &definition);

/// Current value towards \a definition (0 for Event achievements).
double progress(const Definition &definition, const HistoryStats &stats);
bool   reached(const Definition &definition, const HistoryStats &stats);

/// Keys of the event achievements.
namespace Events {
inline const QString FirstTest     = QStringLiteral("first_test");
inline const QString MapStep6      = QStringLiteral("map_step_6");
inline const QString MapStep7      = QStringLiteral("map_step_7");
inline const QString FtpRaised     = QStringLiteral("ftp_raised");
inline const QString TargetManiac  = QStringLiteral("target_maniac");
inline const QString GroupRide     = QStringLiteral("group_ride");
inline const QString GroupHost     = QStringLiteral("group_host");
inline const QString GroupJoin     = QStringLiteral("group_join");
inline const QString StudioHost    = QStringLiteral("studio_host");
}

} // namespace AchievementCatalog

#endif // ACHIEVEMENTCATALOG_H
