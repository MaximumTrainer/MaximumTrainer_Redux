#include "workoutplot.h"

#include <QPen>
#include "qwt_text.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTimer>
#include <QToolButton>
#include <QKeyEvent>

#include "qwt_date_scale_draw.h"
#include "qwt_plot_zoneitem.h"
#include "qwt_plot_layout.h"

#include "util.h"
#include "curvedatacadence.h"
#include "curvedatapower.h"
#include "curvedataheartrate.h"
#include "curvedataspeed.h"






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
        return Util::showQTimeAsString(upTime);
    }
private:
    QTime baseTime;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
WorkoutPlot::~WorkoutPlot() {

    qDeleteAll(lstPlotMarket);
    qDeleteAll(lstPlotMarkerSeperatorInterval);
    qDeleteAll(lstTargetPower);
    qDeleteAll(lstTargetCadence);
    qDeleteAll(lstTargetHr);
    lstTargetPower.clear();
    lstTargetCadence.clear();
    lstTargetHr.clear();
    lstPlotMarket.clear();
    lstPlotMarkerSeperatorInterval.clear();

}


WorkoutPlot::WorkoutPlot(QWidget *parent) : QwtPlot(parent) {

    //     setMouseTracking(true);

    isStopped = true;
    lastXClicked = 0;
    max_power = 200;

    // Remove the extra on the right in the canvas caused by extra label width on bottom axis scale
    this->plotLayout()->setAlignCanvasToScales(true);

    bottomAxisLimitSec = 300;
    showingMessage = false;

    d_picker = new MyQwtPlotPicker( QwtPlot::xBottom, QwtPlot::yLeft,
                                    QwtPlotPicker::CrossRubberBand, QwtPicker::AlwaysOn,
                                    this->canvas());

    QPen pen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    d_picker->setTrackerPen(pen);
    pickerMachine = new MyQwtPickerMachine();
    d_picker->setStateMachine(pickerMachine );
    connect(d_picker, SIGNAL(selected(QPointF)), this, SLOT(pointClicked(QPointF)) );

    canvas()->setStyleSheet(" QwtPlotCanvas { background-color: rgb(35, 35, 35); }");
    canvas()->setCursor(Qt::CrossCursor);



}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::clearPlot() {

    //Delete old target
    for (int i=0; i<lstTargetPower.size(); i++) {
        lstTargetPower.at(i)->detach();
    }
    for (int i=0; i<lstTargetCadence.size(); i++) {
        lstTargetCadence.at(i)->detach();
    }
    for (int i=0; i<lstTargetHr.size(); i++) {
        lstTargetHr.at(i)->detach();
    }
    for (int i=0; i<lstPlotMarket.size(); i++) {
        lstPlotMarket.at(i)->detach();
    }
    for (int i=0; i<lstPlotMarkerSeperatorInterval.size(); i++) {
        lstPlotMarkerSeperatorInterval.at(i)->detach();
    }

}

