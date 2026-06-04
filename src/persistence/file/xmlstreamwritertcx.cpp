#include "xmlstreamwritertcx.h"
#include <QDebug>
#include <QFile>
#include "util.h"
#include "environnement.h"
#include "account.h"


XmlStreamWriterTCX::XmlStreamWriterTCX()
{
}



//---------------------------------------------------------------------------------------------------------------------------------
//QString XmlStreamWriterTCX::generateFileName(DataWorkout *data) {

//    qDebug() << "generateFileName";
//    Account *account = qApp->property("Account").value<Account*>();

//    QString nameFileTcx = Util::getSystemPathHistory() + "/" +
//            startTimeWorkout.toLocalTime().toString("yyyy-MM-dd(hh-mm-ss)") + "-MT-" + account->email_clean + "-" +
//            data->getWorkout().getName() + ".tcx";
//    qDebug() << "name of tcx file:" << nameFileTcx;


//    return nameFileTcx;
//}



// lstSkipped represent the time skipped in each interval (lstSkippedSec.at(0) = sec skipped in interval 0
//---------------------------------------------------------------------------------------------------------------------------------
//void XmlStreamWriterTCX::buildWorkoutTCXfile(DataWorkout *data, int lastIntervalDone, QList<double> lstSkippedSec) {

//    qDebug() << "buildWorkoutTCXfile";

//    this->setAutoFormatting(true);


//    startTimeWorkout = data->getStartTimeWorkout();
//    startTimeTrackPoint = startTimeWorkout;
//    startTimeWorkout.setTimeSpec(Qt::UTC);
//    startTimeTrackPoint.setTimeSpec(Qt::UTC);


//    QString nameFileTcx = generateFileName(data);
//    QFile fileTcx(nameFileTcx);



//    if (!fileTcx.open(QIODevice::WriteOnly)) {
//        qDebug() << "problem writing to file tcx";
//    }
//    // Write to file
//    else {
//        this->setDevice(&fileTcx);
//        this->addStartDocumentTcx(data->getStartTimeWorkout());

//        helperBuildTcxWorkout(data, lastIntervalDone, lstSkippedSec);
//    }

//    fileTcx.close();
//}


//// OPENRIDE
////------------------------------------------------------------------------------------------------------------------------------------
//void XmlStreamWriterTCX::buildOpenTCXfile(DataWorkout *data, int timeDoneSec) {

//    qDebug() << "buildOpenTCXfile";


//    this->setAutoFormatting(true);


//    startTimeWorkout = data->getStartTimeWorkout();
//    startTimeTrackPoint = startTimeWorkout;
//    startTimeWorkout.setTimeSpec(Qt::UTC);
//    startTimeTrackPoint.setTimeSpec(Qt::UTC);


//    QString nameFileTcx = generateFileName(data);
//    QFile fileTcx(nameFileTcx);


//    if (!fileTcx.open(QIODevice::WriteOnly)) {
//        qDebug() << "problem writing to file tcx";
//    }
//    // Write to file
//    else {
//        this->setDevice(&fileTcx);
//        this->addStartDocumentTcx(data->getStartTimeWorkout());

//        helperBuildTcxFreeRide(data, timeDoneSec);
//    }

//    fileTcx.close();
//}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//void XmlStreamWriterTCX::helperBuildTcxWorkout(DataWorkout *data, int lastIntervalDone, QList<double> lstSkippedSec) {



//    qDebug() << "helperBuildTcxWorkout" << "last interval DONE: " <<lastIntervalDone;



//    /// Set time start each interval
//    double timeDoneSecsTotal = 0;
//    const QDateTime startTimeWorkoutTemp = startTimeWorkout;
//    for (int i=0; i<data->getListIntervalData().size(); i++)  {

//        IntervalData *interval = data->getIntervalDataAt(i);
//        interval->startTimeInterval = startTimeWorkoutTemp.addSecs(timeDoneSecsTotal);
//        timeDoneSecsTotal += interval->timeSecInterval;
//    }



//    ///--- 1- Find the time done in each interval
//    QList<double> lstTimeDone;
//    qDebug() << "Start time workout:" << startTimeWorkout;

//    for (int i=0; i<data->getListIntervalData().size(); i++)  {

