#include <QtTest>
#include <QSet>
#include <QStandardPaths>
#include <QTimeZone>

#include "achievementcatalog.h"
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
        QVERIFY(all().size() >= 25);
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
        QCOMPARE(categories.size(), 5);
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
