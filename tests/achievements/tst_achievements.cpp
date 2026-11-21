#include <QtTest>
#include <QSet>
#include <QStandardPaths>
#include <QTimeZone>

#include "achievementcatalog.h"
#include "achievementlivetracker.h"
#include "achievementstore.h"
#include "workouthistorysummary.h"

using namespace AchievementCatalog;

namespace {
WorkoutHistorySummary ride(const QDateTime &start, int minutes, double tss = 0, double km = 0, int kcal = 0)
{
    WorkoutHistorySummary summary;
    summary.startTime       = start;
    summary.durationSec     = minutes * 60;
    summary.tss             = tss;
    summary.totalDistanceKm = km;
    summary.calories        = kcal;
    summary.valid           = true;
    return summary;
}
QDateTime at(int day, int hour, int minute = 0)
{
    return QDateTime(QDate(2026, 3, day), QTime(hour, minute), QTimeZone::LocalTime);
}
} // namespace

class TstAchievements : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("MaximumTrainerTest"));
        QCoreApplication::setApplicationName(QStringLiteral("AchievementsTest"));
        QSettings().clear();
    }

    void catalog_hasUniqueKeysInEveryCategory()
    {
        QVERIFY(all().size() >= 40);
        QSet<QString> keys;
        QSet<int> categories;
        for (const Definition &definition : all()) {
            QVERIFY2(!keys.contains(definition.key), qPrintable(definition.key));
            keys.insert(definition.key);
            categories.insert(static_cast<int>(definition.category));
            QVERIFY(!definition.name.isEmpty());
            QVERIFY(!definition.description.isEmpty());
            QCOMPARE(find(definition.key), &definition);
        }
        QCOMPARE(categories.size(), 6);
        QVERIFY(find(Events::FtpRaised));
        QVERIFY(!find(QStringLiteral("nope")));
    }

    void stats_totalsAndBestRide()
    {
        const HistoryStats stats = statsFrom({ride(at(1, 10), 90, 80, 40, 900),
                                              ride(at(5, 10), 30, 25, 12, 300)});
        QCOMPARE(stats.rides, 2);
        QCOMPARE(stats.hours, 2.0);
        QCOMPARE(stats.km, 52.0);
        QCOMPARE(stats.longestMinutes, 90);
        QCOMPARE(stats.maxRideTss, 80.0);
        QCOMPARE(stats.maxRideKcal, 900);
    }

    void stats_streakAndSevenDayWindows()
    {
        // Days 1, 2, 3 (streak of 3), then 6 and 7; days 1..7 hold 5 rides.
        const HistoryStats stats = statsFrom({ride(at(1, 18), 60, 100), ride(at(2, 18), 60, 100),
                                              ride(at(3, 18), 60, 100), ride(at(6, 18), 60, 100),
                                              ride(at(7, 18), 60, 100), ride(at(9, 18), 60, 100)});
        QCOMPARE(stats.bestStreakDays, 3);
        QCOMPARE(stats.bestWeekRides, 5);
        QCOMPARE(stats.bestWeekTss, 500.0);
    }

    void stats_timeOfDay()
    {
        QVERIFY(statsFrom({ride(at(1, 6, 45), 30)}).earlyBird);
        QVERIFY(!statsFrom({ride(at(1, 7, 0), 30)}).earlyBird);
        QVERIFY(statsFrom({ride(at(1, 21, 0), 30)}).nightOwl);
        QVERIFY(!statsFrom({ride(at(1, 20, 59), 30)}).nightOwl);
    }

    void stats_newerRules()
    {
        // 2026-03-07 is a Saturday.
        QVERIFY(statsFrom({ride(at(7, 9), 60), ride(at(8, 9), 60)}).weekendDouble);
        QVERIFY(!statsFrom({ride(at(8, 9), 60), ride(at(9, 9), 60)}).weekendDouble);   // Sun + Mon
        const QDateTime newYear(QDate(2027, 1, 1), QTime(10, 0), QTimeZone::LocalTime);
        const QDateTime christmas(QDate(2026, 12, 25), QTime(10, 0), QTimeZone::LocalTime);
        QVERIFY(statsFrom({ride(newYear, 30)}).newYear);
        QVERIFY(statsFrom({ride(christmas, 30)}).christmas);
        QVERIFY(!statsFrom({ride(at(1, 10), 30)}).christmas);

        QList<WorkoutHistorySummary> month;
        for (int day = 1; day <= 20; ++day)
            month.append(ride(at(day, 18), 45));
        const HistoryStats stats = statsFrom(month);
        QCOMPARE(stats.bestMonthRides, 20);
        QCOMPARE(stats.bestWeekHours, 7 * 0.75);
        QCOMPARE(stats.bestStreakDays, 20);

        HistoryStats profile;
        profile.ftpWattsPerKg = 3.2;
        QVERIFY(reached(*find(QStringLiteral("wkg_3")), profile));
        QVERIFY(!reached(*find(QStringLiteral("wkg_4")), profile));
        QVERIFY(isYesNo(*find(QStringLiteral("weekend_double"))));
        QVERIFY(!isYesNo(*find(QStringLiteral("wkg_3"))));
    }

    void reached_followsTargets()
    {
        HistoryStats stats;
        stats.rides = 10;
        QVERIFY(reached(*find(QStringLiteral("rides_10")), stats));
        QVERIFY(!reached(*find(QStringLiteral("rides_50")), stats));
        QCOMPARE(progress(*find(QStringLiteral("rides_50")), stats), 10.0);
        // Event achievements are never reached from history.
        QVERIFY(!reached(*find(Events::FirstTest), stats));
    }

    void rideKm_fromHistoryAndNotLive()
    {
        const HistoryStats stats = statsFrom({ride(at(2, 8), 70, 0, 31.5), ride(at(3, 8), 40, 0, 12.0)});
        QCOMPARE(stats.maxRideKm, 31.5);
        QVERIFY(reached(*find(QStringLiteral("ride_km_30")), stats));
        QVERIFY(!reached(*find(QStringLiteral("ride_km_50")), stats));
        QVERIFY(!isLive(*find(QStringLiteral("ride_km_30"))));
    }

    void live_peaksAreNeverReachedFromHistory()
    {
        HistoryStats stats;
        stats.rides = 500;
        stats.maxRideKm = 500;
        for (const Definition &definition : all()) {
            if (!isLive(definition))
                continue;
            QVERIFY2(definition.seconds > 0, qPrintable(definition.key));
            QVERIFY(isYesNo(definition));
            QVERIFY2(!reached(definition, stats), qPrintable(definition.key));
        }
        QVERIFY(isLive(*find(QStringLiteral("sprint_600"))));
    }

    void tracker_sprintFiresOnTheFifthSecondOnce()
    {
        AchievementLiveTracker tracker;
        for (int second = 0; second < 4; ++second)
            QVERIFY(!tracker.addSecond(650, 95, 250).contains(QStringLiteral("sprint_600")));
        QCOMPARE(tracker.addSecond(650, 95, 250), QStringList{QStringLiteral("sprint_600")});
        for (int second = 0; second < 10; ++second)
            QVERIFY(!tracker.addSecond(650, 95, 250).contains(QStringLiteral("sprint_600")));
        tracker.reset();
        for (int second = 0; second < 4; ++second)
            tracker.addSecond(650, 95, 250);
        QVERIFY(tracker.addSecond(650, 95, 250).contains(QStringLiteral("sprint_600")));
    }

    void tracker_usesTheRollingAverage()
    {
        AchievementLiveTracker tracker;
        QStringList fired;
        for (double watts : {700.0, 700.0, 700.0, 700.0, 200.0})
            fired += tracker.addSecond(watts, 90, 250);
        QVERIFY(fired.contains(QStringLiteral("sprint_600")));   // (4 x 700 + 200) / 5 = 600

        AchievementLiveTracker shortEffort;
        fired.clear();
        for (double watts : {0.0, 900.0, 900.0, 900.0, -1.0, 0.0})
            fired += shortEffort.addSecond(watts, 90, 250);
        QVERIFY(!fired.contains(QStringLiteral("sprint_600")));  // best 5 s window: 2,700 / 5 = 540 W
    }

    void tracker_ftpRelativeNeedsAnFtp()
    {
        AchievementLiveTracker noFtp;
        QStringList fired;
        for (int second = 0; second < 120; ++second)
            fired += noFtp.addSecond(400, 90, 0);
        QVERIFY(!fired.contains(QStringLiteral("ftp150_1min")));

        AchievementLiveTracker tracker;
        for (int second = 0; second < 59; ++second)
            QVERIFY(!tracker.addSecond(300, 90, 200).contains(QStringLiteral("ftp150_1min")));
        QVERIFY(tracker.addSecond(300, 90, 200).contains(QStringLiteral("ftp150_1min")));
    }

    void tracker_cadenceIgnoresPowerAndMissingData()
    {
        AchievementLiveTracker tracker;
        QStringList fired;
        for (int second = 0; second < 9; ++second)
            fired += tracker.addSecond(100, 145, 250);
        fired += tracker.addSecond(100, -1, 250);   // dropout counts as 0 rpm
        QVERIFY(!fired.contains(QStringLiteral("cadence_140")));
        for (int second = 0; second < 10; ++second)
            fired += tracker.addSecond(100, 145, 250);
        QVERIFY(fired.contains(QStringLiteral("cadence_140")));
        QVERIFY(!fired.contains(QStringLiteral("sprint_600")));
    }

    void store_unlocksOnceWithItsDate()
    {
        const QDateTime when(QDate(2026, 3, 4), QTime(8, 0), QTimeZone::LocalTime);
        QVERIFY(!AchievementStore::isUnlocked(QStringLiteral("rides_10")));
        QVERIFY(AchievementStore::unlock(QStringLiteral("rides_10"), when));
        QVERIFY(!AchievementStore::unlock(QStringLiteral("rides_10")));
        QVERIFY(AchievementStore::isUnlocked(QStringLiteral("rides_10")));
        QCOMPARE(AchievementStore::unlocked().value(QStringLiteral("rides_10")), when);
    }
};

QTEST_GUILESS_MAIN(TstAchievements)
#include "tst_achievements.moc"