//        IntervalData *interval = data->getIntervalDataAt(i);
//        int timeDoneInterval = interval->timeSecInterval - lstSkippedSec.at(i);
//        lstTimeDone.append(timeDoneInterval);
//    }






//    int timeStartIntervalSec = 0;
//    int timeEndIntervalSec = 0;
//    double distanceMeterCumul = 0;  //get incremented each trackpoint
//    //-------------------------ADD LAP-----------------------------------------------
//    for (int i=0; i<=lastIntervalDone; i++)
//    {

//        //do not write interval smaller than 1.5sec
//        if (lstTimeDone.at(i) < 1.5)
//            break;

//        IntervalData *interval = data->getIntervalDataAt(i);
//        QString startTime = interval->startTimeInterval.toString(Qt::ISODate);

//        //        double timeIntervalDouble = lstTimeDone.at(i);
//        double timeIntervalDouble = (double)interval->timeSecInterval;
//        double distanceMeter = (interval->avgSpeed*1000.0/3600.0) * lstTimeDone.at(i);

//        double maxSpeed = interval->maxSpeed*1000.0/3600.0;
//        double calories = interval->calories;
//        int calories_rounded = (int)(calories + 0.5);
//        int avgCadence = (int)(interval->avgCadence + 0.5);


//        writeStartElement("Lap");
//        writeAttribute("StartTime", startTime);
//        writeTextElement("TotalTimeSeconds", QString::number(timeIntervalDouble));
//        writeTextElement("DistanceMeters", QString::number(distanceMeter));
//        writeTextElement("MaximumSpeed", QString::number(maxSpeed));
//        writeTextElement("Calories", QString::number(calories_rounded));
//        writeStartElement("AverageHeartRateBpm");
//        writeTextElement("Value", QString::number(qRound(interval->avgHr)));
//        writeEndElement();  /// AverageHeartRateBpm
//        writeStartElement("MaximumHeartRateBpm");
//        writeTextElement("Value", QString::number(interval->maxHr));
//        writeEndElement();  /// MaximumHeartRateBpm
//        writeTextElement("Intensity", "Active");
//        writeTextElement("Cadence", QString::number(avgCadence));
//        writeTextElement("TriggerMethod", "Manual");
//        writeStartElement("Track");


//        /// ----------- ADD TRACKPOINT ----------------------------
//        timeEndIntervalSec += interval->timeSecInterval;


//        qDebug() << "Should write between" << timeStartIntervalSec << "and" << timeEndIntervalSec-1;
//        int currentSecIn = 0;
//        for (int j=timeStartIntervalSec; j<timeEndIntervalSec; j++)
//        {
//            startTimeTrackPoint = startTimeTrackPoint.addSecs(1);
//            currentSecIn++;

//            //do not write trackpoint for skipped time
//            if (currentSecIn < lstTimeDone.at(i)) {

//                if (data->vecMeanSpeed1sec[j] != -1.0 || data->vecMeanHr1sec[j] != -1.0 || data->vecMeanPower1sec[j] != -1.0 || data->vecMeanCadence1sec[j] != -1.0)
//                {
//                    qDebug() << "ok writing index" << j << "valueSpeed" << data->vecMeanSpeed1sec[j] << "valueHr" << data->vecMeanHr1sec[j] <<
//                                "valuePower" << data->vecMeanPower1sec[j] << "valueCadence" << data->vecMeanCadence1sec[j];

//                    writeStartElement("Trackpoint");
//                    writeTextElement("Time", startTimeTrackPoint.toString(Qt::ISODate));


//                    /// ------ Calculate Distance Meter Cumul ------
//                    if (data->vecMeanSpeed1sec[j] != -1.0)
//                    {
//                        distanceMeterCumul += data->vecMeanSpeed1sec[j]*1000.0/3600.0;
//                    }
//                    writeTextElement("DistanceMeters", QString::number(distanceMeterCumul));


//                    if (data->vecMeanHr1sec[j] != -1.0)
//                    {
//                        int hr_rounded = (int) (data->vecMeanHr1sec[j] + 0.5);
//                        writeStartElement("HeartRateBpm");
//                        writeTextElement("Value", QString::number(hr_rounded));
//                        writeEndElement();  /// HeartRateBpm
//                    }


