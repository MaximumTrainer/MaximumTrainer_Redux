#include "dataworkout.h"

#include <QTimer>
#include <QObject>

#include "datacadence.h"
#include "datapower.h"
#include "dataheartrate.h"
#include "dataspeed.h"
#include "util.h"
#include "myconstants.h"



DataWorkout::~DataWorkout() {

    qDebug() << "Destructor DataWorkout";


    closeFitFile();
}

////////////////////////////////////////////////////////////////////////////////////////
DataWorkout::DataWorkout(Workout workout, int userftp, QObject *parent) : QObject(parent)  {

    this->workout = workout;
    this->ftp = userftp;
    startTimeWorkout.setTimeSpec(Qt::UTC);

    nbLap = 0;
    normalizedPower = 0;
    intensityFactor = 0;
    trainingStressScore = 0;


    /// Hr
    avgIntervalHr = 0;
    maxIntervalHr = 0;
    nbPointsIntervalHr = 0;

    avgWorkoutHr = 0;
    maxWorkoutHr = 0;
    nbPointsWorkoutHr = 0;


    /// Cad
    avgIntervalCad = 0;
    maxIntervalCad = 0;
    nbPointsIntervalCad = 0;

    avgWorkoutCad = 0;
    maxWorkoutCad = 0;
    nbPointsWorkoutCad = 0;


    /// Speed
    avgIntervalSpeed = 0;
    maxIntervalSpeed = 0;
    nbPointsIntervalSpeed = 0;

    avgWorkoutSpeed = 0;
    maxWorkoutSpeed = 0;
    nbPointsWorkoutSpeed = 0;


    /// Power
    avgIntervalPower = 0;
    maxIntervalPower = 0;
    nbPointsIntervalPower = 0;

    avgWorkoutPower = 0;
    maxWorkoutPower = 0;
    nbPointsWorkoutPower = 0;



    /// Calories
    calories = 0.0;
    lapCalories = 0.0;

    /// Distance
    totalMeters = 0.0;


    /// Workout Stats
    nbPoints30SecPower = 0;
    last30secMean = 0;
    nb30secMean = 0;
    totalSumExp4 = 0;
    normalizedPower = 0;
    intensityFactor = 0;
    trainingStressScore = 0;


    ///Last Used Value (to fix dropouts)
    lastHr = -1;
    lastCadence = -1;
    lastSpeed = -1;
    lastPower = -1;
    lastRightPedal = -1;
    lastLeftTorqueEff = -1;
    lastRightTorqueEff = -1;
    lastLeftPedalSmooth = -1;
    lastRightPedalSmooth = -1;
    lastCombinedPedalSmooth = -1;
    lastSaturatedHemoglobinPercent = -1;
    lastTotalHemoglobinConc = -1;

    numberTimeUsedLastHr = 0;
    numberTimeUsedLastCadence = 0;
    numberTimeUsedLastSpeed = 0;
    numberTimeUsedLastPower = 0;
    numberTimeUsedLastRightPedal = 0;
    numberTimeUsedLastLeftTorqueEff = 0;
    numberTimeUsedLastSaturatedHemoglobinPercent = 0;





}












///////////////////////////////////////////////////////////////////////////////////////////
void DataWorkout::setStartTimeWorkout(QDateTime dt) {

    this->startTimeWorkout = dt;
}

//----------------------------------------------------------------------------
void DataWorkout::initFitFile(bool createDir, QString username, QString nameWorkout, QDateTime timeStarted) {
    fitCreator.initialize_FIT_File(createDir, username, nameWorkout, timeStarted);
}
void DataWorkout::closeFitFile() {
    fitCreator.close_FIT_File();
}
void DataWorkout::closeAndDeleteFitFile() {
    fitCreator.closeAndDeleteFile();
}
void DataWorkout::writeEndFile(double timeNow) {
    fitCreator.writeEndFile(timeNow, avgWorkoutSpeed, avgWorkoutCad, avgWorkoutPower, avgWorkoutHr,
                            maxWorkoutSpeed, maxWorkoutCad, maxWorkoutPower, maxWorkoutHr,
                            calories, normalizedPower, intensityFactor, trainingStressScore,
                            nbLap);

}
//----------------------------------------------------------------------------
QString DataWorkout::getFitFilename() {

    if (fitCreator.getFileIsComplete())
        return fitCreator.getFilename();
    else return "";
}



