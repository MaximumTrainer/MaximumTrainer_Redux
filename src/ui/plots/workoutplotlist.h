#ifndef WORKOUTPLOTLIST_H
#define WORKOUTPLOTLIST_H

#include "qwt_plot.h"
#include "qwt_plot_grid.h"
#include "workout.h"
#include "qwt_plot_curve.h"
#include "qwt_plot_histogram.h"
#include "shapefactory.h"
#include "qwt_plot_marker.h"
#include "account.h"


class QwtPlotCurve;
class QwtPlotDirectPainter;


class WorkoutPlotList : public QwtPlot
{
    Q_OBJECT

public:


    WorkoutPlotList(QWidget *parent = nullptr);
    void setWorkoutData(Workout workout);

private :
    void init();
    void drawGraphIntervals();
    void addShapeFromPoints(const QString &title, const QColor &color, QList<QPointF> lstPoints, int positionZ, bool antiliasing);


private:
    int max_power;

    Workout workout;
    Account *account;


    QwtPlotCurve *powerTargetCurve;
    QVector<QPointF> powerTargetSamples;


};

#endif // WORKOUTPLOTLIST_H