//                    if (data->vecMeanCadence1sec[j] != -1.0)
//                    {
//                        int cad_rounded = (int) (data->vecMeanCadence1sec[j] + 0.5);
//                        writeTextElement("Cadence", QString::number(cad_rounded));
//                    }


//                    /// Extension
//                    if (data->vecMeanPower1sec[j] != -1.0 || data->vecMeanSpeed1sec[j] != -1.0)
//                    {
//                        writeStartElement("Extensions");
//                        writeStartElement("TPX");
//                        writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");

//                        if ( data->vecMeanSpeed1sec[j] != -1.0) {
//                            double speedMetersperSecond = data->vecMeanSpeed1sec[j]*1000.0/3600.0;
//                            writeTextElement("Speed", QString::number(speedMetersperSecond));
//                        }
//                        if ( data->vecMeanPower1sec[j] != -1.0) {
//                            int power_rounded = (int) (data->vecMeanPower1sec[j] + 0.5);
//                            if (power_rounded > 0)
//                                writeTextElement("Watts", QString::number(power_rounded));
//                        }
//                        writeEndElement();  /// TPX
//                        writeEndElement();  /// Extensions
//                    }
//                    writeEndElement();  /// Trackpoint
//                }
//            }
//            // Skipped Time
//            else {

//                writeStartElement("Trackpoint");
//                writeTextElement("Time", startTimeTrackPoint.toString(Qt::ISODate));

//                writeStartElement("HeartRateBpm");
//                writeTextElement("Value", 0);
//                writeEndElement();  /// HeartRateBpm

//                writeTextElement("Cadence", 0);

//                writeStartElement("Extensions");
//                writeStartElement("TPX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                writeTextElement("Speed", 0);
//                writeTextElement("Watts", 0);

//                writeEndElement();  /// TPX
//                writeEndElement();  /// Extensions
//                writeEndElement();  /// Trackpoint

//            }
//        }
//        timeStartIntervalSec = timeEndIntervalSec;
//        /// ----------- ADD TRACKPOINT ----------------------------

//        writeEndElement();  /// Track


//        /// Lap Extension
//        if (interval->maxCadence > 0 || interval->avgSpeed > 0 || interval->avgPower > 0 || interval->maxPower > 0)
//        {
//            writeStartElement("Extensions");

//            if (interval->maxCadence > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                writeTextElement("MaxBikeCadence", QString::number(interval->maxCadence));
//                writeEndElement();  /// LX
//            }
//            if (interval->avgSpeed > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                double avgSpeedMetersPerSecond = interval->avgSpeed*1000.0/3600.0;
//                writeTextElement("AvgSpeed", QString::number(avgSpeedMetersPerSecond));
//                writeEndElement();  /// LX
//            }
//            if (interval->avgPower > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                int avgPower_rounded = (int) (interval->avgPower + 0.5);
//                writeTextElement("AvgWatts", QString::number(avgPower_rounded));
//                writeEndElement();  /// LX
//            }
//            if (interval->maxPower > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                writeTextElement("MaxWatts", QString::number(interval->maxPower));
//                writeEndElement();  /// LX
//            }
//            writeEndElement();  /// Extensions
//        }

//        writeEndElement();  /// Lap
//    }


//}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//void XmlStreamWriterTCX::helperBuildTcxFullWorkout(DataWorkout *data) {


//    qDebug() << "helperBuildTcxFullWorkout";


//    int timeStartIntervalSec = 0;
//    int timeEndIntervalSec = 0;
//    double distanceMeterCumul = 0;  //get incremented each trackpoint
//    //-------------------------ADD LAP-----------------------------------------------
//    foreach (IntervalData *interval, data->getListIntervalData())
//    {
//        QString startTime = interval->startTimeInterval.toString(Qt::ISODate);
//        double timeIntervalDouble = (double)interval->timeSecInterval;
//        double distanceMeter = (interval->avgSpeed*1000.0/3600.0) * ((double)interval->timeSecInterval);
//        double maxSpeed = interval->maxSpeed*1000.0/3600.0;
//        //            double calories = interval->avgPower * (interval->timeSecInterval/3600.0) * 3.6;
//        double calories = interval->calories;
//        int calories_rounded = (int)(calories + 0.5);
//        int avgCadence = (int)(interval->avgCadence + 0.5);