//-----------------------------------------------------------------------------------------------
void DataWorkout::updateData(bool studioMode, int secReceived,
                             double averageHr1sec, double averageCadence1sec, double averageSpeed1sec, double averagePower1sec,
                             double rightPedal, double avgLeftTorqueEff, double avgRightTorqueEff, double avgLeftPedalSmooth, double avgRightPedalSmooth, double avgCombinedPedalSmooth,
                             double saturatedHemoglobinPercent, double totalHemoglobinConc) {


    bool writeRecord = false;
    //    qDebug() << secReceived << "update Data DataWorkout" <<
    //                "HR:" << averageHr1sec << "CAD:" << averageCadence1sec << "Speed:" << averageSpeed1sec<< "Power:" << averagePower1sec <<
    //                "rightPedal:" << rightPedal << "saturatedHemoglobinPercent:" << saturatedHemoglobinPercent << "totalHemoglobinConc:" << totalHemoglobinConc;


    // --- HR ------------------------------------------------------------------
    if (averageHr1sec != -1) {
        if (!studioMode)
            DataHeartRate::instance().append( QPointF( secReceived, averageHr1sec) );
        hrDataReceived(averageHr1sec);
        writeRecord = true;
        lastHr = averageHr1sec;
        numberTimeUsedLastHr = 0;
    }
    else {
        if (lastHr != -1 && numberTimeUsedLastHr < constants::nbMaxRepeatHrPointFitFile) {
            writeRecord = true;
            averageHr1sec = lastHr;
            numberTimeUsedLastHr++;
        }
    }
    // --- Cadence ------------------------------------------------------------------
    if (averageCadence1sec != -1) {
        if (!studioMode)
            DataCadence::instance().append( QPointF( secReceived, averageCadence1sec) );
        cadDataReceived(averageCadence1sec);
        writeRecord = true;
        lastCadence = averageCadence1sec;
        numberTimeUsedLastCadence = 0;
    }
    else {
        if (lastCadence != -1 && numberTimeUsedLastCadence < constants::nbMaxRepeatPointFitFile) {
            writeRecord = true;
            averageCadence1sec = lastCadence;
            numberTimeUsedLastCadence++;
        }
    }
    // --- Speed ------------------------------------------------------------------
    if (averageSpeed1sec != -1) {
        if (!studioMode)
            DataSpeed::instance().append( QPointF( secReceived, averageSpeed1sec) );
        speedDataReceived(averageSpeed1sec);
        writeRecord = true;
        lastSpeed = averageSpeed1sec;
        numberTimeUsedLastSpeed = 0;
    }
    else {
        if (lastSpeed != -1 && numberTimeUsedLastSpeed < constants::nbMaxRepeatPointFitFile) {
            writeRecord = true;
            averageSpeed1sec = lastSpeed;
            numberTimeUsedLastSpeed++;
        }
    }
    // --- Power ------------------------------------------------------------------
    if (averagePower1sec != -1) {
        if (!studioMode)
            DataPower::instance().append( QPointF( secReceived, averagePower1sec) );
        powerDataReceived(secReceived, averagePower1sec);
        writeRecord = true;
        lastPower = averagePower1sec;
        numberTimeUsedLastPower = 0;
    }
    else {
        if (lastPower != -1 && numberTimeUsedLastPower < constants::nbMaxRepeatPointFitFile) {
            writeRecord = true;
            averagePower1sec = lastPower;
            numberTimeUsedLastPower++;
        }
    }
    // -- RightPedalPower ------------------------------------------------------------------
    if (rightPedal != -1) {
        writeRecord = true;
        lastRightPedal = rightPedal;
        numberTimeUsedLastRightPedal = 0;
    }
    else {
        if (lastRightPedal != -1 && numberTimeUsedLastRightPedal < constants::nbMaxRepeatPointFitFile) {
            writeRecord = true;
            rightPedal = lastRightPedal;
            numberTimeUsedLastRightPedal++;      }
    }
    // -- Pedal Metric ------------------------------------------------------------------
    if (avgLeftTorqueEff != -1) {
        writeRecord = true;
        lastLeftTorqueEff = avgLeftTorqueEff;
        lastRightTorqueEff = avgRightTorqueEff;
        lastLeftPedalSmooth = avgLeftPedalSmooth;
        lastRightPedalSmooth = avgRightPedalSmooth;
        lastCombinedPedalSmooth = avgCombinedPedalSmooth;
        numberTimeUsedLastLeftTorqueEff = 0;
    }
    else {
        if (lastLeftTorqueEff != -1 && numberTimeUsedLastLeftTorqueEff < constants::nbMaxRepeatPointFitFile) {
            writeRecord = true;
            avgLeftTorqueEff = lastLeftTorqueEff;
            avgRightTorqueEff = lastRightTorqueEff;
            avgLeftPedalSmooth = lastLeftPedalSmooth;;
            avgRightPedalSmooth = lastRightPedalSmooth;
            avgCombinedPedalSmooth = lastCombinedPedalSmooth;
            numberTimeUsedLastLeftTorqueEff++;
        }
    }
    // -- Muscle Oxygen ------------------------------------------------------------------
    if (saturatedHemoglobinPercent != -1 && totalHemoglobinConc!= -1) {
        writeRecord = true;
        lastSaturatedHemoglobinPercent = saturatedHemoglobinPercent;
        lastTotalHemoglobinConc = totalHemoglobinConc;
        numberTimeUsedLastSaturatedHemoglobinPercent = 0;
    }
    else {
        if (lastSaturatedHemoglobinPercent != -1 && numberTimeUsedLastSaturatedHemoglobinPercent < constants::nbMaxRepeatPointFitFile) {
            writeRecord = true;
            saturatedHemoglobinPercent = lastSaturatedHemoglobinPercent;
            totalHemoglobinConc = lastTotalHemoglobinConc;
            numberTimeUsedLastSaturatedHemoglobinPercent++;
        }
    }



    if (writeRecord) {
        fitCreator.writeRecord(secReceived,
                               averageHr1sec, averageCadence1sec, averageSpeed1sec, averagePower1sec,
                               rightPedal, avgLeftTorqueEff, avgRightTorqueEff, avgLeftPedalSmooth,avgRightPedalSmooth, avgCombinedPedalSmooth,
                               saturatedHemoglobinPercent, totalHemoglobinConc);
    }







}


