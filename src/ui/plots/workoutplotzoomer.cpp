#include "workoutplotzoomer.h"


#include <QFile>
#include <QPalette>
#include <QVBoxLayout>
#include <QPixmap>

#include "qwt_date_scale_draw.h"
#include "qwt_symbol.h"
#include "qwt_plot_shapeitem.h"
#include "qwt_scale_widget.h"
#include "qwt_plot_layout.h"
#include "qwt_plot_textlabel.h"
#include "qwt_text.h"


#include "util.h"
#include "markeritem.h"
#include "iconitem.h"


#include "myqwtpickermachine.h"






////////////////////////////////////////////////////////////////////////////////
/// TimeScaleDraw
////////////////////////////////////////////////////////////////////////////////
class TimeScaleDraw: public QwtScaleDraw {
public:
    TimeScaleDraw( const QTime &base ):
        baseTime( base )
    {
    }

    virtual QwtText label( double v ) const {
        QTime upTime = baseTime.addSecs( static_cast<int>( v ) );
        return upTime.toString();
    }

private:
    QTime baseTime;
};



//////////////////////////////////////////////////////////////////////////////////////////
WorkoutPlotZoomer::~WorkoutPlotZoomer() {

    qDeleteAll(lstTargetShape);
    lstTargetShape.clear();
}

WorkoutPlotZoomer::WorkoutPlotZoomer(QWidget *parent) :
    QwtPlot(parent), d_interval( 0.0, 30.0 ) {

    isStopped = true;
    noTarget = false;

    ///-25 negative to hide extra color on the right caused by axis +25
    /// remove this, issue color not coming back
//    setContentsMargins(0,0,-25,0);
//    canvas()->setContentsMargins(0,0,25,0);



    d_picker = new MyQwtPlotPicker( QwtPlot::xBottom, QwtPlot::yLeft,
                                    QwtPlotPicker::CrossRubberBand, QwtPicker::AlwaysOn,
                                    this->canvas());

    QPen pen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    d_picker->setTrackerPen(pen);




}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::clearPlot() {

    //Delete old target
    for (int i=0; i<lstTargetShape.size(); i++) {
        lstTargetShape.at(i)->detach();
    }
    for (int i=0; i<lstPlotMarket.size(); i++) {
        lstPlotMarket.at(i)->detach();
    }
    zone2->detach();

}


//////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::setUserData(double FTP, double LTHR) {
    this->FTP = FTP;
    this->LTHR = LTHR;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::setWorkoutData(Workout workout, GRAPH_TYPE graph, bool firstInit) {

    this->workout = workout;
    this->type = graph;


    if (!firstInit) {
        clearPlot();
    }
    //firstInit
    else {

        target = -1;
        targetRange = 10;


        ///Init graph default generic left axis
        if (type == GRAPH_TYPE::POWER) {
            leftLow = 100;
            leftHigh = 140;
        }
        else if (type == GRAPH_TYPE::HEART_RATE) {
            leftLow = 100;
            leftHigh = 130;
        }
        else { //Cadence
            leftLow = 80;
            leftHigh = 100;

        }
        setAxisScale( QwtPlot::yLeft, leftLow, leftHigh, 10);
    }



    init(graph, firstInit);
    replot();
}