//////////////////////////////////////////////////////////////
void WorkoutPlot::setUserData(double FTP, double LTHR) {
    this->FTP = FTP;
    this->LTHR = LTHR;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::setWorkoutData(Workout workout, bool firstInit) {

    this->workout = workout;

    if (!firstInit) {
        clearPlot();
    }

    init(firstInit);
    replot();
}





////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::updateAxisHelper() {


    max_power = workout.getMaxPowerPourcent() * FTP;
    if (max_power < 200)
        max_power = 200;
    if (max_power < FTP)
        max_power = FTP;

    setAxisScale( QwtPlot::yLeft, 0.0, max_power+50, 50);
    QTime timeStartWorkout(0, 0, 0);
    setAxisScaleDraw( QwtPlot::xBottom, new TimeScaleDraw( timeStartWorkout ) );

    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {

        QList<double> minorTicks;
        QList<double> mediumTicks;
        QList<double> majorTicks;

        minorTicks.append(60);
        minorTicks.append(120);
        minorTicks.append(180);
        minorTicks.append(240);

        majorTicks.append(0);
        majorTicks.append(300);

        QwtScaleDiv myDiv(0, 300, minorTicks, mediumTicks, majorTicks);
        setAxisScaleDiv(QwtPlot::xBottom, myDiv);
    }
    else {
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

            int tick = 300; //5min
            int lastValue = 0;
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
    }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool WorkoutPlot::eventFilter(QObject *watched, QEvent *event) {


    if (watched == widgetDifficulty || watched == labelDifficulty
            || watched == btnDecreaseDifficulty || watched == btnIncreaseDifficulty) {
        if (event->type() == QEvent::HoverEnter || event->type() == QEvent::Enter) {
            d_picker->setEnabled(false);
        }
        else if (event->type() == QEvent::HoverLeave ||  event->type() == QEvent::Leave) {
            d_picker->setEnabled(true);
        }
    }
    return false;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::init(bool firstInit) {

    setAutoReplot(false);

    updateAxisHelper();



    if (firstInit) {
        labelMsg = new FaderLabel(this);
        // White text: this label sits on the always-dark plot canvas. Set it
        // explicitly rather than relying on a cascading QLabel rule.
        labelMsg->setStyleSheet("color : white;");
        labelMsg->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        labelMsg->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
        labelMsg->hide();


        labelAlertMessage = new FaderLabel(this);
        labelAlertMessage->setMargin(10);
        labelAlertMessage->setStyleSheet("background-color : rgba(1,1,1,220);"
                                         "border-radius: 10px;"
                                         "border: 1px solid beige;"
                                         "color : white;");
        labelAlertMessage->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        labelAlertMessage->setAlignment(Qt::AlignTop | Qt::AlignLeft);
        labelAlertMessage->hide();



        labelMsgInterval = new FaderLabel(this);
        labelMsgInterval->setStyleSheet("background-color : rgba(1,1,1,220);"
                                        "border-radius: 15px;"
                                        "border: 1px solid beige;"
                                        "color : white;");
        labelMsgInterval->setMinimumHeight(75);
        labelMsgInterval->setWordWrap(true);
        labelMsgInterval->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
        labelMsgInterval->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        labelMsgInterval->hide();



        // Difficulty nudge: explicit -/+ buttons around a read-only label. A
        // QSpinBox here rendered its native +/- buttons wrong under the app-wide
        // stylesheet on Windows (the up-button collapsed onto the line edit, so
        // "+" showed a text cursor and did nothing — #302). The value is not
        // directly editable; it is only changed via the buttons / arrow keys.
        difficultyPercent = 0;

        btnDecreaseDifficulty = new QToolButton(this);
        btnDecreaseDifficulty->setText(QStringLiteral("−"));
        btnDecreaseDifficulty->setCursor(Qt::PointingHandCursor);
        btnDecreaseDifficulty->setFocusPolicy(Qt::NoFocus);
        btnDecreaseDifficulty->setAutoRepeat(true);
        btnDecreaseDifficulty->setFixedSize(40, 40);

        btnIncreaseDifficulty = new QToolButton(this);
        btnIncreaseDifficulty->setText(QStringLiteral("+"));
        btnIncreaseDifficulty->setCursor(Qt::PointingHandCursor);
        btnIncreaseDifficulty->setFocusPolicy(Qt::NoFocus);
        btnIncreaseDifficulty->setAutoRepeat(true);
        btnIncreaseDifficulty->setFixedSize(40, 40);

        labelDifficulty = new QLabel(QStringLiteral("0%"), this);
        labelDifficulty->setAlignment(Qt::AlignCenter);
        labelDifficulty->setMinimumWidth(56);

        widgetDifficulty = new QWidget(this);
        widgetDifficulty->setObjectName(QStringLiteral("widgetDifficulty"));
        widgetDifficulty->setFixedHeight(40);
        widgetDifficulty->setCursor(Qt::ArrowCursor);
        widgetDifficulty->setStyleSheet(
            "QWidget#widgetDifficulty { background-color: rgb(35,35,35); border-radius: 4px; }"
            "QLabel { color: white; font-weight: bold; }"
            "QToolButton { background-color: rgb(60,60,60); color: white; border: none;"
            "              border-radius: 3px; font-size: 22px; font-weight: bold; }"
            "QToolButton:hover { background-color: rgb(90,90,90); }"
            "QToolButton:pressed { background-color: rgb(120,120,120); }");

        QHBoxLayout *layoutDifficulty = new QHBoxLayout(widgetDifficulty);
        layoutDifficulty->setContentsMargins(0, 0, 0, 0);
        layoutDifficulty->setSpacing(0);
        layoutDifficulty->addWidget(btnDecreaseDifficulty);
        layoutDifficulty->addWidget(labelDifficulty);
        layoutDifficulty->addWidget(btnIncreaseDifficulty);

        connect(btnIncreaseDifficulty, &QToolButton::clicked, this, &WorkoutPlot::increaseDifficulty);
        connect(btnDecreaseDifficulty, &QToolButton::clicked, this, &WorkoutPlot::decreaseDifficulty);


        QFont fontBig;
        fontBig.setPointSize(20);
        labelMsg->setFont(fontBig);
        labelMsgInterval->setFont(fontBig);

        QFont fontAlert;
        fontAlert.setPointSize(12);
        labelAlertMessage->setFont(fontAlert);


        //------------------------------
        widgetCanvas = this->canvas();
        QGridLayout *gridLayout = new QGridLayout();
        gridLayout->setContentsMargins(0, 0, 0, 0);
        gridLayout->setSpacing(0);
        gridLayout->setContentsMargins(0,0,0,0);
        gridLayout->addWidget(widgetDifficulty, 0, 1, 1, 1, Qt::AlignRight | Qt::AlignTop);
        gridLayout->addWidget(labelAlertMessage, 0, 0, 0, 0, Qt::AlignLeft | Qt::AlignTop);
        gridLayout->addWidget(labelMsg, 0, 0, 2, 2);
        gridLayout->addWidget(labelMsgInterval, 0, 0, 2, 2, Qt::AlignBottom);
        widgetCanvas->setLayout(gridLayout);

        // Suppress the plot's click-picker while the pointer is over the
        // difficulty control so button clicks don't also register on the graph.
        widgetDifficulty->installEventFilter(this);
        labelDifficulty->installEventFilter(this);
        btnDecreaseDifficulty->installEventFilter(this);
        btnIncreaseDifficulty->installEventFilter(this);
//        this->installEventFilter(this);

        labelMsg->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        labelAlertMessage->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        labelMsgInterval->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        //---------------------------------------




        QColor colorCadence = Util::getColor(Util::LINE_CADENCE);
        QColor colorPower = Util::getColor(Util::LINE_POWER);
        QColor colorHeartRate = Util::getColor(Util::LINE_HEARTRATE);
        QColor colorSpeed = Util::getColor(Util::LINE_SPEED);


        grid = new QwtPlotGrid();
        grid->setZ(1);
        grid->enableX(false);
        grid->attach( this );
        grid->enableXMin(false);
        grid->enableYMin(false);
        grid->setMajorPen(QPen(Qt::gray, 0, Qt::DotLine));
        grid->setMinorPen(QPen(Qt::gray, 0, Qt::DotLine));

        hrCurve = new QwtPlotCurve();
        hrCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
        hrCurve->setStyle( QwtPlotCurve::Lines );
        hrCurve->setPaintAttribute( QwtPlotCurve::ClipPolygons, false );
        hrCurve->setZ(20);
        hrCurve->setPen( QPen( colorHeartRate, 1.5 ) );
        hrCurve->setPaintAttribute(QwtPlotCurve::FilterPoints, true);
        hrCurve->setData(new CurveDataHeartRate() );
        hrCurve->attach(this);

        powerCurve = new QwtPlotCurve();
        powerCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
        powerCurve->setStyle( QwtPlotCurve::Lines );
        powerCurve->setPaintAttribute( QwtPlotCurve::ClipPolygons, false );
        powerCurve->setZ(22);
        powerCurve->setPen( QPen( colorPower, 1.5 ) );
        powerCurve->setPaintAttribute(QwtPlotCurve::FilterPoints, true);
        powerCurve->setData(new CurveDataPower() );
        powerCurve->attach(this);

        cadenceCurve = new QwtPlotCurve();
        cadenceCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
        cadenceCurve->setStyle( QwtPlotCurve::Lines );
        cadenceCurve->setPaintAttribute( QwtPlotCurve::ClipPolygons, false );
        cadenceCurve->setZ(21);
        cadenceCurve->setPen( QPen( colorCadence, 1.5 ) );
        cadenceCurve->setPaintAttribute(QwtPlotCurve::FilterPoints, true);
        cadenceCurve->setData(new CurveDataCadence() );
        cadenceCurve->attach(this);

        speedCurve = new QwtPlotCurve();
        speedCurve->setRenderHint( QwtPlotItem::RenderAntialiased, true );
        speedCurve->setStyle( QwtPlotCurve::Lines );
        speedCurve->setPaintAttribute( QwtPlotCurve::ClipPolygons, false );
        speedCurve->setZ(21);
        speedCurve->setPen( QPen( colorSpeed, 1.5 ) );
        speedCurve->setPaintAttribute(QwtPlotCurve::FilterPoints, true);
        speedCurve->setData(new CurveDataSpeed() );
        speedCurve->attach(this);
    }



    zoneDone = new ZoneItem( "Zone Done");
    zoneDone->setColor( Util::getColor(Util::DONE) );
    zoneDone->setInterval( 0, 0 );
    zoneDone->setVisible( true );
    zoneDone->attach( this );



    if (workout.getWorkoutNameEnum() != Workout::OPEN_RIDE) {

        drawGraphIntervals();

        zoneToDo = new ZoneItem( "Zone ToDo");
        zoneToDo->setColor( Util::getColor(Util::NOT_DONE) );
        zoneToDo->setInterval( 0, Util::convertQTimeToSecD(workout.getDurationQTime()) );
        zoneToDo->setVisible( true );
        zoneToDo->attach( this );
    }
    else {

        zoneToDo = new ZoneItem( "Zone ToDo");
        zoneToDo->setColor( Util::getColor(Util::NOT_DONE) );
        zoneToDo->setInterval( 0, 300 );
        zoneToDo->setVisible( true );
        zoneToDo->attach( this );
    }




    qDebug() << "workoutPlot init done";

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::setStopped(bool stopped) {


    this->isStopped = stopped;


}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::setMessage(QString msg) {

    labelMsg->setText(msg);
    labelMsg->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    labelMsg->setStyleSheet("background-color : rgba(1,1,1,140); color : white;");
    labelMsg->show();
    labelMsg->fadeIn(300);
    showingMessage = true;


}
//---------------------------------------
void WorkoutPlot::removeMainMessage() {
    labelMsg->fadeOut(300);
    showingMessage = false;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::setMessageEndWorkout() {



    QString msgEndWorkout = tr("Workout completed!");
    labelMsg->setText(msgEndWorkout);
    labelMsg->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    //    labelMsg->setStyleSheet("background-color : rgba(35,35,35,0);");


    //    labelMsg->setMaximumHeight(60);
    //    labelMsg->setStyleSheet("background-color : rgba(1,1,1,140);"
    //                            "border-radius: 15px;"
    //                            "border: 1px solid beige;");

    labelMsg->show();
    labelMsg->fadeInAndFadeOutAfterPause(300,2000,5000);

}







//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// setDisplayMessage
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::setDisplayIntervalMessage(bool fadeIn, QString text, int timeToDisplayInSec) {

    labelMsgInterval->setText(text);
    labelMsgInterval->show();

    if (fadeIn)
        labelMsgInterval->fadeInAndFadeOutAfterPause(400, 1000, timeToDisplayInSec*1000);
    else
        labelMsgInterval->fadeInAndFadeOutAfterPause(1, 1000, timeToDisplayInSec*1000);


}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::setAlertMessage(bool fadeIn, bool fadeOut, QString text, int timeToDisplay) {

    labelAlertMessage->setText(text);
    labelAlertMessage->show();


    if (!fadeOut) {
        if (fadeIn)
            labelAlertMessage->fadeIn(400);
    }
    else {
        if (fadeIn)
            labelAlertMessage->fadeInAndFadeOutAfterPause(400, 1000, timeToDisplay);
        else
            labelAlertMessage->fadeInAndFadeOutAfterPause(1, 1000, timeToDisplay);
    }


}

void WorkoutPlot::removeAlertMessage() {
    labelAlertMessage->fadeOut(300);
}





//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// drawGraphIntervals
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::drawGraphIntervals() {


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


    double time = 0;
    int i=0;
    Interval lastVal;

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    foreach (Interval val, workout.getLstInterval()) {


        //        int secInterval = Util::convertQTimeToSec(val.getDurationQTime());

        double secInterval = Util::convertQTimeToSecD(val.getDurationQTime());


        /// POWER SHAPE
        lstPointsPower.clear();
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
            btnLeftH = QPointF(time, val.getHR_start()*LTHR - val.getHR_range() );
            topLeftH = QPointF(time, val.getHR_start()*LTHR + val.getHR_range() );
            btnRightH = QPointF(time + secInterval, val.getHR_end()*LTHR - val.getHR_range() );
            topRightH = QPointF(time + secInterval, val.getHR_end()*LTHR + val.getHR_range() );

            lstPointsHr.append(btnLeftH);
            lstPointsHr.append(topLeftH);
            lstPointsHr.append(topRightH);
            lstPointsHr.append(btnRightH);
            if (val.getHRStepType() == Interval::PROGRESSIVE)
                addShapeFromPoints("PARALLELOGRAM_HR", colorHr, lstPointsHr, 10, true);
            else
                addShapeFromPoints("PARALLELOGRAM_HR", colorHr, lstPointsHr, 10, false);
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
            lstPlotMarket.append(d_markerBalance);
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
            lstPlotMarket.append(d_markerTest);
        }


        /// MARKER SEPARATE INTERVALS
        if (i!=0 && i < workout.getNbInterval() ) {
            QwtPlotMarker *d_marker1 = new QwtPlotMarker();
            d_marker1->setValue( time, 0.0 );
            d_marker1->setLineStyle( QwtPlotMarker::VLine );
            d_marker1->setLinePen( Qt::lightGray, 0, Qt::DashLine );
            d_marker1->setZ(3);
            d_marker1->attach( this );
            lstPlotMarkerSeperatorInterval.append(d_marker1);


        }
        time += secInterval;
        i++;
        lastVal = val;
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::setSpinBoxDisabled() {

    widgetDifficulty->hide();
}





///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::addMarkerInterval(double time) {

    QwtPlotMarker *d_marker1 = new QwtPlotMarker();
    d_marker1->setValue( time, 0.0 );
    d_marker1->setLineStyle( QwtPlotMarker::VLine );
    d_marker1->setLinePen( Qt::lightGray, 0, Qt::DashLine );
    d_marker1->setZ(3);
    d_marker1->attach( this );
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::increaseDifficulty() {
    setDifficulty(difficultyPercent + 1);
}

void WorkoutPlot::decreaseDifficulty() {
    setDifficulty(difficultyPercent - 1);
}

void WorkoutPlot::setDifficulty(int percentageIncrease) {

    const int clamped = qBound(-100, percentageIncrease, 100);
    if (clamped == difficultyPercent)
        return;

    difficultyPercent = clamped;
    labelDifficulty->setText((clamped > 0 ? QStringLiteral("+") : QString())
                             + QString::number(clamped) + QStringLiteral("%"));
    emit workoutDifficultyChanged(clamped);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::pointClicked(QPointF pt) {

    qDebug() << "point clicked**********************" << pt.x() << "lastXClicked:" << lastXClicked;


    if (lastXClicked == pt.x())
        return;
    lastXClicked = pt.x();


    //find interval clicked
    int secCounter = 0;
    int nbIntervalClicked = 0;
    for (int i=0; i<workout.getLstInterval().size(); i++) {

        Interval interval = workout.getInterval(i);
        double secInterval = Util::convertQTimeToSecD(interval.getDurationQTime());
        if (pt.x() >= secCounter && pt.x() < secCounter+secInterval ) {
            nbIntervalClicked = i;
            break;
        }
        secCounter += secInterval;
    }

    qDebug() << "interval clicked is:" << nbIntervalClicked << "x:" << pt.x();

    emit intervalClicked(nbIntervalClicked, pt.x(), secCounter, true);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::updateMarkerTimeNow(double timeNow_sec) {


    bool updateAxisBottom = false;



    if (showingMessage) {
        removeMainMessage();
    }


    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE) {

        /// Only ajust axes once every 5min
        if (timeNow_sec >= bottomAxisLimitSec ) {
            bottomAxisLimitSec += 300;
            updateAxisBottom = true;
        }

        if (updateAxisBottom) {
            //-----------------------------------  <1:30 ------------------------
            if (timeNow_sec < 5400) {

                QList<double> minorTicks;
                QList<double> mediumTicks;
                QList<double> majorTicks;

                int tick = 60; //1min
                int lastValue = 0;
                for (int i=1; i<=bottomAxisLimitSec/60; i++) {
                    minorTicks.append(tick + lastValue);
                    lastValue += tick;
                }


                tick = 300; //5min
                lastValue = 0;
                majorTicks.append(0);
                for (int i=1; i<=bottomAxisLimitSec/300; i++) {
                    majorTicks.append(tick + lastValue);
                    lastValue += tick;
                }


                QwtScaleDiv myDiv(0, bottomAxisLimitSec, minorTicks, mediumTicks, majorTicks);
                setAxisScaleDiv(QwtPlot::xBottom, myDiv);
            }
            //-----------------------------------  <3:00 --------------------------
            else if (timeNow_sec < 10800)  {

                QList<double> minorTicks;
                QList<double> mediumTicks;
                QList<double> majorTicks;

                int tick = 60; //1min
                int lastValue = 0;
                for (int i=1; i<=bottomAxisLimitSec/60; i++) {
                    minorTicks.append(tick + lastValue);
                    lastValue += tick;
                }


                tick = 300; //5min
                lastValue = 0;
                for (int i=1; i<=bottomAxisLimitSec/300; i++) {
                    mediumTicks.append(tick + lastValue);
                    lastValue += tick;
                }

                tick = 600; //10min
                lastValue = 0;
                majorTicks.append(0);
                for (int i=1; i<=bottomAxisLimitSec/600; i++) {
                    majorTicks.append(tick + lastValue);
                    lastValue += tick;
                }


                QwtScaleDiv myDiv(0, bottomAxisLimitSec, minorTicks, mediumTicks, majorTicks);
                setAxisScaleDiv(QwtPlot::xBottom, myDiv);
            }
            //-----------------------------------  > 3:00 --------------------------
            else {

                QList<double> minorTicks;
                QList<double> mediumTicks;
                QList<double> majorTicks;

                int tick = 300; //5min
                int lastValue = 0;
                for (int i=1; i<=bottomAxisLimitSec/300; i++) {
                    mediumTicks.append(tick + lastValue);
                    lastValue += tick;
                }

                tick = 600; //10min
                lastValue = 0;
                majorTicks.append(0);
                for (int i=1; i<=bottomAxisLimitSec/600; i++) {
                    majorTicks.append(tick + lastValue);
                    lastValue += tick;
                }


                QwtScaleDiv myDiv(0, bottomAxisLimitSec, minorTicks, mediumTicks, majorTicks);
                setAxisScaleDiv(QwtPlot::xBottom, myDiv);
            }
        }
    }

    zoneDone->setInterval( 0, timeNow_sec );
    if (workout.getWorkoutNameEnum() == Workout::OPEN_RIDE)
        zoneToDo->setInterval( timeNow_sec, bottomAxisLimitSec );
    else
        zoneToDo->setInterval( timeNow_sec, Util::convertQTimeToSecD(workout.getDurationQTime()) );


    replot();


}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::showHideTargetPower(bool show) {
    foreach(QwtPlotShapeItem *item, lstTargetPower) {
        item->setVisible(show);
    }
    replot();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::showHideTargetCadence(bool show) {
    foreach(QwtPlotShapeItem *item, lstTargetCadence) {
        item->setVisible(show);
    }
    replot();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::showHideTargetHr(bool show) {
    foreach(QwtPlotShapeItem *item, lstTargetHr) {
        item->setVisible(show);
    }
    replot();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::showHideCurvePower(bool show) {
    powerCurve->setVisible(show);
    replot();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::showHideCurveCadence(bool show) {
    cadenceCurve->setVisible(show);
    replot();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::showHideCurveHeartRate(bool show) {
    hrCurve->setVisible(show);
    replot();
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::showHideCurveSpeed(bool show) {
    speedCurve->setVisible(show);
    replot();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::showHideGrid(bool show) {
    grid->setVisible(show);
    replot();
    qDebug() << "showHideGrid" << show;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::showHideSeperator(bool show) {
    for (int i=0; i<lstPlotMarkerSeperatorInterval.size(); i++) {
        lstPlotMarkerSeperatorInterval.at(i)->setVisible(show);
    }
    replot();
    qDebug() << "showHideSeperator" << show;
}






///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WorkoutPlot::addShapeFromPoints(const QString &title, const QColor &color, QList<QPointF> lstPoints, int positionZ, bool antiliasing) {

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

    if (title == "PARALLELOGRAM_POWER") {
        lstTargetPower.append(item);
    }
    else if (title == "PARALLELOGRAM_CADENCE") {
        lstTargetCadence.append(item);
    }
    else if (title == "PARALLELOGRAM_HR") {
        lstTargetHr.append(item);
    }
}