//        writeStartElement("Lap");
//        writeAttribute("StartTime", startTime);
//        writeTextElement("TotalTimeSeconds", QString::number(timeIntervalDouble));
//        writeTextElement("DistanceMeters", QString::number(distanceMeter));
//        writeTextElement("MaximumSpeed", QString::number(maxSpeed));
//        writeTextElement("Calories", QString::number(calories_rounded));
//        writeStartElement("AverageHeartRateBpm");
//        writeTextElement("Value", QString::number(qRound(interval->avgHr)));
//        writeEndElement();  /// AverageHeartRateBpm
//        writeStartElement("MaximumHeartRateBpm");
//        writeTextElement("Value", QString::number(interval->maxHr));
//        writeEndElement();  /// MaximumHeartRateBpm
//        writeTextElement("Intensity", "Active");
//        writeTextElement("Cadence", QString::number(avgCadence));
//        writeTextElement("TriggerMethod", "Manual");
//        writeStartElement("Track");


//        /// ----------- ADD TRACKPOINT ----------------------------
//        timeEndIntervalSec += interval->timeSecInterval;
//        qDebug() << "Should write between" << timeStartIntervalSec << "and" << timeEndIntervalSec-1;
//        for (int i=timeStartIntervalSec; i<timeEndIntervalSec; i++)
//        {
//            startTimeTrackPoint = startTimeTrackPoint.addSecs(1);

//            if (data->vecMeanSpeed1sec[i] != -1.0 || data->vecMeanHr1sec[i] != -1.0 || data->vecMeanPower1sec[i] != -1.0 || data->vecMeanCadence1sec[i] != -1.0)
//            {
//                //                qDebug() << "ok writing index" << i << "valueSpeed" << data->vecMeanSpeed1sec[i] << "valueHr" << data->vecMeanHr1sec[i] <<
//                //                            "valuePower" << data->vecMeanPower1sec[i] << "valueCadence" << data->vecMeanCadence1sec[i];

//                writeStartElement("Trackpoint");
//                writeTextElement("Time", startTimeTrackPoint.toString(Qt::ISODate));


//                /// ------ Calculate Distance Meter Cumul ------
//                if (data->vecMeanSpeed1sec[i] != -1.0)
//                {
//                    distanceMeterCumul += data->vecMeanSpeed1sec[i]*1000.0/3600.0;
//                }
//                writeTextElement("DistanceMeters", QString::number(distanceMeterCumul));


//                if (data->vecMeanHr1sec[i] != -1.0)
//                {
//                    int hr_rounded = (int) (data->vecMeanHr1sec[i] + 0.5);
//                    writeStartElement("HeartRateBpm");
//                    writeTextElement("Value", QString::number(hr_rounded));
//                    writeEndElement();  /// HeartRateBpm
//                }


//                if (data->vecMeanCadence1sec[i] != -1.0)
//                {
//                    int cad_rounded = (int) (data->vecMeanCadence1sec[i] + 0.5);
//                    writeTextElement("Cadence", QString::number(cad_rounded));
//                }


//                /// Extension
//                if (data->vecMeanPower1sec[i] != -1.0 || data->vecMeanSpeed1sec[i] != -1.0)
//                {
//                    writeStartElement("Extensions");
//                    writeStartElement("TPX");
//                    writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");

//                    if ( data->vecMeanSpeed1sec[i] != -1.0) {
//                        double speedMetersperSecond = data->vecMeanSpeed1sec[i]*1000.0/3600.0;
//                        writeTextElement("Speed", QString::number(speedMetersperSecond));
//                    }
//                    if ( data->vecMeanPower1sec[i] != -1.0) {
//                        int power_rounded = (int) (data->vecMeanPower1sec[i] + 0.5);
//                        writeTextElement("Watts", QString::number(power_rounded));
//                    }
//                    writeEndElement();  /// TPX
//                    writeEndElement();  /// Extensions
//                }



//                writeEndElement();  /// Trackpoint
//            }

//        }
//        timeStartIntervalSec = timeEndIntervalSec;
//        /// ----------- ADD TRACKPOINT ----------------------------


//        writeEndElement();  /// Track


//        /// Lap Extension
//        if (interval->maxCadence > 0 || interval->avgSpeed > 0 || interval->avgPower > 0 || interval->maxPower > 0)
//        {
//            writeStartElement("Extensions");

