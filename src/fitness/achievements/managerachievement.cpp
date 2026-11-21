#include "managerachievement.h"

#include <QCoreApplication>
#include <QDir>

#include "achievementcatalog.h"
#include "achievementchecker.h"
#include "account.h"
#include "achievementstore.h"
#include "fitactivityreader.h"
#include "logger.h"
#include "util.h"
#include "workouthistorysummary.h"

using namespace AchievementCatalog;

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
    QList<WorkoutHistorySummary> history;
    const QDir dir(Util::getSystemPathHistory());
    for (const QString &file : dir.entryList({QStringLiteral("*.fit")}, QDir::Files)) {
        WorkoutHistorySummary summary = FitActivityReader::readFile(dir.filePath(file));
        if (summary.valid)
            history.append(summary);
    }
    m_stats = statsFrom(history);
    if (const Account *account = qApp->property("Account").value<Account*>();
        account && account->weight_kg > 0 && account->FTP > 0)
        m_stats.ftpWattsPerKg = account->FTP / account->weight_kg;
    for (const Definition &definition : all())
        if (reached(definition, m_stats))
            award(definition.key, announce);
    emit achievementsChanged();
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
}
