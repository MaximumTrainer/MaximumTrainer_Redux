#include "powerzones.h"
#include "mycreatorplot.h"
#include "setting_keys.h"

#include <QPen>

#include "qwt_date_scale_draw.h"
#include "qwt_text.h"
//#include "qwt_plot_zoneitem.h"

#include "util.h"
#include "interval.h"


/// TimeScaleDraw
class TimeScaleDraw: public QwtScaleDraw {
public:
    TimeScaleDraw( const QTime &base ):
        baseTime( base )
    {
    }
    QwtText label( double v ) const override {
        QTime upTime = baseTime.addSecs( static_cast<int>( v ) );
        return Util::showQTimeAsString(upTime);
    }
private:
    QTime baseTime;
};


myCreatorPlot::myCreatorPlot(QWidget *parent) :QwtPlot(parent){

    this->account = qApp->property("Account").value<Account*>();

    setContentsMargins(0,10,0,0);
    max_power = 200;

    init();

}

myCreatorPlot::~myCreatorPlot() {

    lstBackgroundShape.clear();
}


void myCreatorPlot::init() {

    setAutoReplot(true);
    setMouseTracking(true);


    QTime timeStartWorkout(0, 0, 0);
    setAxisScaleDraw( QwtPlot::xBottom, new TimeScaleDraw( timeStartWorkout ) );


    d_picker = new MyQwtPlotPicker( QwtPlot::xBottom, QwtPlot::yLeft,
                                    QwtPlotPicker::CrossRubberBand, QwtPicker::AlwaysOn,
                                    this->canvas() );


    pickerMachine = new MyQwtPickerMachine();
    d_picker->setStateMachine(pickerMachine );
    connect(d_picker, qOverload<const QPointF&>(&MyQwtPlotPicker::selected), this, &myCreatorPlot::pointClicked);
}


void myCreatorPlot::updateWorkout(Workout workout) {


    this->workout = std::move(workout);


    ajustScales();

    this->detachItems();
    drawGraphIntervals();


}


void myCreatorPlot::ajustScales() {


    max_power = workout.getMaxPowerPourcent() * account->FTP;
    if (max_power < 200)
        max_power = 200;
    if (max_power < account->FTP)
        max_power = account->FTP;

    setAxisScale( QwtPlot::yLeft, 0.0, max_power+50, 50);


    int secTotal = Util::convertQTimeToSecD(workout.getDurationQTime());
    //----------------------------- 90min (1:30) -----------------------
    if (secTotal <= 5400) {

        QList<double> minorTicks;
        QList<double> mediumTicks;
        QList<double> majorTicks;

        int tick = 60; //1min
        int lastValue = 0;
        for (int i=1; i<=secTotal/60; i++) {
            minorTicks.append(tick + lastValue);
            lastValue += tick;
        }


        tick = 300; //5min
        lastValue = 0;
        majorTicks.append(0);
        for (int i=1; i<=secTotal/300; i++) {
            majorTicks.append(tick + lastValue);
            lastValue += tick;
        }


        QwtScaleDiv myDiv(0, secTotal, minorTicks, mediumTicks, majorTicks);
        setAxisScaleDiv(QwtPlot::xBottom, myDiv);
    }
    //----------------------------- 90min-180 (1:30-3:00) -----------------------
    else if (secTotal > 5400 && secTotal <= 10800) {

        QList<double> minorTicks;
        QList<double> mediumTicks;
        QList<double> majorTicks;

        int tick = 60; //1min
        int lastValue = 0;
        for (int i=1; i<=secTotal/60; i++) {
            minorTicks.append(tick + lastValue);
            lastValue += tick;
        }


        tick = 300; //5min
        lastValue = 0;
        for (int i=1; i<=secTotal/300; i++) {
            mediumTicks.append(tick + lastValue);
            lastValue += tick;
        }

        tick = 600; //10min
        lastValue = 0;
        majorTicks.append(0);
        for (int i=1; i<=secTotal/600; i++) {
            majorTicks.append(tick + lastValue);
            lastValue += tick;
        }


        QwtScaleDiv myDiv(0, secTotal, minorTicks, mediumTicks, majorTicks);
        setAxisScaleDiv(QwtPlot::xBottom, myDiv);
    }
    //----------------------------- > 3:00  -----------------------
    else {

        QList<double> minorTicks;
        QList<double> mediumTicks;
        QList<double> majorTicks;

        int tick = 600; //10min
        int lastValue = 0;
        for (int i=1; i<=secTotal/300; i++) {
            mediumTicks.append(tick + lastValue);
            lastValue += tick;
        }

        tick = 1800; //30min
        lastValue = 0;
        majorTicks.append(0);
        for (int i=1; i<=secTotal/600; i++) {
            majorTicks.append(tick + lastValue);
            lastValue += tick;
        }

        QwtScaleDiv myDiv(0, secTotal, minorTicks, mediumTicks, majorTicks);
        setAxisScaleDiv(QwtPlot::xBottom, myDiv);
    }


}