//            if (interval->maxCadence > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                writeTextElement("MaxBikeCadence", QString::number(interval->maxCadence));
//                writeEndElement();  /// LX
//            }
//            if (interval->avgSpeed > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                double avgSpeedMetersPerSecond = interval->avgSpeed*1000.0/3600.0;
//                writeTextElement("AvgSpeed", QString::number(avgSpeedMetersPerSecond));
//                writeEndElement();  /// LX
//            }
//            if (interval->avgPower > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                int avgPower_rounded = (int) (interval->avgPower + 0.5);
//                writeTextElement("AvgWatts", QString::number(avgPower_rounded));
//                writeEndElement();  /// LX
//            }
//            if (interval->maxPower > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                writeTextElement("MaxWatts", QString::number(interval->maxPower));
//                writeEndElement();  /// LX
//            }
//            writeEndElement();  /// Extensions
//        }
//        writeEndElement();  /// Lap
//    }

//}





/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//void XmlStreamWriterTCX::helperBuildTcxPartialWorkout(DataWorkout *data, int timeDoneSec) {


//    qDebug() << "helperBuildTcxFilePartialWorkout";

//    /// Find the last interval done and time done inside it
//    int timeCounter = 0;
//    int timeCounter2 = 0;
//    int lastIntervalDone = 0;
//    int timeDoneInLastInterval = 0;
//    foreach (IntervalData *interval, data->getListIntervalData())
//    {
//        timeCounter +=  interval->timeSecInterval;

//        if (timeDoneSec <= timeCounter ) {
//            timeDoneInLastInterval = timeDoneSec - timeCounter2;
//            break;
//        }
//        timeCounter2 +=  interval->timeSecInterval;
//        lastIntervalDone++;
//    }
//    //    qDebug() << "****** THE LAST INTERVAL DONE IS THE" << lastIntervalDone;
//    //    qDebug() << "****** WE DID " << timeDoneInLastInterval << " SECONDS IN THE LAST INTERVAL";



//    int timeStartIntervalSec = 0;
//    int timeEndIntervalSec = 0;
//    double distanceMeterCumul = 0;  //get incremented each trackpoint
//    //-------------------------ADD LAP-----------------------------------------------
//    for (int i=0; i<= lastIntervalDone; i++)
//    {

//        IntervalData *interval = data->getIntervalDataAt(i);

//        QString startTime = interval->startTimeInterval.toString(Qt::ISODate);

//        double timeIntervalDouble;
//        double distanceMeter;
//        if (i == lastIntervalDone) {
//            timeIntervalDouble = (double)timeDoneInLastInterval;
//            distanceMeter = (interval->avgSpeed*1000.0/3600.0) * ((double)timeDoneInLastInterval);
//        }
//        else {
//            timeIntervalDouble = (double)interval->timeSecInterval;
//            distanceMeter = (interval->avgSpeed*1000.0/3600.0) * ((double)interval->timeSecInterval);
//        }

//        double maxSpeed = interval->maxSpeed*1000.0/3600.0;
//        double calories = interval->calories;
//        int calories_rounded = (int)(calories + 0.5);
//        int avgCadence = (int)(interval->avgCadence + 0.5);


//        writeStartElement("Lap");
//        writeAttribute("StartTime", startTime);
//        writeTextElement("TotalTimeSeconds", QString::number(timeIntervalDouble));
//        writeTextElement("DistanceMeters", QString::number(distanceMeter));
//        writeTextElement("MaximumSpeed", QString::number(maxSpeed));
//        writeTextElement("Calories", QString::number(calories_rounded));
//        writeStartElement("AverageHeartRateBpm");
//        writeTextElement("Value", QString::number(qRound(interval->avgHr)));
//        writeEndElement();  /// AverageHeartRateBpm
//        writeStartElement("MaximumHeartRateBpm");
//        writeTextElement("Value", QString::number(interval->maxHr));
//        writeEndElement();  /// MaximumHeartRateBpm
//        writeTextElement("Intensity", "Active");
//        writeTextElement("Cadence", QString::number(avgCadence));
//        writeTextElement("TriggerMethod", "Manual");
//        writeStartElement("Track");


//        /// ----------- ADD TRACKPOINT ----------------------------
//        if (i == lastIntervalDone) {
//            timeEndIntervalSec += timeDoneInLastInterval;
//        }
//        else {
//            timeEndIntervalSec += interval->timeSecInterval;
//        }