//-----------------------------------------------------------------------------------------------
void DataWorkout::checkUpdateMaxHr(int value) {

    // Update Max Interval & Max Workout
    if (value > maxIntervalHr) {
        emit maxHrIntervalChanged(value);
        maxIntervalHr = value;
        if (value > maxWorkoutHr) {
            emit maxHrWorkoutChanged(value);
            maxWorkoutHr = value;
        }
    }
}

//-----------------------------------------------------------------------------------------------
void DataWorkout::checkUpdateMaxCad(int value) {

    // Update Max Interval & Max Workout
    if (value > maxIntervalCad) {
        emit maxCadenceIntervalChanged(value);
        maxIntervalCad = value;
        if (value > maxWorkoutCad) {
            emit maxCadenceWorkoutChanged(value);
            maxWorkoutCad = value;
        }
    }

}

//-----------------------------------------------------------------------------------------------
void DataWorkout::checkUpdateMaxSpeed(double value) {

    // Update Max Interval & Max Workout
    if (value > maxIntervalSpeed) {
        emit maxSpeedIntervalChanged(value);
        maxIntervalSpeed = value;
        if (value > maxWorkoutSpeed) {
            emit maxSpeedWorkoutChanged(value);
            maxWorkoutSpeed = value;
        }
    }
}