void myCreatorPlot::pointClicked(QPointF pt) {


    if (pickerMachine->rightClickPressed) {
        emit rightClickedGraph(pt);  /// this is the graph spot coords, not good place to show
        return;
    }


    QwtPlotShapeItem *shapeItemClicked = itemAt(pt);
    QSet<QString> setIntervalToHightlight;
    if ( shapeItemClicked )
    {
        setIntervalToHightlight.insert(shapeItemClicked->title().text());
        updateBackgroundHighlight(setIntervalToHightlight);
        emit shapeClicked(shapeItemClicked->title().text());
    }


}

QwtPlotShapeItem* myCreatorPlot::itemAt(const QPointF pos) {


    for (QwtPlotShapeItem *shapeItem : std::as_const(lstBackgroundShape))
    {
        if ( shapeItem->boundingRect().contains( pos ) && shapeItem->shape().contains( pos ) ) {
            return shapeItem;
        }
    }
    return nullptr;
}


void myCreatorPlot::updateBackgroundHighlight(QSet<QString> setIntervalToHightlight) {


    this->setSelectedInterval = setIntervalToHightlight;

    if (lstBackgroundShape.size() < 1) {
        return;
    }

    for (QwtPlotShapeItem *item : std::as_const(lstBackgroundShape)) {
        if (setIntervalToHightlight.contains(item->title().text()))
            item->setVisible(true);
        else
            item->setVisible(false);
    }
    replot();


}

void myCreatorPlot::removeHightlight() {

    setSelectedInterval.clear();

    for (QwtPlotShapeItem *item : std::as_const(lstBackgroundShape)) {
        item->setVisible(false);
    }
    replot();

}