void WorkoutPlotZoomer::setStopped(bool b) {
    isStopped = b;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// init
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::init(GRAPH_TYPE graph, bool firstInit) {


   // QPalette palette;
   // palette.setColor(QPalette::WindowText, Qt::blue);


    if (firstInit) {

        setAutoReplot( false );

        QTime timeStartWorkout(0,0,0,0);
        setAxisScaleDraw( QwtPlot::xBottom, new TimeScaleDraw( timeStartWorkout ) );
        setAxisScale( QwtPlot::xBottom, d_interval.minValue(), d_interval.maxValue(), 10 );

        canvas()->setStyleSheet(" QwtPlotCanvas { background-color: rgb(35, 35, 35); }");
        canvas()->setCursor(Qt::CrossCursor);

        QFont fontBig;
        fontBig.setPointSize(24);

        valueQwtText = QwtText("0");
        valueQwtText.setBorderRadius(3.0);
        valueQwtText.setFont(fontBig);
        valueQwtText.setRenderFlags( Qt::AlignHCenter | Qt::AlignTop );
        //    valueQwtText.setBackgroundBrush(QBrush(Util::getColor(Util::ON_TARGET), Qt::SolidPattern));
        valueQwtText.setColor(Qt::white);
        qwtTextLabel = new QwtPlotTextLabel();
        qwtTextLabel->setText( valueQwtText );
        qwtTextLabel->attach( this );


        /// middle vertical line
        markerLineMiddle = new MarkerItem();
        markerLineMiddle->setZ(13);
        markerLineMiddle->attach(this);


        QColor colorCurve;
        icon = new IconItem();

        if (graph == GRAPH_TYPE::POWER) {
            extraAfterRange = 15;
            colorCurve = Util::getColor(Util::LINE_POWER);
//            setAxisTitle( QwtPlot::yLeft, tr("watts") );
            icon->iconType = "POWER";
        }
        else if (graph == GRAPH_TYPE::CADENCE) {
            extraAfterRange = 5;
            colorCurve = Util::getColor(Util::LINE_CADENCE);
//            setAxisTitle( QwtPlot::yLeft, tr("rpm") );
            icon->iconType = "CADENCE";

        }
        else if (graph == GRAPH_TYPE::HEART_RATE) {
            extraAfterRange = 10;
            colorCurve = Util::getColor(Util::LINE_HEARTRATE);
//            setAxisTitle( QwtPlot::yLeft, tr("bpm") );
            icon->iconType = "HEART_RATE";
        }

        icon->attach(this);
        icon->setZ(12);


        zone1 = new ZoneItem( "Zone Start");
        zone1->setColor( Qt::black );
        zone1->setInterval( -20, 0 );
        zone1->setVisible( true );
        zone1->attach( this );

        curve = new QwtPlotCurve();
        curve->setStyle( QwtPlotCurve::Lines );
        curve->setPaintAttribute( QwtPlotCurve::ClipPolygons, false );
        curve->setZ(12);
        curve->setPen( QPen( colorCurve, 2.5 ) );
        curve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
        curve->setPaintAttribute(QwtPlotCurve::FilterPoints, true);
        curve->attach(this); // attach once; updateCurve() only refreshes samples
    }




    if (graph == GRAPH_TYPE::POWER && workout.getWorkoutNameEnum() != Workout::OPEN_RIDE) {
        drawGraphIntervalsPower();
    }
    else if (graph == GRAPH_TYPE::CADENCE && workout.getWorkoutNameEnum() != Workout::OPEN_RIDE) {
        drawGraphIntervalsCadence();
    }
    else if (graph == GRAPH_TYPE::HEART_RATE && workout.getWorkoutNameEnum() != Workout::OPEN_RIDE) {
        drawGraphIntervalsHeartrate();
    }


    double secWorkout = Util::convertQTimeToSecD(workout.getDurationQTime());
    if (workout.getWorkoutNameEnum() != Workout::OPEN_RIDE) {
        zone2 = new ZoneItem( "Zone End");
        zone2->setColor( Qt::black );
        zone2->setInterval( secWorkout, secWorkout+20 );
        zone2->setVisible( true );
        zone2->attach( this );
    }









}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::drawGraphIntervalsCadence() {


    QColor colorCadence = Util::getColor(Util::SQUARE_CADENCE);
    QColor colorTarget = Qt::white;


    QList<QPointF> lstPointsCadence;
    QPointF btnLeftC;
    QPointF topLeftC;
    QPointF topRightC;
    QPointF btnRightC;


    QList<QPointF> lstPointsCadenceTarget;
    QPointF btnLeft;
    QPointF topLeft;
    QPointF topRight;
    QPointF btnRight;


    double time = 0;
    int i=0;
    Interval lastVal;

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    foreach (Interval val, workout.getLstInterval()) {


        double secInterval = Util::convertQTimeToSecD(val.getDurationQTime());

        lstPointsCadence.clear();
        lstPointsCadenceTarget.clear();
        if (val.getCadenceStepType() != Interval::NONE) {
            /// CADENCE PARALLELOGRAM --------------------------------------------------------
            btnLeftC = QPointF(time, val.getCadence_start() - val.getCadence_range() );
            topLeftC = QPointF(time, val.getCadence_start() + val.getCadence_range() );

            btnRightC = QPointF(time + secInterval, val.getCadence_end() - val.getCadence_range() );
            topRightC = QPointF(time + secInterval, val.getCadence_end() + val.getCadence_range() );

            lstPointsCadence.append(btnLeftC);
            lstPointsCadence.append(topLeftC);
            lstPointsCadence.append(topRightC);
            lstPointsCadence.append(btnRightC);
            if (val.getCadenceStepType() == Interval::PROGRESSIVE)
                addShapeFromPoints("PARALLELOGRAM_CADENCE", colorCadence, lstPointsCadence, 10, true);
            else
                addShapeFromPoints("PARALLELOGRAM_CADENCE", colorCadence, lstPointsCadence, 10, false);




            /// CADENCE TARGET LINE
            btnLeft = QPointF(time, val.getCadence_start() - 0.1 );
            topLeft = QPointF(time, val.getCadence_start() + 0.1 );

            btnRight = QPointF(time + secInterval, val.getCadence_end() - 0.1 );
            topRight = QPointF(time + secInterval, val.getCadence_end() + 0.1 );

            lstPointsCadenceTarget.append(btnLeft);
            lstPointsCadenceTarget.append(topLeft);
            lstPointsCadenceTarget.append(topRight);
            lstPointsCadenceTarget.append(btnRight);
            if (val.getCadenceStepType() == Interval::PROGRESSIVE)
                addShapeFromPoints("PARALLELOGRAM_CADENCE", colorTarget, lstPointsCadenceTarget, 11, true);
            else
                addShapeFromPoints("PARALLELOGRAM_CADENCE", colorTarget, lstPointsCadenceTarget, 11, false);

        }


        /// MARKER SEPARATE INTERVALS
        if (i!=0 && i < workout.getNbInterval() ) {
            QwtPlotMarker *d_marker1 = new QwtPlotMarker();
            d_marker1->setValue( time, 0.0 );
            d_marker1->setLineStyle( QwtPlotMarker::VLine );
            d_marker1->setLabelAlignment( Qt::AlignRight | Qt::AlignBottom );
            d_marker1->setLinePen( Qt::lightGray, 0, Qt::DashLine );
            d_marker1->setZ(3);
            d_marker1->attach( this );
            lstPlotMarket.append(d_marker1);
        }
        time += secInterval;
        i++;
        lastVal = val;
    }
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::drawGraphIntervalsPower() {


    QColor colorPower = Util::getColor(Util::SQUARE_POWER);
    QColor colorTarget = Qt::white;


    QList<QPointF> lstPointsPower;
    QPointF btnLeft;
    QPointF topLeft;
    QPointF topRight;
    QPointF btnRight;

    QList<QPointF> lstPointsPowerTarget;
    QPointF btnLeftT;
    QPointF topLeftT;
    QPointF topRightT;
    QPointF btnRightT;


    double time = 0.0;
    int i=0;
    Interval lastVal;


    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    foreach (Interval val, workout.getLstInterval()) {


        double secInterval = Util::convertQTimeToSecD(val.getDurationQTime());
        //        qDebug() << "interval QTime IS" << val.getDurationQTime() << " secInterval:" << secInterval;


        lstPointsPower.clear();
        lstPointsPowerTarget.clear();
        if (val.getPowerStepType() != Interval::NONE) {
            /// POWER PARALLELOGRAM --------------------------------------------------------
            btnLeft = QPointF(time, val.getFTP_start()*FTP - val.getFTP_range() );
            topLeft = QPointF(time, val.getFTP_start()*FTP + val.getFTP_range() );

            btnRight = QPointF(time + secInterval, val.getFTP_end()*FTP - val.getFTP_range() );
            topRight = QPointF(time + secInterval, val.getFTP_end()*FTP + val.getFTP_range() );

            lstPointsPower.append(btnLeft);
            lstPointsPower.append(topLeft);
            lstPointsPower.append(topRight);
            lstPointsPower.append(btnRight);
            if (val.getPowerStepType() == Interval::PROGRESSIVE)
                addShapeFromPoints("PARALLELOGRAM_POWER", colorPower, lstPointsPower, 10, true);
            else
                addShapeFromPoints("PARALLELOGRAM_POWER", colorPower, lstPointsPower, 10, false);


            /// POWER TARGET LINE
            btnLeftT = QPointF(time, val.getFTP_start()*FTP - 0.1 );
            topLeftT = QPointF(time, val.getFTP_start()*FTP + 0.1 );
            btnRightT = QPointF(time + secInterval, val.getFTP_end()*FTP - 0.1 );
            topRightT = QPointF(time + secInterval, val.getFTP_end()*FTP + 0.1 );

            lstPointsPowerTarget.append(btnLeftT);
            lstPointsPowerTarget.append(topLeftT);
            lstPointsPowerTarget.append(topRightT);
            lstPointsPowerTarget.append(btnRightT);
            if (val.getPowerStepType() == Interval::PROGRESSIVE)
                addShapeFromPoints("PARALLELOGRAM_POWER_TARGET_LINE", colorTarget, lstPointsPowerTarget, 11, true);
            else
                addShapeFromPoints("PARALLELOGRAM_POWER_TARGET_LINE", colorTarget, lstPointsPowerTarget, 11, false);

        }


        /// MARKER SEPARATE INTERVALS
        if (i!=0 && i < workout.getNbInterval() ) {
            QwtPlotMarker *d_marker1 = new QwtPlotMarker();
            d_marker1->setValue( time, 0.0 );
            d_marker1->setLineStyle( QwtPlotMarker::VLine );
            d_marker1->setLabelAlignment( Qt::AlignRight | Qt::AlignBottom );
            d_marker1->setLinePen( Qt::lightGray, 0, Qt::DashLine );
            d_marker1->setZ(3);
            d_marker1->attach( this );
            lstPlotMarket.append(d_marker1);
        }
        time += secInterval;
        i++;
        lastVal = val;
    }
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::drawGraphIntervalsHeartrate() {


    QColor colorHrShape = Util::getColor(Util::SQUARE_HEARTRATE);
    QColor colorTarget = Qt::white;


    QList<QPointF> lstPointsShape;
    QPointF btnLeft;
    QPointF topLeft;
    QPointF topRight;
    QPointF btnRight;

    QList<QPointF> lstPointsTarget;
    QPointF btnLeftT;
    QPointF topLeftT;
    QPointF topRightT;
    QPointF btnRightT;

    double time = 0;
    int i=0;
    Interval lastVal;

    /////////////////////////////////////////////////////////////////////////////////////////////////////////
    foreach (Interval val, workout.getLstInterval()) {


        double secInterval = Util::convertQTimeToSecD(val.getDurationQTime());

        lstPointsShape.clear();
        lstPointsTarget.clear();
        if (val.getHRStepType() != Interval::NONE) {
            /// HR PARALLELOGRAM --------------------------------------------------------
            btnLeft = QPointF(time, val.getHR_start()*LTHR - val.getHR_range() );
            topLeft = QPointF(time, val.getHR_start()*LTHR + val.getHR_range() );
            btnRight = QPointF(time + secInterval, val.getHR_end()*LTHR - val.getHR_range() );
            topRight = QPointF(time + secInterval, val.getHR_end()*LTHR + val.getHR_range() );

            lstPointsShape.append(btnLeft);
            lstPointsShape.append(topLeft);
            lstPointsShape.append(topRight);
            lstPointsShape.append(btnRight);
            if (val.getHRStepType() == Interval::PROGRESSIVE)
                addShapeFromPoints("PARALLELOGRAM_POWER", colorHrShape, lstPointsShape, 10, true);
            else
                addShapeFromPoints("PARALLELOGRAM_POWER", colorHrShape, lstPointsShape, 10, false);


            /// HR TARGET LINE
            btnLeftT = QPointF(time, val.getHR_start()*LTHR - 0.1 );
            topLeftT = QPointF(time, val.getHR_start()*LTHR + 0.1 );
            btnRightT = QPointF(time + secInterval, val.getHR_end()*LTHR - 0.1 );
            topRightT = QPointF(time + secInterval, val.getHR_end()*LTHR + 0.1 );

            lstPointsTarget.append(btnLeftT);
            lstPointsTarget.append(topLeftT);
            lstPointsTarget.append(topRightT);
            lstPointsTarget.append(btnRightT);
            if (val.getHRStepType() == Interval::PROGRESSIVE)
                addShapeFromPoints("PARALLELOGRAM_POWER_TARGET_LINE", colorTarget, lstPointsTarget, 11, true);
            else
                addShapeFromPoints("PARALLELOGRAM_POWER_TARGET_LINE", colorTarget, lstPointsTarget, 11, false);

        }


        /// MARKER SEPARATE INTERVALS
        if (i!=0 && i < workout.getNbInterval() ) {
            QwtPlotMarker *d_marker1 = new QwtPlotMarker();
            d_marker1->setValue( time, 0.0 );
            d_marker1->setLineStyle( QwtPlotMarker::VLine );
            d_marker1->setLabelAlignment( Qt::AlignRight | Qt::AlignBottom );
            d_marker1->setLinePen( Qt::lightGray, 0, Qt::DashLine );
            d_marker1->setZ(3);
            d_marker1->attach( this );
            lstPlotMarket.append(d_marker1);
        }
        time += secInterval;
        i++;
        lastVal = val;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////
/// SLOT
////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::updateCurve(double timeNow, int value) {

    if (samples.size() > 90 )  ///After 15sec, half-graph ((15*4=60) of data, we can erase past data not shown on graph for faster replot
        samples.pop_front();

    samples.append(QPointF( timeNow, value ));
    curve->setSamples(samples);
    // curve is attached once in init(); no need to re-attach on every sample.

}





////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::moveIntervalTime(double timeNow_msec) {


    double timeNow_sec = timeNow_msec/1000.0;


    d_interval = QwtInterval( timeNow_sec - 15 , timeNow_sec + 15);
    setAxisScale( QwtPlot::xBottom, d_interval.minValue(), d_interval.maxValue(), 10 );
    setAxisScale( QwtPlot::yLeft, leftLow, leftHigh, 10);  //Calculated when receiving value or with target



    replot();
}


////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::moveIntervalTarget(double target, int range) {


    this->target = target;
    this->targetRange = range;

    //    qDebug() << "target is :" << "graphType is:" << this->type;

    /// No target
    if (target > 0) {
        noTarget = false;
    }
    else {
        noTarget = true;
        return; /// range will be calculated when received data (updateTextLabelValue)
    }



    /// Make graph smaller when target range is small (zoomed effect)
    if (range <= 10) {
        leftHigh = target + range + 7 ;
        leftLow = target - range - 7;
        if (leftLow < 0) leftLow = 0;
    }
    else {
        leftHigh = target + range + extraAfterRange ;
        leftLow = target - range - extraAfterRange;
        if (leftLow < 0) leftLow = 0;
    }

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::addMarkerInterval(double time) {

    QwtPlotMarker *d_marker1 = new QwtPlotMarker();
    d_marker1->setValue( time, 0.0 );
    d_marker1->setLineStyle( QwtPlotMarker::VLine );
    d_marker1->setLinePen( Qt::lightGray, 0, Qt::DashLine );
    d_marker1->setZ(3);
    d_marker1->attach( this );
}





////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::updateTextLabelValue(int value) {


    if(value < 0) {
        valueQwtText.setText("-");
        qwtTextLabel->setText( valueQwtText );
        return;
    }

    valueQwtText.setText(QString::number(value));
    qwtTextLabel->setText( valueQwtText );


    /// Ajust graph if we have no target right now
    if (noTarget) {
        if (type == GRAPH_TYPE::POWER) {
            if (value < leftLow || value > leftHigh) {
                leftLow = value - 15;
                leftHigh = value + 15 ;
            }
        }
        else if(type == GRAPH_TYPE::HEART_RATE) {
            if (value < leftLow || value > leftHigh) {
                leftLow = value - 10;
                leftHigh = value + 10 ;
            }
        }
        else {  //Cadence
            if (value < leftLow || value > leftHigh) {
                //                qDebug() << "Ajust scale here cadence";
                leftLow = value - 15;
                leftHigh = value + 15 ;
            }
        }
    }
    ///------------------



    int diff = value - target;

    if (isStopped) {
        canvas()->setStyleSheet(" QwtPlotCanvas { background-color: rgb(35, 35, 35); }");
        replot();
    }
    else {
        if (target < 0) {  /// no target
            canvas()->setStyleSheet(" QwtPlotCanvas { background-color: rgb(35, 35, 35); }");;
        }
        /// Change background QwtText
        else if ( (diff < (-targetRange)) ) {
            canvas()->setStyleSheet(" QwtPlotCanvas { background-color: rgb(14, 61, 170); }");
        }
        else if( (diff > targetRange) ) {
            canvas()->setStyleSheet(" QwtPlotCanvas { background-color: rgb(128, 0, 0); }");
        }
        else if ( ((diff < targetRange) && (diff > (-targetRange)))) {
            canvas()->setStyleSheet(" QwtPlotCanvas { background-color: rgb(35, 35, 35); }");
        }

    }
}




//////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::targetChanged(double percentageTarget, int range ) {

    int targetValue = percentageTarget;

    if (type == GRAPH_TYPE::POWER) {
        targetValue = qRound(percentageTarget * FTP);
    }
    else if (type == GRAPH_TYPE::HEART_RATE) {
         targetValue = qRound(percentageTarget * LTHR);
    }


    this->target = targetValue;
    this->targetRange = range;
    moveIntervalTarget(target, range);

    replot();


}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlotZoomer::addShapeFromPoints(const QString &title, const QColor &color, QList<QPointF> lstPoints, int positionZ, bool antiliasing) {

    QwtPlotShapeItem *item = new QwtPlotShapeItem(title);


    item->setRenderHint(QwtPlotItem::RenderAntialiased, antiliasing);
    item->setShape( ShapeFactory::path(lstPoints));

    QColor fillColor = color;
    fillColor.setAlpha(200);

    QPen pen(color, 1);
    pen.setJoinStyle(Qt::MiterJoin);
    item->setPen(pen);
    item->setBrush(fillColor);
    item->setZ(positionZ);
    item->attach(this);

    lstTargetShape.append(item);



}



//--------------------------------------------------------------
void WorkoutPlotZoomer::setPosition(double timeMsec)  {

    moveIntervalTime(timeMsec);
}


