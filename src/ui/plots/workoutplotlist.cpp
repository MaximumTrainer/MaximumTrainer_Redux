#include "workoutplotlist.h"
#include "util.h"

#include "qwt_date_scale_draw.h"
#include "qwt_plot_shapeitem.h"
#include "qwt_text.h"

#include "util.h"
#include <QVBoxLayout>
#include <QPen>


/// TimeScaleDraw
class TimeScaleDraw: public QwtScaleDraw {
public:
    TimeScaleDraw( const QTime &base ):
        baseTime( base )
    {
    }
    virtual QwtText label( double v ) const {
        QTime upTime = baseTime.addSecs( static_cast<int>( v ) );
        return Util::showQTimeAsString(upTime);
    }
private:
    QTime baseTime;
};


WorkoutPlotList::WorkoutPlotList(QWidget *parent) : QwtPlot(parent) {

    account = qApp->property("Account").value<Account*>();

    max_power = 200;
}


/// setWorkoutData
void WorkoutPlotList::setWorkoutData(Workout workout) {

    this->workout = workout;
    init();
}


/// init
void WorkoutPlotList::init() {

    setAutoReplot( false );

    int max_power = workout.getMaxPowerPourcent() * account->FTP;

    if (max_power < 200)
        max_power = 200;

    setAxisScale( QwtPlot::yLeft, 0.0, max_power+25, 50);


    QTime timeStartWorkout(0, 0, 0);
    setAxisScaleDraw( QwtPlot::xBottom, new TimeScaleDraw( timeStartWorkout ) );


    QTime durationQTime = workout.getDurationQTime();
    int secTotal = Util::convertQTimeToSecD(durationQTime);
    setAxisScale( QwtPlot::xBottom, 0, secTotal, 1800 );


    canvas()->setStyleSheet(" QwtPlotCanvas { background-color: rgb(35, 35, 35); }");
    canvas()->setCursor(Qt::ArrowCursor);


    enableAxis(0, false);
    enableAxis(1, false);
    enableAxis(2, false);

    drawGraphIntervals();


}


/// drawGraphIntervals
void WorkoutPlotList::drawGraphIntervals() {


    QColor colorPower = Util::getColor(Util::SQUARE_POWER);
    QColor colorCadence = Util::getColor(Util::SQUARE_CADENCE);
    QColor colorHr = Util::getColor(Util::SQUARE_HEARTRATE);

    QList<QPointF> lstPointsPower;
    QPointF btnLeft;
    QPointF topLeft;
    QPointF topRight;
    QPointF btnRight;
    QList<QPointF> lstPointsCadence;
    QPointF btnLeftC;
    QPointF topLeftC;
    QPointF topRightC;
    QPointF btnRightC;
    QList<QPointF> lstPointsHr;
    QPointF btnLeftH;
    QPointF topLeftH;
    QPointF topRightH;
    QPointF btnRightH;


    int ftp = account->FTP;
    int lthr = account->LTHR;
    double time = 0;
    Interval lastVal;

    for (const Interval &val : workout.getLstInterval()) {

        int secInterval = Util::convertQTimeToSecD(val.getDurationQTime());


        /// POWER SHAPE
        lstPointsPower.clear();
        if (val.getPowerStepType() != Interval::NONE) {
            /// POWER PARALLELOGRAM --------------------------------------------------------
            btnLeft = QPointF(time, val.getFTP_start()*ftp - val.getFTP_range() );
            topLeft = QPointF(time, val.getFTP_start()*ftp + val.getFTP_range() );

            btnRight = QPointF(time + secInterval, val.getFTP_end()*ftp - val.getFTP_range() );
            topRight = QPointF(time + secInterval, val.getFTP_end()*ftp + val.getFTP_range() );

            lstPointsPower.append(btnLeft);
            lstPointsPower.append(topLeft);
            lstPointsPower.append(topRight);
            lstPointsPower.append(btnRight);
            if (val.getPowerStepType() == Interval::PROGRESSIVE)
                addShapeFromPoints("PARALLELOGRAM_POWER", colorPower, lstPointsPower, 11, true);
            else
                addShapeFromPoints("PARALLELOGRAM_POWER", colorPower, lstPointsPower, 11, false);
        }


        /// CADENCE SHAPE
        lstPointsCadence.clear();
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
        }


        /// HR SHAPE
        lstPointsHr.clear();
        if (val.getHRStepType() != Interval::NONE) {
            /// HR PARALLELOGRAM --------------------------------------------------------
            btnLeftH = QPointF(time, val.getHR_start()*lthr - val.getHR_range() );
            topLeftH = QPointF(time, val.getHR_start()*lthr + val.getHR_range() );
            btnRightH = QPointF(time + secInterval, val.getHR_end()*lthr - val.getHR_range() );
            topRightH = QPointF(time + secInterval, val.getHR_end()*lthr + val.getHR_range() );

            lstPointsHr.append(btnLeftH);
            lstPointsHr.append(topLeftH);
            lstPointsHr.append(topRightH);
            lstPointsHr.append(btnRightH);
            if (val.getHRStepType() == Interval::PROGRESSIVE)
                addShapeFromPoints("PARALLELOGRAM_HR", colorHr, lstPointsHr, 10, true);
            else
                addShapeFromPoints("PARALLELOGRAM_HR", colorHr, lstPointsHr, 10, false);
        }


        time += secInterval;
        lastVal = val;
    }

    replot();

}


void WorkoutPlotList::addShapeFromPoints(const QString &title, const QColor &color, QList<QPointF> lstPoints, int positionZ, bool antiliasing) {

    QwtPlotShapeItem  *item = new QwtPlotShapeItem(title);
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

}