void myCreatorPlot::drawGraphIntervals() {

    lstBackgroundShape.clear();


    QColor colorBg = QColor(20,103,152);

    const QVector<int> zoneBounds = PowerZones::boundsPct(account->power_zones_pct);
    QColor colorCadence = Util::getColor(Util::SQUARE_CADENCE);
    QColor colorHr = Util::getColor(Util::SQUARE_HEARTRATE);


    QList<QPointF> lstPointsBg;
    QPointF btnLeftBg;
    QPointF topLeftBg;
    QPointF topRightBg;
    QPointF btnRightBg;
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
    int maxPower = workout.getMaxPowerPourcent() * ftp + 200;
    double time = 0;
    int i=0;
    Interval lastVal;

    for (const Interval &val : workout.getLstInterval()) {


        int secInterval = Util::convertQTimeToSecD(val.getDurationQTime());


        /// BACKGROUND SHAPE
        lstPointsBg.clear();
        btnLeftBg = QPointF(time, 0 );
        topLeftBg = QPointF(time, maxPower );
        btnRightBg = QPointF(time + secInterval, 0 );
        topRightBg = QPointF(time + secInterval, maxPower );
        lstPointsBg.append(btnLeftBg);
        lstPointsBg.append(topLeftBg);
        lstPointsBg.append(topRightBg);
        lstPointsBg.append(btnRightBg);
        QString shapeName = QString::number(val.getSourceRowLstInterval());
        addShapeFromPoints(shapeName, colorBg, lstPointsBg, 1, false, true);


        /// POWER SHAPE
        lstPointsPower.clear();
        if (val.getPowerStepType() != Interval::NONE) {
            // Each block takes its zone colour (a ramp: the zone of its midpoint).
            const QColor zoneColor = PowerZones::color(
                PowerZones::zoneForPct((val.getFTP_start() + val.getFTP_end()) * 50.0, zoneBounds));
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
                addShapeFromPoints("PARALLELOGRAM_POWER", zoneColor, lstPointsPower, 11, true, false);
            else
                addShapeFromPoints("PARALLELOGRAM_POWER", zoneColor, lstPointsPower, 11, false, false);
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
                addShapeFromPoints("PARALLELOGRAM_CADENCE", colorCadence, lstPointsCadence, 10, true, false);
            else
                addShapeFromPoints("PARALLELOGRAM_CADENCE", colorCadence, lstPointsCadence, 10, false, false);
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
                addShapeFromPoints("PARALLELOGRAM_HR", colorHr, lstPointsHr, 10, true, false);
            else
                addShapeFromPoints("PARALLELOGRAM_HR", colorHr, lstPointsHr, 10, false, false);
        }


        /// Power Balance
        if (val.getRightPowerTarget() != -1) {
            QwtPlotMarker *d_markerBalance = new QwtPlotMarker();
            d_markerBalance->setValue( time + secInterval - (secInterval/2.0), 0.0 );
            d_markerBalance->setLineStyle( QwtPlotMarker::NoLine );
            d_markerBalance->setLabelAlignment( Qt::AlignHCenter | Qt::AlignTop );
            d_markerBalance->setLinePen( Qt::lightGray, 0, Qt::SolidLine );
            d_markerBalance->setZ(99);
            d_markerBalance->attach( this );

            QFont fontBold;
            fontBold.setPointSize(12);
            fontBold.setBold(true);
            QwtText balanceTxt("B");
            balanceTxt.setFont(fontBold);
            balanceTxt.setColor(Util::getColor(Util::BALANCE_POWER_TXT));
            d_markerBalance->setLabel(balanceTxt);
        }

        /// Test Interval
        if (val.isTestInterval()) {
            QwtPlotMarker *d_markerTest = new QwtPlotMarker();
            d_markerTest->setValue( time + secInterval - (secInterval/2.0), max_power);
            d_markerTest->setLineStyle( QwtPlotMarker::NoLine );
            d_markerTest->setLabelAlignment( Qt::AlignHCenter | Qt::AlignTop );
            d_markerTest->setLinePen( Qt::lightGray, 0, Qt::SolidLine );
            d_markerTest->setZ(99);
            d_markerTest->attach( this );

            QFont font;
            font.setPointSize(8);
            font.setBold(true);
            QwtText testText("Test");
            testText.setFont(font);
            testText.setColor(Util::getColor(Util::BALANCE_POWER_TXT));
            d_markerTest->setLabel(testText);
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
        }
        time += secInterval;
        i++;
        lastVal = val;
    }

}


void myCreatorPlot::addShapeFromPoints(const QString &title, const QColor &color, QList<QPointF> lstPoints,
                                       int positionZ, bool antiliasing, bool isBackgroundItem) {

    QwtPlotShapeItem  *item = new QwtPlotShapeItem(title);
    item->setRenderHint(QwtPlotItem::RenderAntialiased, antiliasing);
    item->setShape( ShapeFactory::path(lstPoints));

    QColor fillColor = color;
    fillColor.setAlpha(200);
    // Cadence is an outline, so it never reads as a power zone block.
    const bool outline = !isBackgroundItem && title == QLatin1String("PARALLELOGRAM_CADENCE");
    if (outline)
        fillColor.setAlpha(40);
    QPen pen(color, outline ? 1.5 : 0.1);
    if (outline)
        pen.setStyle(Qt::DashLine);
    pen.setJoinStyle(Qt::MiterJoin);
    item->setPen(pen);
    item->setZ(positionZ);
    item->attach(this);


    QLinearGradient linearGrad(lstPoints.at(0), lstPoints.at(1));
    linearGrad.setColorAt(0, Qt::white);
    linearGrad.setColorAt(1, color);
    QBrush brush(linearGrad);

    if (isBackgroundItem) {
        item->setBrush(brush);
        lstBackgroundShape.append(item);
        if (setSelectedInterval.contains(item->title().text()))
            item->setVisible(true);
        else
            item->setVisible(false);

    }
    else {
        item->setBrush(fillColor);
    }


}


QString myCreatorPlot::getSavePathExport() {


    return AppSettings::read(SettingKeys::LastPath::ReportExport, QDir::homePath());
}
void myCreatorPlot::savePathExport(QString path) {

    AppSettings::write(SettingKeys::LastPath::ReportExport, path);
}

