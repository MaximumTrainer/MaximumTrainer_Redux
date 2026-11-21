#include "managerachievement.h"

#include <QCoreApplication>
#include <QDir>
#ifndef GC_WASM_BUILD
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#endif

#include "achievementcatalog.h"
#include "achievementchecker.h"
#include "account.h"
#include "achievementstore.h"
#include "fitactivityreader.h"
#include "logger.h"
#include "mmpcalculator.h"
#include "powerbests.h"
#include "setting_keys.h"
#include "util.h"
#include "workouthistorysummary.h"

using namespace AchievementCatalog;

namespace {

QList<WorkoutHistorySummary> readHistory()
{
    QList<WorkoutHistorySummary> history;
    const QDir dir(Util::getSystemPathHistory());
    for (const QString &file : dir.entryList({QStringLiteral("*.fit")}, QDir::Files)) {
        WorkoutHistorySummary summary = FitActivityReader::readFile(dir.filePath(file));
        if (summary.valid)
            history.append(summary);
    }
    return history;
}

QVector<PersonalBestTracker::Record> loadBestRecords()
{
    const QList<PowerBests::Ride> rides = PowerBests::loadRides(readHistory());
    const QVector<PowerBests::Effort> allTime = PowerBests::bestEfforts(rides);
    const QVector<PowerBests::Effort> recent = PowerBests::bestEfforts(
        rides, QDateTime::currentDateTime().addDays(-PersonalBestTracker::kRecentDays));
    QVector<PersonalBestTracker::Record> records;
    for (int seconds : PersonalBestTracker::trackedDurations()) {
        const qsizetype index = MMP_DURATIONS.indexOf(seconds);
        if (index >= 0)
            records.append({seconds, recent[index].watts, allTime[index].watts});
    }
    return records;
}

} // namespace

ManagerAchievement::ManagerAchievement(QObject *parent) : QObject(parent)
{
}

void ManagerAchievement::award(const QString &key, bool announce)
{
    const Definition *definition = find(key);
    if (!definition || !AchievementStore::unlock(key))
        return;
    LOG_INFO("Achievements", QStringLiteral("earned: ") + key);
    if (announce)
        emit achievementCompleted(Achievement(0, definition->name, definition->description, QString()));
    emit achievementsChanged();
}

void ManagerAchievement::evaluateHistory(bool announce)
{
    m_history = readHistory();
    m_stats = statsFrom(m_history);
    m_stats.bestGoalWeeks = goalStreakWeeks(m_history, weeklyGoalHours());
    if (const Account *account = qApp->property("Account").value<Account*>();
        account && account->weight_kg > 0 && account->FTP > 0)
        m_stats.ftpWattsPerKg = account->FTP / account->weight_kg;
    for (const Definition &definition : all())
        if (reached(definition, m_stats))
            award(definition.key, announce);
    emit achievementsChanged();
}

double ManagerAchievement::weeklyGoalHours()
{
    return AppSettings::read(SettingKeys::Training::WeeklyGoalHours);
}

void ManagerAchievement::setWeeklyGoalHours(double hours)
{
    AppSettings::write(SettingKeys::Training::WeeklyGoalHours, std::max(0.0, hours));
    LOG_INFO("Achievements", QStringLiteral("weekly goal set to %1 h").arg(hours));
    emit weeklyGoalChanged();
    evaluateHistory(false);   // goal streak achievements follow the goal
}

void ManagerAchievement::recordEvent(const QString &key)
{
    award(key, true);
}

void ManagerAchievement::workoutCompleted(Workout workout)
{
    const Workout::WORKOUT_NAME type = workout.getWorkoutNameEnum();
    if (type == Workout::FTP_TEST || type == Workout::FTP8min_TEST || type == Workout::MAP_TEST)
        recordEvent(Events::FirstTest);
    if (AchievementChecker::checkTargetManiac(workout))
        recordEvent(Events::TargetManiac);
}

void ManagerAchievement::checkMAPAchievement(int lastStepCompleted)
{
    if (lastStepCompleted >= 6)
        recordEvent(Events::MapStep6);
    if (lastStepCompleted >= 7)
        recordEvent(Events::MapStep7);
}

void ManagerAchievement::checkLiveMinutes(int elapsedMinutes)
{
    for (const Definition &definition : all())
        if (definition.kind == Kind::RideMinutes && elapsedMinutes >= definition.target)
            award(definition.key, true);
}

void ManagerAchievement::checkLiveTss(double tss)
{
    for (const Definition &definition : all())
        if (definition.kind == Kind::RideTss && tss >= definition.target)
            award(definition.key, true);
}

void ManagerAchievement::checkLiveSecond(double powerWatts, double cadenceRpm, double ftpWatts)
{
    for (const QString &key : m_liveTracker.addSecond(powerWatts, cadenceRpm, ftpWatts))
        award(key, true);
    announceBests(m_bestTracker.addSecond(powerWatts));
}

void ManagerAchievement::checkLiveDistance(double meters)
{
    for (const Definition &definition : all())
        if (definition.kind == Kind::RideKm && meters / 1000.0 >= definition.target)
            award(definition.key, true);
}

void ManagerAchievement::startLiveRide()
{
    m_liveTracker.reset();
    m_bestTracker.stop();
    const int generation = ++m_bestLoadGeneration;
    const auto begin = [this, generation](const QVector<PersonalBestTracker::Record> &records) {
        if (generation != m_bestLoadGeneration)
            return;  // a newer ride started meanwhile
        m_bestTracker.start(records);
        QStringList summary;
        for (const PersonalBestTracker::Record &record : records)
            summary << QStringLiteral("%1 %2/%3 W").arg(PersonalBestTracker::durationLabel(record.seconds))
                           .arg(qRound(record.recentWatts)).arg(qRound(record.allTimeWatts));
        LOG_INFO("Achievements", QStringLiteral("power bests (90-day/all-time): ") + summary.join(QStringLiteral(", ")));
    };
#ifdef GC_WASM_BUILD
    begin(loadBestRecords());
#else
    // Decoding rides that are not cached yet can take a few seconds.
    auto *watcher = new QFutureWatcher<QVector<PersonalBestTracker::Record>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [watcher, begin] {
        begin(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(loadBestRecords));
#endif
}

void ManagerAchievement::finishLiveRide()
{
    announceBests(m_bestTracker.finish());
    m_bestTracker.stop();
}

void ManagerAchievement::announceBests(const QVector<PersonalBestTracker::Event> &events)
{
    for (const PersonalBestTracker::Event &event : events) {
        const QString duration = PersonalBestTracker::durationLabel(event.seconds);
        const int watts = qRound(event.watts);
        const int previous = qRound(event.previousWatts);
        const QString name = event.allTime
            ? tr("New all-time %1 best: %2 W").arg(duration).arg(watts)
            : tr("New 90-day %1 best: %2 W").arg(duration).arg(watts);
        const QString description = event.allTime
            ? tr("Your previous best ever was %1 W.").arg(previous)
            : tr("Your best of the last 90 days was %1 W.").arg(previous);
        LOG_INFO("Achievements", name + QStringLiteral(" (was %1 W)").arg(previous));
        emit achievementCompleted(Achievement(0, name, description, QString()));
    }
}
