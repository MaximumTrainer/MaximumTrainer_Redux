#include "sensor.h"

#include <QDebug>


Sensor::Sensor() {

    this->device_id = 0;
}


Sensor::Sensor(int device_id, Sensor::SENSOR_TYPE device_type, QString name, QString details) {

    this->device_id = device_id;
    this->device_type = device_type;
    this->name = std::move(name);
    this->details = std::move(details);
}


QString Sensor::getName(Sensor::SENSOR_TYPE s) {

    if (s == SENSOR_HR) {
        return QApplication::translate("SensorEnum", "Heart rate", nullptr);
    }
    else if (s == SENSOR_SPEED ){
        return QApplication::translate("SensorEnum", "Speed", nullptr);
    }
    else if (s == SENSOR_CADENCE ){
        return QApplication::translate("SensorEnum", "Cadence", nullptr);
    }
    else if (s == SENSOR_SPEED_CADENCE ){
        return QApplication::translate("SensorEnum", "Speed & Cadence", nullptr);
    }
    else if (s == SENSOR_POWER ){
        return QApplication::translate("SensorEnum", "Power", nullptr);
    }
    else if (s == SENSOR_OXYGEN ){
        return QApplication::translate("SensorEnum", "Muscle Oxygen", nullptr);
    }
    else {
        return "NOT SUPPORTED_ SensorEnum getName()";
    }
}

