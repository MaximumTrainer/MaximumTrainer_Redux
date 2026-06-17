#include "managerachievement.h"
#include "util.h"
#include "achievementchecker.h"


// NOTE: achievements were loaded from / saved to the maximumtrainer.com REST
// backend, which no longer exists. That backend code (AchievementDAO) has been
// removed. The list is therefore never populated, so the size guards below
// short-circuit and the in-workout achievement popup stays dormant. The logic
// is kept in place for a future local-only re-implementation.
ManagerAchievement::ManagerAchievement(QObject *parent) : QObject(parent)
{

    this->account = qApp->property("Account").value<Account*>();


    // number of achievements in the DB at the moment, increase when adding more
    numberOfAchievement = 7;
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ManagerAchievement::checkMAPAchievement(int lastStepCompleted) {


    qDebug() << "checkMAPAchievement" << lastStepCompleted;

    if (lastStepCompleted < 6)
        return;

    // list could be empty if fetch achievement from DB failed
    if (lstAchievement.size() < numberOfAchievement) {
        return;
    }

    // Step 6 completed Achievement
    int indexVal = 5;
    if (lastStepCompleted == 6  && !lstAchievement[indexVal].isCompleted() ) {
        lstAchievement[indexVal].setCompleted(true);
        emit achievementCompleted(lstAchievement.at(indexVal));    }

    // Step 7 completed Achievement
    indexVal = 6;
    if (lastStepCompleted == 7  && !lstAchievement[indexVal].isCompleted() ) {
        lstAchievement[indexVal].setCompleted(true);
        emit achievementCompleted(lstAchievement.at(indexVal));    }



}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ManagerAchievement::updateMinuteRode(int minutes) {

    account->minutes_rode += minutes;
}




////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ManagerAchievement::workoutCompleted(Workout workout) {

    // list could be empty if fetch achievement from DB failed
    if (lstAchievement.size() < numberOfAchievement) {
        return;
    }

    int secTotal = Util::convertQTimeToSecD(workout.getDurationQTime());


    // Endurance Starter (1hr)
    int indexVal = 0;
    if (!lstAchievement[indexVal].isCompleted() && secTotal >= 60*60) {  //60*60=1h
        lstAchievement[indexVal].setCompleted(true);
        emit achievementCompleted(lstAchievement.at(indexVal));    }

    // Endurance Intermediate (2hr)
    indexVal = 1;
    if (!lstAchievement[indexVal].isCompleted() && secTotal >= 60*60*2) {
        lstAchievement[indexVal].setCompleted(true);
        emit achievementCompleted(lstAchievement.at(indexVal));    }

    // Endurance Master (3hr)
    indexVal = 2;
    if (!lstAchievement[indexVal].isCompleted() && secTotal >= 60*60*3) {
        lstAchievement[indexVal].setCompleted(true);
        emit achievementCompleted(lstAchievement.at(indexVal));    }

    // Target Maniac (all target present in a workout)
    indexVal = 3;
    if (!lstAchievement[indexVal].isCompleted() && AchievementChecker::checkTargetManiac(workout)) {
        lstAchievement[indexVal].setCompleted(true);
        emit achievementCompleted(lstAchievement.at(indexVal));    }

    // Learning to test (FTP Test or FTP_8min Test done)
    indexVal = 4;
    if (!lstAchievement[indexVal].isCompleted() && (workout.getWorkoutNameEnum() == Workout::FTP_TEST || workout.getWorkoutNameEnum() == Workout::FTP8min_TEST) ) {
        lstAchievement[indexVal].setCompleted(true);
        emit achievementCompleted(lstAchievement.at(indexVal));    }





}


