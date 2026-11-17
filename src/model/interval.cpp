#include "interval.h"
#include <QDebug>
#include <QApplication>
#include "util.h"


QDataStream& operator<<(QDataStream& stream, const Interval& obj) {


    stream << obj.sourceRowLstInterval;
    stream << obj.duration;
    stream << obj.displayMessage;

    stream << ((int)obj.powerStepType);
    stream << obj.targetFTP_start;
    stream << obj.targetFTP_end;
    stream << obj.targetFTP_range;
    stream << obj.rightPowerTarget;


    stream << ((int)obj.cadenceStepType);
    stream << obj.targetCadence_start;
    stream << obj.targetCadence_end;
    stream << obj.targetCadence_range;

    stream << ((int)obj.hrStepType);
    stream << obj.targetHR_start;
    stream << obj.targetHR_end;
    stream << obj.targetHR_range;

    stream << obj.testInterval;
    stream << obj.repeatIncreaseFTP;
    stream << obj.repeatIncreaseCadence;
    stream << obj.repeatIncreaseLTHR;


    return stream;
}


QDataStream& operator>>(QDataStream& stream, Interval& obj) {


    stream >> obj.sourceRowLstInterval;
    stream >> obj.duration;
    stream >> obj.displayMessage;

    int powerStepTypeInt;
    stream >> powerStepTypeInt;
    obj.powerStepType = (Interval::StepType)powerStepTypeInt;

    stream >> obj.targetFTP_start;
    stream >> obj.targetFTP_end;
    stream >> obj.targetFTP_range;
    stream >> obj.rightPowerTarget;

    int cadenceStepTypeInt;
    stream >> cadenceStepTypeInt;
    obj.cadenceStepType = (Interval::StepType)cadenceStepTypeInt;

    stream >> obj.targetCadence_start;
    stream >> obj.targetCadence_end;
    stream >> obj.targetCadence_range;


    int hrStepTypeInt;
    stream >> hrStepTypeInt;
    obj.hrStepType = (Interval::StepType)hrStepTypeInt;

    stream >> obj.targetHR_start;
    stream >> obj.targetHR_end;
    stream >> obj.targetHR_range;

    stream >> obj.testInterval;
    stream >> obj.repeatIncreaseFTP;
    stream >> obj.repeatIncreaseCadence;
    stream >> obj.repeatIncreaseLTHR;


    return stream;
}


Interval::Interval() {


    this->duration =  QTime(0,0,5);
    this->displayMessage = "";
    this->testInterval = false;
    this->repeatIncreaseFTP = 0;
    this->repeatIncreaseCadence = 0;
    this->repeatIncreaseLTHR = 0;

    this->powerStepType = Interval::StepType::NONE;
    this->targetFTP_start = 0;
    this->targetFTP_end = 0;
    this->targetFTP_range = 20;
    this->rightPowerTarget = -1;

    this->cadenceStepType = Interval::StepType::NONE;
    this->targetCadence_start = 0;
    this->targetCadence_end = 0;
    this->targetCadence_range = 5;

    this->hrStepType = Interval::StepType::NONE;
    this->targetHR_start = 0;
    this->targetHR_end = 0;
    this->targetHR_range = 20;

}

Interval::Interval(QTime duration, QString displayMessage,
                   StepType powerStepType, double targetFTP_start, double targetFTP_end, int FTP_range, double rightPowerTarget,
                   StepType cadenceStepType, int targetCadence_start, int targetCadence_end, int cadence_range,
                   StepType hrStepType, double targetHR_start, double targetHR_end, int HR_range,
                   bool testInterval, double repeatIncreaseFTP, int repeatIncreaseCadence, double repeatIncreaseLTHR) {


    this->duration = duration;
    this->displayMessage = std::move(displayMessage);

    /// POWER
    this->powerStepType = powerStepType;
    this->targetFTP_start = targetFTP_start;
    this->targetFTP_end = targetFTP_end;
    this->targetFTP_range = FTP_range;
    this->rightPowerTarget = rightPowerTarget;


    /// CADENCE
    this->cadenceStepType = cadenceStepType;
    this->targetCadence_start = targetCadence_start;
    this->targetCadence_end = targetCadence_end;
    this->targetCadence_range = cadence_range;

    /// HR
    this->hrStepType = hrStepType;
    this->targetHR_start = targetHR_start;
    this->targetHR_end = targetHR_end;
    this->targetHR_range = HR_range;

    this->testInterval = testInterval;
    this->repeatIncreaseFTP = repeatIncreaseFTP;
    this->repeatIncreaseCadence = repeatIncreaseCadence;
    this->repeatIncreaseLTHR = repeatIncreaseLTHR;

}


QTime Interval::getDurationQTime() const {
    return this->duration;
}

QString Interval::getDisplayMessage() const {
    return this->displayMessage;
}


QString Interval::getStepTypeToString(StepType type) {
    if (type == Interval::NONE) {
        return QApplication::translate("Interval", "None", nullptr);
    }
    else if (type == Interval::PROGRESSIVE) {
        return QApplication::translate("Interval", "Progressive", nullptr);
    }
    else if (type == Interval::FLAT) {
        return QApplication::translate("Interval", "Flat", nullptr);
    }
    else {
        return QApplication::translate("Interval", "Interval type not defined", nullptr);
    }
}


/// POWER
Interval::StepType Interval::getPowerStepType() const {
    return this->powerStepType;
}
int Interval::getFTP_range() const {
    //    if (this->powerStepType == StepType::NONE) {
    //        return 75;
    //    }
    return this->targetFTP_range;
}
double Interval::getFTP_start() const {
    //    if (this->powerStepType == StepType::NONE) {
    //        return -0.8;
    //    }
    return this->targetFTP_start;
}
double Interval::getFTP_end() const {
    if (this->powerStepType == StepType::FLAT) {
        return this->targetFTP_start;
    }
    return this->targetFTP_end;

}
double Interval::getRightPowerTarget() const {
    return this->rightPowerTarget;
}


/// CADENCE
Interval::StepType Interval::getCadenceStepType() const {
    return this->cadenceStepType;
}
int Interval::getCadence_range() const {
    return this->targetCadence_range;
}
int Interval::getCadence_start() const {
    return this->targetCadence_start;
}
int Interval::getCadence_end() const {
    if (this->cadenceStepType == StepType::FLAT) {
        return this->targetCadence_start;
    }
    return this->targetCadence_end;
}
/// HR
Interval::StepType Interval::getHRStepType() const {
    return this->hrStepType;
}
int Interval::getHR_range() const {
    //    if (this->hrStepType == StepType::NONE) {
    //        return 75;
    //    }
    return this->targetHR_range;
}
double Interval::getHR_start() const {
    //    if (this->hrStepType == StepType::NONE) {
    //        return -0.5;
    //    }
    return this->targetHR_start;
}
double Interval::getHR_end() const {
    if (this->hrStepType == StepType::FLAT) {
        return this->targetHR_start;
    }
    return this->targetHR_end;
}

bool Interval::isTestInterval() const {
    return this->testInterval;
}
double Interval::getRepeatIncreaseFTP() const {
    return this->repeatIncreaseFTP;
}
int Interval::getRepeatIncreaseCadence() const {
    return this->repeatIncreaseCadence;
}
double Interval::getRepeatIncreaseLTHR() const {
    return this->repeatIncreaseLTHR;
}

