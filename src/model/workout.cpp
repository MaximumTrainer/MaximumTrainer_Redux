#include "workout.h"

#include <QApplication>
#include <QtCore/qmath.h>

#include "util.h"


#include "account.h"


Workout::Workout() {

    this->filePath = "";
    this->workout_name_enum = Workout::WORKOUT_NAME::USER_MADE;
    this->name = "";
    this->createdBy = "";
    this->description = "";
    this->plan = "-";
    this->type = Type::T_ENDURANCE;
    this->maxPowerPourcent = 0.0;
    this->durationQTime = QTime(0,0,0);

}


Workout::Workout(QString filePath, WORKOUT_NAME workout_name_enum, QList<Interval> lstIntervalSource, QList<RepeatData> lstRepeatData,
                 QString name, QString createdBy, QString description, QString plan, Type type) {

    this->filePath = std::move(filePath);
    this->workout_name_enum = workout_name_enum;
    this->lstIntervalSource = std::move(lstIntervalSource);
    this->lstRepeatData = std::move(lstRepeatData);
    this->name = std::move(name);
    this->createdBy = std::move(createdBy);
    this->description = std::move(description);
    this->plan = std::move(plan);
    this->type = type;
    this->maxPowerPourcent = 0.0;
    this->durationQTime = QTime(0,0,0);


    computeWorkout();

    initializeArrayFTP();
    calculateWorkoutMetrics();

}


Workout::Workout(QString filePath, WORKOUT_NAME workout_name_enum, QList<Interval> lstInterval,
                 QString name, QString createdBy, QString description, QString plan, Type type) {

    this->filePath = std::move(filePath);
    this->workout_name_enum = workout_name_enum;
    this->lstInterval = lstInterval;
    this->lstIntervalSource = std::move(lstInterval);
    this->name = std::move(name);
    this->createdBy = std::move(createdBy);
    this->description = std::move(description);
    this->plan = std::move(plan);
    this->type = type;
    this->maxPowerPourcent = 0.0;
    this->durationQTime = QTime(0,0,0);


    computeWorkoutTotalTime();


    initializeArrayFTP();
    calculateWorkoutMetrics();
}


void Workout::computeWorkout() {


    /// Check repeat interval and construct them
    if (lstRepeatData.size() > 0)
    {
        for (int j=0; j<lstRepeatData.size(); j++)
        {
            RepeatData rep = lstRepeatData.at(j);
            int numberRepeat = rep.getNumberRepeat();
            int firstRow = rep.getFirstRow();
            int lastRow = rep.getLastRow();


            /// Copy row before first repeat
            if (j==0 && rep.getFirstRow() != 0) {
                for (int a=0; a<rep.getFirstRow(); a++)
                {
                    Interval interval = lstIntervalSource.at(a);
                    //                    lstIntervalSource.getPosition
                    interval.setSourceRowLstInterval(a);
                    lstInterval.append(interval);
                }
            }

            /// ----------------------- Copy row inside repeat ----------------------------

            for (int k=1; k<=numberRepeat; k++) {
                for (int l=firstRow; l<=lastRow; l++)
                {
                    Interval interval = lstIntervalSource.at(l);
                    interval.setSourceRowLstInterval(l);
                    if (interval.getDisplayMessage().size() > 1)
                        interval.setDisplayMsg(interval.getDisplayMessage() + " ("+ QString::number(k) + "/" + QString::number(numberRepeat) + ")");
                    //Repeat increase FTP
                    if (k>1 && interval.getRepeatIncreaseFTP() != 0 && interval.getPowerStepType() != Interval::StepType::NONE) {

                        double totalIncrease = (k-1) *  interval.getRepeatIncreaseFTP();
                        double newTargetStart = interval.getFTP_start() + (totalIncrease/100);
                        double newTargetEnd= interval.getFTP_end() + (totalIncrease/100);
                        if (newTargetStart<0) newTargetStart=0;
                        if (newTargetEnd<0) newTargetEnd=0;
                        interval.setTargetFTP_start(newTargetStart);
                        interval.setTargetFTP_end(newTargetEnd);
                    }
                    //Repeat increase Cadence
                    if (k>1 && interval.getRepeatIncreaseCadence() != 0 && interval.getCadenceStepType() != Interval::StepType::NONE) {

                        double totalIncrease = (k-1) *  interval.getRepeatIncreaseCadence();
                        double newTargetStart = interval.getCadence_start() + totalIncrease;
                        double newTargetEnd= interval.getCadence_end() + totalIncrease;
                        if (newTargetStart<0) newTargetStart=0;
                        if (newTargetEnd<0) newTargetEnd=0;
                        interval.setTargetCadence_start(newTargetStart);
                        interval.setTargetCadence_end(newTargetEnd);
                    }
                    //Repeat increase LTHR
                    if (k>1 && interval.getRepeatIncreaseLTHR() != 0 && interval.getHRStepType() != Interval::StepType::NONE) {

                        double totalIncrease = (k-1) *  interval.getRepeatIncreaseLTHR();
                        double newTargetStart = interval.getHR_start() + (totalIncrease/100);
                        double newTargetEnd= interval.getHR_end() + (totalIncrease/100);
                        if (newTargetStart<0) newTargetStart=0;
                        if (newTargetEnd<0) newTargetEnd=0;
                        interval.setTargetHR_start(newTargetStart);
                        interval.setTargetHR_end(newTargetEnd);
                    }

                    lstInterval.append(interval);
                }
            }

            /// Check between repeat to see if intervals without repeats to copy
            if (j!= lstRepeatData.size()-1) {
                RepeatData nextRepeat = lstRepeatData.at(j+1);
                if (rep.getLastRow() +1 != nextRepeat.getFirstRow()) {
                    for (int p=rep.getLastRow()+1; p<nextRepeat.getFirstRow(); p++)
                    {
                        Interval interval = lstIntervalSource.at(p);
                        interval.setSourceRowLstInterval(p);
                        lstInterval.append(interval);
                    }
                }
            }

            /// Copy row after last repeat
            else {
                if (rep.getLastRow() != lstIntervalSource.size()-1) {
                    for (int e=rep.getLastRow()+1; e<lstIntervalSource.size(); e++)
                    {
                        Interval interval = lstIntervalSource.at(e);
                        interval.setSourceRowLstInterval(e);
                        lstInterval.append(interval);
                    }
                }
            }
        }
    }
    else {
        lstInterval = lstIntervalSource;
    }


    computeWorkoutTotalTime();

}

