#include "sensor.h"

#include <QDebug>




Sensor::Sensor() {

    this->device_id = 0;
}



Sensor::Sensor(int device_id, Sensor::SENSOR_TYPE device_type, QString name, QString details) {

    this->device_id = device_id;
    this->device_type = device_type;
    this->name = name;
    this->details = details;
}



/////////////////////////////////////////////////////////////////////////////////////////////////
QString Sensor::getName(Sensor::SENSOR_TYPE s) {

    if (s == SENSOR_HR) {
        return QApplication::translate("SensorEnum", "Heart rate", 0);
    }
    else if (s == SENSOR_SPEED ){
        return QApplication::translate("SensorEnum", "Speed", 0);
    }
    else if (s == SENSOR_CADENCE ){
        return QApplication::translate("SensorEnum", "Cadence", 0);
    }
    else if (s == SENSOR_SPEED_CADENCE ){
        return QApplication::translate("SensorEnum", "Speed & Cadence", 0);
    }
    else if (s == SENSOR_POWER ){
        return QApplication::translate("SensorEnum", "Power", 0);
    }
    else if (s == SENSOR_OXYGEN ){
        return QApplication::translate("SensorEnum", "Muscle Oxygen", 0);
    }
    else {
        return "NOT SUPPORTED_ SensorEnum getName()";
    }
}