//-----------------------------------------------------------------------------------------------
void DataWorkout::checkUpdateMaxPower(int value) {

    // Update Max Interval & Max Workout
    if (value > maxIntervalPower) {
        emit maxPowerIntervalChanged(value);
        maxIntervalPower = value;
        if (value > maxWorkoutPower) {
            emit maxPowerWorkoutChanged(value);
            maxWorkoutPower = value;
        }
    }
}

//-----------------------------------------------------------------------------------------------
void DataWorkout::updateDistance(double metersToAdd) {

//    qDebug() << "updateDistanceDone" << "metersToAdd" << metersToAdd << "totalMeters" << totalMeters;
    totalMeters += metersToAdd;

    emit totalDistanceChanged(totalMeters);

}






//-----------------------------------------------------------------------------------------------
void DataWorkout::changeInterval(double timeStarted, double timeNow, int timePaused_msec, bool workoutOver, bool testInterval) {


    qDebug() << "CHANGE INTERVAL! timeStarted:" << timeStarted << "timeNow" << timeNow;



    fitCreator.writeLapMesg(timeStarted, timeNow, timePaused_msec,
                            avgIntervalSpeed, avgIntervalCad, avgIntervalPower, avgIntervalHr,
                            maxIntervalSpeed, maxIntervalCad, maxIntervalPower, maxIntervalHr,
                            lapCalories);


    meanHr.append(avgIntervalHr);
    meanCad.append(avgIntervalCad);
    meanSpeed.append(avgIntervalSpeed);
    meanPower.append(avgIntervalPower);

    if (testInterval) {
        meanHrTest.append(avgIntervalHr);
        meanCadTest.append(avgIntervalCad);
        meanSpeedTest.append(avgIntervalSpeed);
        meanPowerTest.append(avgIntervalPower);
    }

    //    qDebug() << "OK LST OF DATA IN MEANPOWER IS:";
    //    for (int i =0; i<meanPower.size(); i++) {
    //        qDebug() << "MEAN:" << i << "IS:" << meanPower.at(i);
    //    }


    // Reset Interval Values
    if (!workoutOver) {
        avgIntervalHr = 0;
        maxIntervalHr = 0;
        nbPointsIntervalHr = 0;

        avgIntervalCad = 0;
        maxIntervalCad = 0;
        nbPointsIntervalCad = 0;

        avgIntervalSpeed = 0;
        maxIntervalSpeed = 0;
        nbPointsIntervalSpeed = 0;

        avgIntervalPower = 0;
        maxIntervalPower = 0;
        nbPointsIntervalPower = 0;

        lapCalories = 0;
    }
    nbLap++;

}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DataWorkout::hrDataReceived(double value) {


    // Update Avg Workout
    if (nbPointsWorkoutHr == 0) {
        avgWorkoutHr = value;
    }
    else {
        double firstEle = avgWorkoutHr *((double)nbPointsWorkoutHr/(nbPointsWorkoutHr+1));
        double secondEle = ((double)value)/(nbPointsWorkoutHr+1);
        avgWorkoutHr = firstEle + secondEle;
    }


    // Update Avg interval
    if (nbPointsIntervalHr == 0) {
        avgIntervalHr = value;
    }
    else {
        double firstEle = avgIntervalHr *((double)nbPointsIntervalHr/(nbPointsIntervalHr+1));
        double secondEle = ((double)value)/(nbPointsIntervalHr+1);
        avgIntervalHr = firstEle + secondEle;
    }

    nbPointsIntervalHr++;
    nbPointsWorkoutHr++;

    emit avgHrWorkoutChanged(avgWorkoutHr);
    emit avgHrIntervalChanged(avgIntervalHr);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DataWorkout::cadDataReceived(double value) {


    // Update Avg Workout
    if (nbPointsWorkoutCad == 0) {
        avgWorkoutCad = value;
    }
    else {
        double firstEle = avgWorkoutCad *((double)nbPointsWorkoutCad/(nbPointsWorkoutCad+1));
        double secondEle = ((double)value)/(nbPointsWorkoutCad+1);
        avgWorkoutCad = firstEle + secondEle;
    }


    // Update Avg interval
    if (nbPointsIntervalCad == 0) {
        avgIntervalCad = value;
    }
    else {
        double firstEle = avgIntervalCad *((double)nbPointsIntervalCad/(nbPointsIntervalCad+1));
        double secondEle = ((double)value)/(nbPointsIntervalCad+1);
        avgIntervalCad = firstEle + secondEle;
    }

    nbPointsIntervalCad++;
    nbPointsWorkoutCad++;

    emit avgCadenceWorkoutChanged(avgWorkoutCad);
    emit avgCadenceIntervalChanged(avgIntervalCad);
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DataWorkout::speedDataReceived(double value) {


    // Update Avg Workout
    if (nbPointsWorkoutSpeed == 0) {
        avgWorkoutSpeed = value;
    }
    else {
        double firstEle = avgWorkoutSpeed *((double)nbPointsWorkoutSpeed/(nbPointsWorkoutSpeed+1));
        double secondEle = ((double)value)/(nbPointsWorkoutSpeed+1);
        avgWorkoutSpeed = firstEle + secondEle;
    }


    // Update Avg interval
    if (nbPointsIntervalSpeed == 0) {
        avgIntervalSpeed = value;
    }
    else {
        double firstEle = avgIntervalSpeed *((double)nbPointsIntervalSpeed/(nbPointsIntervalSpeed+1));
        double secondEle = ((double)value)/(nbPointsIntervalSpeed+1);
        avgIntervalSpeed = firstEle + secondEle;
    }

    nbPointsIntervalSpeed++;
    nbPointsWorkoutSpeed++;


    emit avgSpeedWorkoutChanged(avgWorkoutSpeed);
    emit avgSpeedIntervalChanged(avgIntervalSpeed);
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DataWorkout::powerDataReceived(int secReceived, double value) {

    //Update Calories
    double calToAdd = value * 0.001;
    calories += calToAdd;
    lapCalories += calToAdd;


    // Update Avg Workout
    if (nbPointsWorkoutPower == 0) {
        avgWorkoutPower = value;
    }
    else {
        double firstEle = avgWorkoutPower *((double)nbPointsWorkoutPower/(nbPointsWorkoutPower+1));
        double secondEle = ((double)value)/(nbPointsWorkoutPower+1);
        avgWorkoutPower = firstEle + secondEle;
    }


    // Update Avg interval
    if (nbPointsIntervalPower == 0) {
        avgIntervalPower = value;
    }
    else {
        double firstEle = avgIntervalPower *((double)nbPointsIntervalPower/(nbPointsIntervalPower+1));
        double secondEle = ((double)value)/(nbPointsIntervalPower+1);
        avgIntervalPower = firstEle + secondEle;
    }

    nbPointsIntervalPower++;
    nbPointsWorkoutPower++;


    emit avgPowerWorkoutChanged(avgWorkoutPower);
    emit avgPowerIntervalChanged(avgIntervalPower);
    emit caloriesWorkoutChanged(calories);


    updateWorkoutMetrics(secReceived, value);
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DataWorkout::updateWorkoutMetrics(int secReceived, double value) {


    ///--------------- Update vecMeanPower30sec for NP, TSS, IF in real-time ----------------------
    //    qDebug() << "UpdateWorkoutMetrics, secReveiced:" << secReceived << "nbPoints30SecPower" << nbPoints30SecPower << "power:" << value;




    /// 2- Put the value of each vecMeanPower30sec[x] ^4
    /// 3 -Average of all values calculated in step2
    /// 4 -Fourth root of step3 value  (exp 1/4)

    /*
    1) starting at the 30 s mark, calculate a rolling 30 s average (of the preceeding time points, obviously).
    2) raise all the values obtained in step #1 to the 4th power.
    3) take the average of all of the values obtained in step #2.
    4) take the 4th root of the value obtained in step #3.*/


    if (nbPoints30SecPower == 0) {
        last30secMean = value;
        nb30secMean++;
    }
    else {
        double firstEle = last30secMean *((double)nbPoints30SecPower/(nbPoints30SecPower+1));
        double secondEle = ((double)value)/(nbPoints30SecPower+1);
        last30secMean = firstEle + secondEle;
        //        qDebug() << "New mean is" << last30secMean;
    }


    double totalSumExp4WithLast30Sec = totalSumExp4 + (last30secMean*last30secMean*last30secMean*last30secMean);
    //    qDebug() << "TotalSumExp4:" << totalSumExp4 << "TotalSumExp4WithLast30sec:" << totalSumExp4WithLast30Sec << "last30secMean:" << last30secMean;
    double avgStep2 = totalSumExp4WithLast30Sec/nb30secMean;
    normalizedPower = qPow(avgStep2, 1/4.);
    //    qDebug() << "NP IS:" << normalizedPower;

    //    Intensity Factor = NP/FTP
    intensityFactor = normalizedPower/ftp;

    // Training Stress Score  = (IF^2) * DurationHrs x 100
    trainingStressScore = (intensityFactor*intensityFactor) * (secReceived/36.0);


    if (nbPoints30SecPower == 29) {
        //        qDebug() << "#######30 sec done, increment index value!";
        nbPoints30SecPower = 0;
        // add last 30sec mean to cumulative totalSumExp4
        totalSumExp4 += last30secMean*last30secMean*last30secMean*last30secMean;
    }
    else {
        nbPoints30SecPower++;
    }


    emit normalizedPowerChanged(normalizedPower);
    emit intensityFactorChanged(intensityFactor);
    emit tssChanged(trainingStressScore);



}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int DataWorkout::getCP() {

    if (meanPowerTest.isEmpty()) {
        return 0;
    }

    int criticalPower = 0;
    double moyenne = meanPowerTest.first();
    qDebug() << "meanPowerTest" << meanPowerTest.first();
    criticalPower = ((int)(moyenne + 0.5));
    return criticalPower;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int DataWorkout::getFTP() {


    if (meanPowerTest.isEmpty()) {
        return 0;
    }

    if (workout.getWorkoutNameEnum() == Workout::FTP_TEST) {
        return getFTP_normal();
    }
    else if(workout.getWorkoutNameEnum() == Workout::FTP8min_TEST) {
        return getFTP_8min();
    }
    else {
        return -1;
    }

}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int DataWorkout::getFTP_normal() {

    double moyenne = meanPowerTest.first() * 0.95;
    qDebug() << "meanPowerTest" << meanPowerTest.first();
    return ((int)(moyenne + 0.5));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int DataWorkout::getFTP_8min() {

    double moyenne = meanPowerTest.first();
    double moyenne2 =  meanPowerTest.last();
    double avg = ((moyenne + moyenne2)/2) * 0.90;

    qDebug() << "meanPowerTest1" << meanPowerTest.first()<< "meanPowerTest1" << meanPowerTest.last();

    qDebug() << "moyenne" << moyenne << "moyenne2:" << moyenne2 << "result:" << avg;
    return ((int)(avg + 0.5));
}



//---------------------------------------------------------------
int DataWorkout::getLTHR() {

    if (meanHrTest.isEmpty()) {
        return 0;
    }
    //dont return for this test yet
    if (workout.getWorkoutNameEnum() ==  Workout::CP5min_TEST || workout.getWorkoutNameEnum() ==  Workout::CP20min_TEST) {
        return 0;
    }

    double moyenne = 0;
    double nbElement = meanHrTest.size();
    for (int i=0; i<nbElement; i++) {
        moyenne += meanHrTest.at(i);
    }
    moyenne = (moyenne/nbElement) * 0.95;

    return ((int)(moyenne + 0.5));
}










