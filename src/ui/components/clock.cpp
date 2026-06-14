#include "clock.h"
#include "myconstants.h"
#include "cyclingphysics.h"   // shared power→speed model (also used by the retro race)

#include <QDebug>

Clock::Clock(QString objectName, QObject *parent) :
    QObject(parent)
{

    this->setObjectName(objectName);
    totalTimeElapsed_msec = 0;
    lastSecond = 0;
    totalTimePaused_msec = 0;


    nbUser = 1;

    cda = QVector<double>(constants::nbMaxUserStudio, 30);
    weightKG = QVector<double>(constants::nbMaxUserStudio, 80);
    currentSpeedMS = QVector<double>(constants::nbMaxUserStudio, 0);
    currentPowerWatts = QVector<int>(constants::nbMaxUserStudio, 0);
    currentSlope = QVector<double>(constants::nbMaxUserStudio, 0);


    timerClock = new QTimer(this);
    connect(timerClock, SIGNAL(timeout()), this, SLOT(timerClockTimeout()) );

    //    d_clock.start();
    //    pauseClock();

    // for virtual speed
    timerSpeed = new QTimer(this);
    connect(timerSpeed, SIGNAL(timeout()), this, SLOT(timerSpeedTimeout()) );




}




//////////////////////////////////////////////////////////////////////////
void Clock::startClock()  {

    qDebug() << "starting clock object";

    d_clock.start();
    timerClock->start(timerClockMs);
}

//-------------------------------------------------------------------
void Clock::finishClock() {

    emit finished();
}



//----------------------------
void Clock::pauseClock() {

    pauseActivatedAt_time_msec = d_clock.elapsed();
    qDebug() << "Current time when paused:" << pauseActivatedAt_time_msec;

    timerClock->stop();
}

//----------------------------
void Clock::resumeClock() {

    resumeActivatedAt_time_msec = d_clock.elapsed();
    qDebug() << "Current time when resumed:" << resumeActivatedAt_time_msec;
    totalTimePaused_msec += resumeActivatedAt_time_msec - pauseActivatedAt_time_msec;
    qDebug() << "Workout was paused for:" << totalTimePaused_msec;

    emit updateTimePaused(totalTimePaused_msec);

    timerClock->start(timerClockMs);
}


//-------------------------------------------------------------------
void Clock::timerClockTimeout() {


    totalTimeElapsed_msec = d_clock.elapsed() - totalTimePaused_msec;
    int timeElapsed_sec = totalTimeElapsed_msec/1000;


    emit oneCyclePassed(totalTimeElapsed_msec);


    // Check if 1 sec has passed
    int currentSecond = ((int)timeElapsed_sec) % 10;
    if (currentSecond != lastSecond) {
        emit oneSecPassed(timeElapsed_sec);
    }

    lastSecond = currentSecond;
}











/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Clock::startClockSpeed() {

    qDebug() << "STarting clock speed!";

    // initiale virtual speed calculation, calculated at timerSpeedMs Interval
    totalTimeElapsedLast = 0;
    firstSpeedCheck = true;

    d_clock_speed.start();
    timerSpeed->start(timerSpeedMs);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Clock::receiveUserInfo(int userID, double cda, double weightKG, int nbUser) {

    qDebug() << "OK RECEIVED USER INFO!" << userID << "cda" << cda << "w:" << weightKG << "nbUser" << nbUser;

    this->cda[userID-1] = cda;
    this->weightKG[userID-1] = weightKG;


    this->currentSpeedMS[userID-1] = 0;
    this->currentPowerWatts[userID-1] = 0;
    this->currentSlope[userID-1] = 0.0;


    this->nbUser = nbUser;


}


// Speed Related
/////////////////////////////////////////////////////////////////////////
void Clock::receivePowerData(int userID, int powerWatts) {

    this->currentPowerWatts[userID-1] = powerWatts;
}

/////////////////////////////////////////////////////////////////////////
void Clock::receiveSlopeData(int userID, double slope) {


    this->currentSlope[userID-1] = slope;
}



/////////////////////////////////////////////////////////////////////////
void Clock::timerSpeedTimeout() {

    double totalTimeElapsedNow = d_clock_speed.elapsed();
    double diffTime = totalTimeElapsedNow - totalTimeElapsedLast;
    //diffTime , always smaller than 1 second

    //    qDebug() << "totalTimeElapsedNow" << totalTimeElapsedNow << "totalTimeElapsedLast" << totalTimeElapsedLast << "diffTime" << diffTime;

    if (!firstSpeedCheck) {

        double fractionSecond = diffTime / 1000.0;

        //        for (int i=0; i<nbUser; i++) {
        for (int i=0; i<1; i++) {

            //            qDebug() << "Calculating speed for user:" << i;

            // Flat-road power→speed via the shared CyclingPhysics model (the same
            // one the retro race integrates), so virtual speed and the race agree.
            // Uses rider+bike mass (weightKG) and the rider's CdA.
            currentSpeedMS[i] = CyclingPhysics::stepSpeed(
                currentSpeedMS[i], currentPowerWatts[i], weightKG[i], cda[i], fractionSecond);


            //        qDebug() << "fractionSecond:" << fractionSecond << "diffTime" << diffTime;

            //        qDebug() << "Virtual Speed Debug::" << "currentPowerWatts" << currentPowerWatts <<
            //                    "powerRequiredRolling" << powerRequiredRolling << "powerRequiredWind" << powerRequiredWind << "powerRequiredGravity" <<
            //                    powerRequiredGravity << "accelerationMs" << accelerationMs << "currentSpeedMS" <<  currentSpeedMS << "diffTime" << diffTime;


        }

        //        for (int i=0; i<nbUser; i++) {
        for (int i=0; i<1; i++) {
            //            qDebug() << "emiting speed for user:" << i+1 << "is" << currentSpeedMS[i];
            emit virtualSpeed(i+1, currentSpeedMS[i], fractionSecond);
        }




    }


    totalTimeElapsedLast = totalTimeElapsedNow;
    firstSpeedCheck = false;
}




