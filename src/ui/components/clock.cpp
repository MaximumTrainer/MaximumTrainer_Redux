#include "clock.h"
#include "myconstants.h"
#include "cyclingphysics.h"


Clock::Clock(QString objectName, QObject *parent) :
    QObject(parent)
{

    this->setObjectName(objectName);
    totalTimeElapsed_msec = 0;
    lastSecond = 0;
    totalTimePaused_msec = 0;




    timerClock = new QTimer(this);
    connect(timerClock, &QTimer::timeout, this, &Clock::timerClockTimeout);


    // for virtual speed
    timerSpeed = new QTimer(this);
    connect(timerSpeed, &QTimer::timeout, this, &Clock::timerSpeedTimeout);


}


void Clock::startClock()  {


    d_clock.start();
    timerClock->start(timerClockMs);
}

void Clock::finishClock() {

    emit finished();
}


void Clock::pauseClock() {

    pauseActivatedAt_time_msec = d_clock.elapsed();

    timerClock->stop();
}

void Clock::resumeClock() {

    resumeActivatedAt_time_msec = d_clock.elapsed();
    totalTimePaused_msec += resumeActivatedAt_time_msec - pauseActivatedAt_time_msec;

    emit updateTimePaused(totalTimePaused_msec);

    timerClock->start(timerClockMs);
}


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


void Clock::startClockSpeed() {


    // initiale virtual speed calculation, calculated at timerSpeedMs Interval
    totalTimeElapsedLast = 0;
    firstSpeedCheck = true;

    d_clock_speed.start();
    timerSpeed->start(timerSpeedMs);
}


void Clock::receiveUserInfo(double cda, double weightKG) {
    this->cda = cda;
    this->weightKG = weightKG;
    currentSpeedMS = 0;
    currentPowerWatts = 0;
    currentSlope = 0.0;
}


// Speed Related
void Clock::receivePowerData(int userID, int powerWatts) {
    if (userID == 1)
        currentPowerWatts = powerWatts;
}

void Clock::receiveSlopeData(int userID, double slope) {
    if (userID == 1)
        currentSlope = slope;
}


void Clock::timerSpeedTimeout() {

    double totalTimeElapsedNow = d_clock_speed.elapsed();
    double diffTime = totalTimeElapsedNow - totalTimeElapsedLast;
    //diffTime , always smaller than 1 second


    if (!firstSpeedCheck) {

        double fractionSecond = diffTime / 1000.0;

        // Flat-road power→speed via the CyclingPhysics model, using rider+bike
        // mass (weightKG) and the rider's CdA.
        currentSpeedMS = CyclingPhysics::stepSpeed(currentSpeedMS, currentPowerWatts, weightKG, cda, fractionSecond);
        emit virtualSpeed(1, currentSpeedMS, fractionSecond);


    }


    totalTimeElapsedLast = totalTimeElapsedNow;
    firstSpeedCheck = false;
}

