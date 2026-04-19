#ifndef WORKOUTPLOT_H
#define WORKOUTPLOT_H

#include "qwt_plot.h"
#include "qwt_plot_grid.h"
#include "qwt_plot_curve.h"
#include "qwt_plot_histogram.h"
#include "qwt_plot_marker.h"
#include "qwt_plot_textlabel.h"
#include "qwt_plot_shapeitem.h"

#include "settings.h"
#include "account.h"
#include "zoneitem.h"
#include "shapefactory.h"
#include "workout.h"
#include "faderlabel.h"

#include "myqwtplotpicker.h"
#include "myqwtpickermachine.h"

#include <QSpinBox>





class QwtPlotCurve;
class QwtPlotDirectPainter;


class WorkoutPlot : public QwtPlot
{
    Q_OBJECT

public:

    ~WorkoutPlot();
    WorkoutPlot(QWidget *parent = 0);
    void setWorkoutData(Workout workout, bool firstInit);
    void setUserData(double FTP, double LTHR);



    void showHideTargetPower(bool show);
    void showHideTargetCadence(bool show);
    void showHideTargetHr(bool show);
    void showHideCurvePower(bool show);
    void showHideCurveCadence(bool show);
    void showHideCurveHeartRate(bool show);
    void showHideCurveSpeed(bool show);

    void showHideGrid(bool show);
    void showHideSeperator(bool show);


    void setMessageEndWorkout();
    void setMessage(QString msg);
    void setDisplayIntervalMessage(bool fadeIn, QString text, int timeToDisplay);
    void setAlertMessage(bool fadeIn, bool fadeOut, QString text, int timeToDisplay);
    void removeAlertMessage();
    void removeMainMessage();


    void setStopped(bool stopped);
    void setSpinBoxDisabled();

    void addMarkerInterval(double time);

    bool eventFilter(QObject *watched, QEvent *event);



signals:
    void workoutDifficultyChanged(int percentageIncrease);
    void intervalClicked(int positionInterval, double secondClicked, double startIntervalSec, bool showConfirmation);


    /////////////////////////////////////////////////////////////////////////////////////////////
public slots:
    void updateMarkerTimeNow(double sec);
    void pointClicked(QPointF);

    void increaseDifficulty();
    void decreaseDifficulty();




    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
private :
    void init(bool firstInit);
    void clearPlot();
    void updateAxisHelper();
    void drawGraphIntervals();
    void addShapeFromPoints(const QString &title, const QColor &color, QList<QPointF> lstPoints, int positionZ, bool antiliasing);



    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
private:
    int max_power;

    bool isStopped;
    int bottomAxisLimitSec;

    Workout workout;
    double FTP;
    double LTHR;


    MyQwtPlotPicker *d_picker;
    MyQwtPickerMachine *pickerMachine;
    QSpinBox *spinBoxDifficulty;

    QwtPlotGrid *grid;


    QList<QwtPlotMarker*> lstPlotMarket;
    QList<QwtPlotMarker*> lstPlotMarkerSeperatorInterval;

    QList<QwtPlotShapeItem*> lstTargetPower;
    QList<QwtPlotShapeItem*> lstTargetCadence;
    QList<QwtPlotShapeItem*> lstTargetHr;
    ZoneItem* zoneDone;
    ZoneItem* zoneToDo;


    /// Display Message
    FaderLabel *labelMsg;
    FaderLabel *labelMsgInterval;
    FaderLabel *labelAlertMessage;
    QWidget *widgetCanvas;


    QwtPlotCurve *hrCurve;
    QwtPlotCurve *powerCurve;
    QwtPlotCurve *cadenceCurve;
    QwtPlotCurve *speedCurve;

    bool showingMessage;


    //to ignore clicked signal emits 2 times in a row
    double lastXClicked;



};

#endif // WORKOUTPLOT_H