//        qDebug() << "Should write between" << timeStartIntervalSec << "and" << timeEndIntervalSec-1;
//        for (int i=timeStartIntervalSec; i<timeEndIntervalSec; i++)
//        {
//            startTimeTrackPoint = startTimeTrackPoint.addSecs(1);

//            if (data->vecMeanSpeed1sec[i] != -1.0 || data->vecMeanHr1sec[i] != -1.0 || data->vecMeanPower1sec[i] != -1.0 || data->vecMeanCadence1sec[i] != -1.0)
//            {
//                qDebug() << "ok writing index" << i << "valueSpeed" << data->vecMeanSpeed1sec[i] << "valueHr" << data->vecMeanHr1sec[i] <<
//                            "valuePower" << data->vecMeanPower1sec[i] << "valueCadence" << data->vecMeanCadence1sec[i];

//                writeStartElement("Trackpoint");
//                writeTextElement("Time", startTimeTrackPoint.toString(Qt::ISODate));


//                /// ------ Calculate Distance Meter Cumul ------
//                if (data->vecMeanSpeed1sec[i] != -1.0)
//                {
//                    distanceMeterCumul += data->vecMeanSpeed1sec[i]*1000.0/3600.0;
//                }
//                writeTextElement("DistanceMeters", QString::number(distanceMeterCumul));


//                if (data->vecMeanHr1sec[i] != -1.0)
//                {
//                    int hr_rounded = (int) (data->vecMeanHr1sec[i] + 0.5);
//                    writeStartElement("HeartRateBpm");
//                    writeTextElement("Value", QString::number(hr_rounded));
//                    writeEndElement();  /// HeartRateBpm
//                }


//                if (data->vecMeanCadence1sec[i] != -1.0)
//                {
//                    int cad_rounded = (int) (data->vecMeanCadence1sec[i] + 0.5);
//                    writeTextElement("Cadence", QString::number(cad_rounded));
//                }


//                /// Extension
//                if (data->vecMeanPower1sec[i] != -1.0 || data->vecMeanSpeed1sec[i] != -1.0)
//                {
//                    writeStartElement("Extensions");
//                    writeStartElement("TPX");
//                    writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");

//                    if ( data->vecMeanSpeed1sec[i] != -1.0) {
//                        double speedMetersperSecond = data->vecMeanSpeed1sec[i]*1000.0/3600.0;
//                        writeTextElement("Speed", QString::number(speedMetersperSecond));
//                    }
//                    if ( data->vecMeanPower1sec[i] != -1.0) {
//                        int power_rounded = (int) (data->vecMeanPower1sec[i] + 0.5);
//                        if (power_rounded > 0)
//                            writeTextElement("Watts", QString::number(power_rounded));
//                    }
//                    writeEndElement();  /// TPX
//                    writeEndElement();  /// Extensions
//                }



//                writeEndElement();  /// Trackpoint
//            }

//        }
//        timeStartIntervalSec = timeEndIntervalSec;
//        /// ----------- ADD TRACKPOINT ----------------------------


//        writeEndElement();  /// Track


//        /// Lap Extension
//        if (interval->maxCadence > 0 || interval->avgSpeed > 0 || interval->avgPower > 0 || interval->maxPower > 0)
//        {
//            writeStartElement("Extensions");

//            if (interval->maxCadence > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                writeTextElement("MaxBikeCadence", QString::number(interval->maxCadence));
//                writeEndElement();  /// LX
//            }
//            if (interval->avgSpeed > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                double avgSpeedMetersPerSecond = interval->avgSpeed*1000.0/3600.0;
//                writeTextElement("AvgSpeed", QString::number(avgSpeedMetersPerSecond));
//                writeEndElement();  /// LX
//            }
//            if (interval->avgPower > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                int avgPower_rounded = (int) (interval->avgPower + 0.5);
//                writeTextElement("AvgWatts", QString::number(avgPower_rounded));
//                writeEndElement();  /// LX
//            }
//            if (interval->maxPower > 0)
//            {
//                writeStartElement("LX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//                writeTextElement("MaxWatts", QString::number(interval->maxPower));
//                writeEndElement();  /// LX
//            }
//            writeEndElement();  /// Extensions
//        }

