#ifndef SENSOR_H
#define SENSOR_H

#include <QtCore>
#include <QApplication>

//`device_type` int(2)
// 120=hr, 11=power, 121=SC, 122=Cadence, 123=Speed, 31=OXYGEN

class Sensor
{
public:

    enum SENSOR_TYPE
    {

        SENSOR_HR,
        SENSOR_CADENCE,
        SENSOR_SPEED_CADENCE,
        SENSOR_SPEED,
        SENSOR_POWER,
        SENSOR_OXYGEN,
    };
    static QString getName(Sensor::SENSOR_TYPE);



    Sensor();
    Sensor(int device_id, Sensor::SENSOR_TYPE sensorType, QString name, QString details);




    Sensor::SENSOR_TYPE getDeviceType() const {
        return this->device_type;
    }
    QString getName() const {
        return this->name;
    }
    QString getDetails() const {
        return this->details;
    }



private :

    int device_id;
    SENSOR_TYPE device_type;

    QString name;
    QString details;


};
Q_DECLARE_METATYPE(Sensor)

#endif // SENSOR_H