void Workout::computeWorkoutTotalTime() {

    QTime time(0,0,0,0);
    double msecTotal = 0;

    /// Compute stats
    for (const Interval &val : std::as_const(this->lstInterval))
    {
        msecTotal += time.msecsTo(val.getDurationQTime());

        if (val.getFTP_start() > this->maxPowerPourcent) {
            maxPowerPourcent = val.getFTP_start();
        }
        if (val.getFTP_end() > this->maxPowerPourcent) {
            maxPowerPourcent = val.getFTP_end();
        }
    }
    durationQTime = durationQTime.addMSecs(msecTotal);

}


void Workout::initializeArrayFTP() {


    int totalSec  = Util::convertQTimeToSecD(this->durationQTime);

    /// 1 - Create array for expected FTP at every second -------------------------
    const int sizeArray = totalSec + 30; //30 safety
    const double initValueArrayFTP = -1.0;
    this->vecFtp1sec = QVector<double>(sizeArray, initValueArrayFTP);


    /// Calculate %FTP value for every second of the workout
    int secNow = 0;
    for (const Interval &interval : std::as_const(lstInterval))
    {
        int durationSecInterval =  Util::convertQTimeToSecD(interval.getDurationQTime());

        if (interval.getPowerStepType() == Interval::FLAT) {
            for (int i=0; i<=durationSecInterval; i++) {
                vecFtp1sec[secNow+i] = interval.getFTP_start();
            }
        }
        else if (interval.getPowerStepType() == Interval::PROGRESSIVE)  {
            // y=ax+b
            double b = interval.getFTP_start();
            double a = (interval.getFTP_end() - b) / durationSecInterval;
            for (int i=0; i<=durationSecInterval; i++) {
                vecFtp1sec[secNow+i] = a*i + b;
            }
        }
        else {  //NONE
            for (int i=0; i<=durationSecInterval; i++) {
                vecFtp1sec[secNow+i] = 0.55;
            }
        }
        secNow += durationSecInterval;
    }


    /// 2 - Create array for average %FTP on 30secs -----------------------------
    const int sizeArray2 = qRound(totalSec/30.0);
    const double initValueArray30secFTP = 0.0;
    this->vecFtp30sec = QVector<double>(sizeArray2, initValueArray30secFTP);


    int counter30sec = 0;
    for (int i=0; i<vecFtp30sec.size(); i++)
    {
        for (int j=0; j<30; j++)
        {
            int secNow = i*30 + j;
            if (vecFtp1sec[secNow] != -1.0) {
                vecFtp30sec[i] += vecFtp1sec[secNow];
                counter30sec++;
            }
        }
        if (counter30sec > 0)
            vecFtp30sec[i] = vecFtp30sec[i] /counter30sec;      // Avg on 30sec
        counter30sec = 0;
    }


    //    for (int i=0; i<vecFtp1sec.size(); i++)
    //    {
    //    }


    //    for (int i=0; i<vecFtp30sec.size(); i++)
    //    {
    //    }


}


