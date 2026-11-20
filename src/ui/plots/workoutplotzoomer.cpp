#include "workoutplotzoomer.h"
#include "account.h"
#include "powerzones.h"


#include <QFile>
#include <QPalette>
#include <QVBoxLayout>
#include <QPixmap>

#include "qwt_date_scale_draw.h"
#include "qwt_plot_canvas.h"
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


/// TimeScaleDraw
class TimeScaleDraw: public QwtScaleDraw {
public:
    TimeScaleDraw( const QTime &base ):
        baseTime( base )
    {
    }

    QwtText label( double v ) const override {
        QTime upTime = baseTime.addSecs( static_cast<int>( v ) );
        return upTime.toString();
    }

private:
    QTime baseTime;
};


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


void WorkoutPlotZoomer::setUserData(double FTP, double LTHR) {
    this->FTP = FTP;
    this->LTHR = LTHR;
}


void WorkoutPlotZoomer::setWorkoutData(Workout workout, GRAPH_TYPE graph, bool firstInit) {

    this->workout = std::move(workout);
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


/// init
void WorkoutPlotZoomer::init(GRAPH_TYPE graph, bool firstInit) {


    if (firstInit) {

        setAutoReplot( false );

        QTime timeStartWorkout(0,0,0,0);
        setAxisScaleDraw( QwtPlot::xBottom, new TimeScaleDraw( timeStartWorkout ) );
        setAxisScale( QwtPlot::xBottom, d_interval.minValue(), d_interval.maxValue(), 10 );

        canvas()->setStyleSheet(" QwtPlotCanvas { background-color: rgb(35, 35, 35); }");
        canvas()->setCursor(Qt::CrossCursor);

        // Match the detailed InfoWidget hero value exactly: 600 25pt 'Inter'.
        QFont fontBig("Inter");
        fontBig.setPointSize(25);
        fontBig.setWeight(QFont::DemiBold);   // CSS weight 600

        valueQwtText = QwtText("0");
        valueQwtText.setBorderRadius(3.0);
        valueQwtText.setFont(fontBig);
        valueQwtText.setRenderFlags( Qt::AlignHCenter | Qt::AlignTop );
        valueQwtText.setColor(Qt::white);
        qwtTextLabel = new QwtPlotTextLabel();
        qwtTextLabel->setText( valueQwtText );
        qwtTextLabel->attach( this );
        m_zoneLabel = new QwtPlotTextLabel();
        m_zoneLabel->setMargin(6);
        m_zoneLabel->setZ(14);
        m_zoneLabel->attach(this);
        m_zoneLabel->hide();


        /// middle vertical line
        markerLineMiddle = new MarkerItem();
        markerLineMiddle->setZ(13);
        markerLineMiddle->attach(this);


        QColor colorCurve;
        icon = new IconItem();

        if (graph == GRAPH_TYPE::POWER) {
            extraAfterRange = 15;
            colorCurve = Util::getColor(Util::LINE_POWER);
            icon->iconType = "POWER";
        }
        else if (graph == GRAPH_TYPE::CADENCE) {
            extraAfterRange = 5;
            colorCurve = Util::getColor(Util::LINE_CADENCE);
            icon->iconType = "CADENCE";

        }
        else if (graph == GRAPH_TYPE::HEART_RATE) {
            extraAfterRange = 10;
            colorCurve = Util::getColor(Util::LINE_HEARTRATE);
            icon->iconType = "HEART_RATE";
        }

        icon->attach(this);
        icon->setZ(12);
        // Let the icon measure the value number so it can sit just left of it.
        icon->valueFont = std::move(fontBig);
        icon->valueText = "0";


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

    for (const Interval &val : workout.getLstInterval()) {


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
                addShapeFromPoints("PARALLELOGRAM_CADENCE_TARGET_LINE", colorTarget, lstPointsCadenceTarget, 11, true);
            else
                addShapeFromPoints("PARALLELOGRAM_CADENCE_TARGET_LINE", colorTarget, lstPointsCadenceTarget, 11, false);

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


void WorkoutPlotZoomer::drawGraphIntervalsPower() {


    const Account *zoneAccount = qApp->property("Account").value<Account*>();
    const QVector<int> zoneBounds = PowerZones::boundsPct(zoneAccount ? zoneAccount->power_zones_pct : QList<int>{});
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


    for (const Interval &val : workout.getLstInterval()) {


        double secInterval = Util::convertQTimeToSecD(val.getDurationQTime());


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
            const QColor zoneColor = PowerZones::color(
                PowerZones::zoneForPct((val.getFTP_start() + val.getFTP_end()) * 50.0, zoneBounds));
            const QBrush zoneBrush = PowerZones::brush(val.getFTP_start() * 100.0, val.getFTP_end() * 100.0, zoneBounds, 200);
            addShapeFromPoints("PARALLELOGRAM_POWER", zoneColor, lstPointsPower, 10,
                               val.getPowerStepType() == Interval::PROGRESSIVE, zoneBrush);


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

    for (const Interval &val : workout.getLstInterval()) {


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

/// SLOT
void WorkoutPlotZoomer::updateCurve(double timeNow, int value) {

    if (samples.size() > 90 )  ///After 15sec, half-graph ((15*4=60) of data, we can erase past data not shown on graph for faster replot
        samples.pop_front();

    samples.append(QPointF( timeNow, value ));
    curve->setSamples(samples);
    // curve is attached once in init(); no need to re-attach on every sample.

}


void WorkoutPlotZoomer::moveIntervalTime(double timeNow_msec) {


    double timeNow_sec = timeNow_msec/1000.0;


    d_interval = QwtInterval( timeNow_sec - 15 , timeNow_sec + 15);
    setAxisScale( QwtPlot::xBottom, d_interval.minValue(), d_interval.maxValue(), 10 );
    setAxisScale( QwtPlot::yLeft, leftLow, leftHigh, leftHigh - leftLow > 80 ? 0 : 10);  // auto steps once widened


    replot();
}


void WorkoutPlotZoomer::moveIntervalTarget(double target, int range) {


    this->target = target;
    this->targetRange = range;


    /// No target
    if (target > 0) {
        noTarget = false;
    }
    else {
        noTarget = true;
        return; /// range will be calculated when received data (updateTextLabelValue)
    }


    /// Make graph smaller when target range is small (zoomed effect)
    const int margin = range <= 10 ? 7 : extraAfterRange;
    targetHigh = int(target) + range + margin;
    targetLow = qMax(0, int(target) - range - margin);
    fitAxisToLiveValues();

}


// The target band, widened so the live line and the value on screen stay in view.
void WorkoutPlotZoomer::fitAxisToLiveValues() {
    leftLow = targetLow;
    leftHigh = targetHigh;
    if (m_lastValue < 0)
        return;
    int lowest = m_lastValue;
    int highest = m_lastValue;
    for (const QPointF &sample : std::as_const(samples)) {
        lowest = qMin(lowest, int(sample.y()));
        highest = qMax(highest, int(sample.y()));
    }
    const int margin = qMax(3, extraAfterRange / 2);
    leftLow = qMax(0, qMin(targetLow, lowest - margin));
    leftHigh = qMax(targetHigh, highest + margin);
    // Headroom so the line never runs under the big value drawn at the top.
    if (m_valueOverlayVisible && leftHigh > targetHigh)
        leftHigh += (leftHigh - leftLow) / 4;
}


void WorkoutPlotZoomer::addMarkerInterval(double time) {

    QwtPlotMarker *d_marker1 = new QwtPlotMarker();
    d_marker1->setValue( time, 0.0 );
    d_marker1->setLineStyle( QwtPlotMarker::VLine );
    d_marker1->setLinePen( Qt::lightGray, 0, Qt::DashLine );
    d_marker1->setZ(3);
    d_marker1->attach( this );
}


void WorkoutPlotZoomer::setValueOverlayVisible(bool visible) {
    m_valueOverlayVisible = visible;
    if (m_zoneLabel && !visible)
        m_zoneLabel->hide();
    if (qwtTextLabel)
        qwtTextLabel->setVisible(visible);
    if (icon)
        icon->setVisible(visible);
    replot();
}

void WorkoutPlotZoomer::updateTextLabelValue(int value) {


    if(value < 0) {
        valueQwtText.setText("-");
        qwtTextLabel->setText( valueQwtText );
        if (icon) icon->valueText = "-";
        return;
    }

    valueQwtText.setText(QString::number(value));
    qwtTextLabel->setText( valueQwtText );
    if (icon) icon->valueText = valueQwtText.text();


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
                leftLow = value - 15;
                leftHigh = value + 15 ;
            }
        }
    }
    else {
        m_lastValue = value;
        fitAxisToLiveValues();
    }
    ///------------------


    int diff = value - target;

    if (isStopped) {
        setCanvasColor(CANVAS_NEUTRAL);
        replot();
    }
    else {
        if (target < 0) {  /// no target
            setCanvasColor(CANVAS_NEUTRAL);
        }
        /// Change background QwtText
        else if ( (diff < (-targetRange)) ) {
            setCanvasColor(CANVAS_BELOW);
        }
        else if( (diff > targetRange) ) {
            setCanvasColor(CANVAS_ABOVE);
        }
        else {
            setCanvasColor(CANVAS_NEUTRAL);
        }

    }
}

void WorkoutPlotZoomer::setCanvasColor(CanvasColor color) {

    if (m_canvasColor == color)
        return;
    m_canvasColor = color;

    // Same palette as the card's side bar: red below, amber above.
    switch (color) {
    case CANVAS_BELOW:
        canvas()->setStyleSheet(QStringLiteral(" QwtPlotCanvas { background-color: rgb(84, 34, 36); }"));
        break;
    case CANVAS_ABOVE:
        canvas()->setStyleSheet(QStringLiteral(" QwtPlotCanvas { background-color: rgb(84, 58, 22); }"));
        break;
    case CANVAS_NEUTRAL:
    default:
        canvas()->setStyleSheet(QStringLiteral(" QwtPlotCanvas { background-color: rgb(31, 31, 37); }"));
        break;
    }

    // QwtPlotCanvas caches the styled background (including the rounded-corner
    // border) in its backing store, which a bare setStyleSheet() does not
    // refresh — so the previous colour lingers at the canvas edge/corners until
    // some later full repaint. Invalidate the cache and repaint immediately.
    if (auto *plotCanvas = qobject_cast<QwtPlotCanvas*>(canvas()))
        plotCanvas->invalidateBackingStore();
    canvas()->update();
}


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
    updateZoneLabel(type == GRAPH_TYPE::POWER ? percentageTarget : -1);

    replot();


}


void WorkoutPlotZoomer::addShapeFromPoints(const QString &title, const QColor &color, QList<QPointF> lstPoints, int positionZ, bool antiliasing,
                                           const QBrush &zoneBrush) {

    QwtPlotShapeItem *item = new QwtPlotShapeItem(title);


    item->setRenderHint(QwtPlotItem::RenderAntialiased, antiliasing);
    item->setShape( ShapeFactory::path(lstPoints));

    QColor fillColor = color;
    fillColor.setAlpha(200);
    // Same look as the main graph: cadence is an outline, power blocks take their zone colour.
    const bool outline = title == QLatin1String("PARALLELOGRAM_CADENCE");
    if (outline)
        fillColor.setAlpha(40);
    QPen pen(color, outline ? 1.5 : 1);
    if (outline)
        pen.setStyle(Qt::DashLine);
    if (zoneBrush.gradient())
        pen.setBrush(zoneBrush);
    pen.setJoinStyle(Qt::MiterJoin);
    item->setPen(pen);
    item->setBrush(zoneBrush.style() == Qt::NoBrush ? QBrush(fillColor) : zoneBrush);
    item->setZ(positionZ);
    item->attach(this);

    lstTargetShape.append(item);


}


void WorkoutPlotZoomer::setPosition(double timeMsec)  {

    moveIntervalTime(timeMsec);
}


// The power target's zone as a coloured tag (graph-only cards have no title
// row for the zone chip; when the title row shows, the overlay is off).
void WorkoutPlotZoomer::updateZoneLabel(double percentageTarget)
{
    if (!m_zoneLabel)
        return;
    if (percentageTarget <= 0 || !m_valueOverlayVisible) {
        m_zoneLabel->hide();
        return;
    }
    const auto *account = qApp->property("Account").value<Account *>();
    const QVector<int> bounds = PowerZones::boundsPct(account ? account->power_zones_pct : QList<int>{});
    const int zone = PowerZones::zoneForPct(percentageTarget * 100.0, bounds);
    // QwtText has no padding; thin spaces keep the text off the badge edge.
    const QString pad = QStringLiteral("\u2009\u2009");
    QwtText text(pad + PowerZones::label(zone, int(bounds.size()) + 1,
                                         account ? account->power_zone_names : QStringList{}) + pad);
    QFont font("Inter");
    font.setPointSize(9);
    font.setBold(true);
    text.setFont(font);
    text.setColor(QColor("#141418"));
    text.setBackgroundBrush(PowerZones::color(zone));
    text.setBorderRadius(6);
    text.setRenderFlags(Qt::AlignRight | Qt::AlignTop);
    m_zoneLabel->setText(text);
    m_zoneLabel->show();
}
