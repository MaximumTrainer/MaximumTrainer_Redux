#include "clock.h"
#include "myconstants.h"

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

            double powerRequiredRolling = calculateProlling(i);
            double powerRequiredWind = calculatePwind(i);
            double powerRequiredGravity = calculatePgravity(i);

            // Net power available to accelerate relates to speed by
            // P_net = m·v·(dv/dt), so dv/dt = P_net / (m·v) — NOT P_net / m
            // (which omits the ·v and makes acceleration independent of speed).
            // Floor v in the denominator to avoid a div-by-zero / runaway push
            // from a standstill.
            double speedForAccel = currentSpeedMS[i] < 0.5 ? 0.5 : currentSpeedMS[i];
            double netPowerW = currentPowerWatts[i]
                             - (powerRequiredRolling + powerRequiredWind + powerRequiredGravity);
            double accelerationMs = netPowerW / (weightKG[i] * speedForAccel);
            accelerationMs = accelerationMs * fractionSecond;
            currentSpeedMS[i] = currentSpeedMS[i] + accelerationMs;
            if (currentSpeedMS[i] < 0)
                currentSpeedMS[i] = 0;


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




//P = Crr x N x v,
//------------------------------------------------------------------------------
//user ID is 0 to 39
double Clock::calculateProlling(int pos) {

    if (currentSpeedMS[pos] <= 0)
        return 0;


    return (0.005 * 9.8067 * weightKG[pos] * currentSpeedMS[pos]);

    // insignifiant under 1 m/s, still calculate so user get to 0 eventually if stop pedalling
    //    if (currentSpeedMS < 1)
    //        return powerRequiredRolling + 1;

}


//P = 0.5 x ρ x v3 x Cd x A,
//------------------------------------------------------------------------------
double Clock::calculatePwind(int pos) {

    // insignifiant under 1 m/s
    if (currentSpeedMS[pos] < 1)
        return 0;

    return (0.5 * 1.275 * (currentSpeedMS[pos] * currentSpeedMS[pos] * currentSpeedMS[pos]) * cda[pos]);
}

//https://strava.zendesk.com/entries/20959332-Power-Calculations
//P = m x g x sin(arctan(grade)) x v
//------------------------------------------------------------------------------
double Clock::calculatePgravity(int pos) {

    // 0 for now, will adjust with slope later
    Q_UNUSED(pos);
    return 0;
}