void Workout::calculateWorkoutMetrics() {


    Account *account = qApp->property("Account").value<Account*>();
    int ftp = account->FTP;


    /// Average Power -----------------------------
    double avgFTP = 0.0;
    double counter = 0;
    for (int i=0; i<vecFtp1sec.size(); i++) {
        if (vecFtp1sec[i] != -1.0) {
            avgFTP += vecFtp1sec[i];
            counter++;
        }
    }
    this->averagePower = (counter > 0) ? avgFTP/counter * ftp : 0.0;
    ///--------------------------------------------


    /// Normalized Power ------------------------------
    const double initValueArrayFTP = -1.0;
    QVector<double> vecAvgPowerEvery30sec = QVector<double>(vecFtp30sec.size(), initValueArrayFTP);
    double avgStep = 0.0;

    /// Workout is not always divisible by 30sec, could give the last workout values more importance than it should be
    for (int i=0; i<vecFtp30sec.size(); i++)
    {
        vecAvgPowerEvery30sec[i] = vecFtp30sec[i] * ftp;
        vecAvgPowerEvery30sec[i] = vecAvgPowerEvery30sec[i]*vecAvgPowerEvery30sec[i]*vecAvgPowerEvery30sec[i]*vecAvgPowerEvery30sec[i]; //^4
        avgStep += vecAvgPowerEvery30sec[i];
    }
    if (!vecAvgPowerEvery30sec.isEmpty())
        avgStep = avgStep/vecAvgPowerEvery30sec.size();


    this->normalizedPower = qPow(avgStep, 1/4.);
    ///--------------------------------------------


    ///    Intensity Factor = NP/FTP
    this->intensityFactor = (ftp > 0) ? normalizedPower/ftp : 0.0;

    /// Training Stress Score  = (IF^2) * DurationHrs x 100
    this->trainingStressScore = (intensityFactor*intensityFactor) * (Util::convertQTimeToSecD(this->durationQTime)/3600) * 100;


}


QString Workout::getName() const {
    return this->name;
}
Workout::WORKOUT_NAME Workout::getWorkoutNameEnum() const {
    return this->workout_name_enum;
}
QString Workout::getFilePath() const {
    return this->filePath;
}

QString Workout::getCreatedBy() const {
    return this->createdBy;
}
QString Workout::getDescription() const {
    return this->description;
}
QString Workout::getPlan() const {
    return this->plan;
}
Workout::Type Workout::getType() const {
    return this->type;
}
double Workout::getMaxPowerPourcent() const {
    return this->maxPowerPourcent;
}


QString Workout::getMaxPowerPourcentQString() const {
    return QString::number(this->maxPowerPourcent*100);
}
int Workout::getNbInterval() const {
    return this->lstInterval.size();
}
const QList<Interval> &Workout::getLstInterval() const {
    return this->lstInterval;
}

QTime Workout::getDurationQTime() const {
    return this->durationQTime;
}
double Workout::getAveragePower() const {
    return this->averagePower;
}
double Workout::getNormalizedPower() const {
    return this->normalizedPower;
}
double Workout::getIntensityFactor() const {
    return this->intensityFactor;
}
double Workout::getTrainingStressScore() const {
    return this->trainingStressScore;
}

Interval Workout::getInterval(int nb) const {
    return this->lstInterval.at(nb);
}

QList<Interval> Workout::getLstIntervalSource() const {
    return this->lstIntervalSource;
}

QList<RepeatData> Workout::getLstRepeat() const {
    return this->lstRepeatData;
}


QString Workout::getTypeToString() const {

    if (type == Workout::T_ENDURANCE ) {
        return QApplication::translate("Workout", "Endurance", nullptr);
    }
    else if (type == Workout::T_INTERVAL) {
        return QApplication::translate("Workout", "Interval", nullptr);
    }
    else if (type == Workout::T_TEMPO) {
        return QApplication::translate("Workout", "Tempo", nullptr);
    }
    else if (type == Workout::T_TEST) {
        return QApplication::translate("Workout", "Test", nullptr);
    }
    else if (type == Workout::T_OTHERS) {
        return QApplication::translate("Workout", "Other", nullptr);
    }
    else if (type == Workout::T_THRESHOLD) {
        return QApplication::translate("Workout", "Threshold", nullptr);
    }
    else {
        return QApplication::translate("Workout", "Workout type not defined", nullptr);
    }

}