//        writeEndElement();  /// Lap
//    }
//}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//void XmlStreamWriterTCX::helperBuildTcxFreeRide(DataWorkout *data, int timeDoneSec) {


//    qDebug() << "helperBuildTcxFileFreeRide" << "timeDone:" << timeDoneSec;



//    //    startTimeWorkout = data->getStartTimeWorkout();
//    //    startTimeTrackPoint = startTimeWorkout;


//    int timeStartIntervalSec = 0;
//    int timeEndIntervalSec = timeDoneSec;
//    double distanceMeterCumul = 0;  //get incremented each trackpoint

//    //-------------------------ADD 1 LAP-----------------------------------------------
//    QString startTime = startTimeWorkout.toString(Qt::ISODate);
//    double timeIntervalDouble = (double)timeDoneSec;
//    double distanceMeter = (data->getAvgWorkoutSpeed()*1000.0/3600.0) * ((double)timeDoneSec);
//    double maxSpeed = data->getMaxWorkoutSpeed()*1000.0/3600.0;
//    //            double calories = interval->avgPower * (interval->timeSecInterval/3600.0) * 3.6;
//    double calories = data->getWorkoutCalories();
//    int calories_rounded = (int)(calories + 0.5);
//    int avgCadence = (int)(data->getAvgWorkoutCad() + 0.5);


//    writeStartElement("Lap");
//    writeAttribute("StartTime", startTime);
//    writeTextElement("TotalTimeSeconds", QString::number(timeIntervalDouble));
//    writeTextElement("DistanceMeters", QString::number(distanceMeter));
//    writeTextElement("MaximumSpeed", QString::number(maxSpeed));
//    writeTextElement("Calories", QString::number(calories_rounded));
//    writeStartElement("AverageHeartRateBpm");
//    writeTextElement("Value", QString::number(qRound(data->getAvgWorkoutHr())));
//    writeEndElement();  /// AverageHeartRateBpm
//    writeStartElement("MaximumHeartRateBpm");
//    writeTextElement("Value", QString::number(data->getMaxWorkoutHr()));
//    writeEndElement();  /// MaximumHeartRateBpm
//    writeTextElement("Intensity", "Active");
//    writeTextElement("Cadence", QString::number(avgCadence));
//    writeTextElement("TriggerMethod", "Manual");
//    writeStartElement("Track");


//    /// ----------- ADD TRACKPOINT ----------------------------
//    qDebug() << "Should write between" << timeStartIntervalSec << "and" << timeEndIntervalSec-1;
//    for (int i=timeStartIntervalSec; i<timeEndIntervalSec; i++)
//    {
//        startTimeTrackPoint = startTimeTrackPoint.addSecs(1);

//        if (data->vecMeanSpeed1sec[i] != -1.0 || data->vecMeanHr1sec[i] != -1.0 || data->vecMeanPower1sec[i] != -1.0 || data->vecMeanCadence1sec[i] != -1.0)
//        {
//            //            qDebug() << "ok writing index" << i << "valueSpeed" << data->vecMeanSpeed1sec[i] << "valueHr" << data->vecMeanHr1sec[i] <<
//            //                        "valuePower" << data->vecMeanPower1sec[i] << "valueCadence" << data->vecMeanCadence1sec[i];

//            writeStartElement("Trackpoint");
//            writeTextElement("Time", startTimeTrackPoint.toString(Qt::ISODate));


//            /// ------ Calculate Distance Meter Cumul ------
//            if (data->vecMeanSpeed1sec[i] != -1.0)
//            {
//                distanceMeterCumul += data->vecMeanSpeed1sec[i]*1000.0/3600.0;
//            }
//            writeTextElement("DistanceMeters", QString::number(distanceMeterCumul));


//            if (data->vecMeanHr1sec[i] != -1.0)
//            {
//                int hr_rounded = (int) (data->vecMeanHr1sec[i] + 0.5);
//                writeStartElement("HeartRateBpm");
//                writeTextElement("Value", QString::number(hr_rounded));
//                writeEndElement();  /// HeartRateBpm
//            }


//            if (data->vecMeanCadence1sec[i] != -1.0)
//            {
//                int cad_rounded = (int) (data->vecMeanCadence1sec[i] + 0.5);
//                writeTextElement("Cadence", QString::number(cad_rounded));
//            }


