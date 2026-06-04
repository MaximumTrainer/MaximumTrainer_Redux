#ifndef XMLSTREAMWRITERTCX_H
#define XMLSTREAMWRITERTCX_H

#include <QXmlStreamWriter>
#include <QDateTime>
#include <QPointF>
#include <QVector>
#include "intervaldata.h"
#include "dataworkout.h"

class XmlStreamWriterTCX : public QXmlStreamWriter
{

public:
    XmlStreamWriterTCX();



//    void buildWorkoutTCXfile(DataWorkout *data, int lastIntervalDone, QList<double> lstSkippedSec) ;
//    void buildOpenTCXfile(DataWorkout *data, int timeDoneSec);





private :
//    QString generateFileName(DataWorkout *data);

//    void helperBuildTcxWorkout(DataWorkout *data, int lastIntervalDone, QList<double> lstSkippedSec);
//    void helperBuildTcxFreeRide(DataWorkout *data, int timeDoneSec);


    ///Deprecated
//    void helperBuildTcxFullWorkout(DataWorkout *data);
//    void helperBuildTcxPartialWorkout(DataWorkout *data, int timeDoneSec);



    void addStartDocumentTcx(QDateTime dateTimeNow);
    void addLapDocumentTcx(IntervalData *intervalData);


private :

    QDateTime startTimeWorkout;
    QDateTime startTimeTrackPoint;





};

#endif // XMLSTREAMWRITERTCX_H