//            /// Extension
//            if (data->vecMeanPower1sec[i] != -1.0 || data->vecMeanSpeed1sec[i] != -1.0)
//            {
//                writeStartElement("Extensions");
//                writeStartElement("TPX");
//                writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");

//                if ( data->vecMeanSpeed1sec[i] != -1.0) {
//                    double speedMetersperSecond = data->vecMeanSpeed1sec[i]*1000.0/3600.0;
//                    writeTextElement("Speed", QString::number(speedMetersperSecond));
//                }
//                if ( data->vecMeanPower1sec[i] != -1.0) {
//                    int power_rounded = (int) (data->vecMeanPower1sec[i] + 0.5);
//                    writeTextElement("Watts", QString::number(power_rounded));
//                }
//                writeEndElement();  /// TPX
//                writeEndElement();  /// Extensions
//            }



//            writeEndElement();  /// Trackpoint
//        }

//    }
//    /// ----------- ADD TRACKPOINT ----------------------------


//    writeEndElement();  /// Track


//    /// Lap Extension

//    if (data->getMaxWorkoutCad() > 0 || data->getAvgWorkoutSpeed() > 0 || data->getAvgWorkoutPower() > 0 || data->getMaxWorkoutPower() > 0)
//    {
//        writeStartElement("Extensions");

//        if (data->getMaxWorkoutCad() > 0)
//        {
//            writeStartElement("LX");
//            writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//            writeTextElement("MaxBikeCadence", QString::number(data->getMaxWorkoutCad()));
//            writeEndElement();  /// LX
//        }
//        if (data->getAvgWorkoutSpeed() > 0)
//        {
//            writeStartElement("LX");
//            writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//            double avgSpeedMetersPerSecond = data->getAvgWorkoutSpeed()*1000.0/3600.0;
//            writeTextElement("AvgSpeed", QString::number(avgSpeedMetersPerSecond));
//            writeEndElement();  /// LX
//        }
//        if (data->getAvgWorkoutPower() > 0)
//        {
//            writeStartElement("LX");
//            writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//            int avgPower_rounded = (int) (data->getAvgWorkoutPower() + 0.5);
//            writeTextElement("AvgWatts", QString::number(avgPower_rounded));
//            writeEndElement();  /// LX
//        }
//        if (data->getMaxWorkoutPower() > 0)
//        {
//            writeStartElement("LX");
//            writeAttribute("xmlns", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//            writeTextElement("MaxWatts", QString::number(data->getMaxWorkoutPower()));
//            writeEndElement();  /// LX
//        }
//        writeEndElement();  /// Extensions
//    }
//    writeEndElement();  /// Lap


//}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//void XmlStreamWriterTCX::addStartDocumentTcx(QDateTime dateTimeNow) {

//    /// Header
//    writeStartDocument();
//    /// JUMP LINE FOR READIBLITY
//    writeStartElement("TrainingCenterDatabase");

//    QXmlStreamAttributes attributes;
//    //    attributes.append("xmlns", "http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2");
//    //    attributes.append("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
//    //    attributes.append("xsi:schemaLocation", "http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2 http://www.garmin.com/xmlschemas/TrainingCenterDatabasev2.xsd");

//    attributes.append("xsi:schemaLocation", "http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2 http://www.garmin.com/xmlschemas/TrainingCenterDatabasev2.xsd");
//    attributes.append("xmlns:ns5", "http://www.garmin.com/xmlschemas/ActivityGoals/v1");
//    attributes.append("xmlns:ns3", "http://www.garmin.com/xmlschemas/ActivityExtension/v2");
//    attributes.append("xmlns:ns2", "http://www.garmin.com/xmlschemas/UserProfile/v2");
//    attributes.append("xmlns", "http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2");
//    attributes.append("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
//    attributes.append("xmlns:ns4", "http://www.garmin.com/xmlschemas/ProfileExtension/v1");
//    writeAttributes(attributes);


//    /// Start
//    writeStartElement("Activities");
//    writeStartElement("Activity");
//    writeAttribute("Sport", "Biking");
//    writeTextElement("Id", dateTimeNow.toString(Qt::ISODate));    ///<Id>2007-08-07T02:42:41Z</Id>
//}




