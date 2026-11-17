#ifndef MYCREATORPLOT_H
#define MYCREATORPLOT_H

#include "qwt_plot.h"
#include "qwt_plot_grid.h"
#include "qwt_plot_curve.h"
#include "qwt_plot_histogram.h"
#include "qwt_plot_marker.h"
#include "qwt_plot_textlabel.h"
#include "qwt_plot_shapeitem.h"


#include "myqwtpickermachine.h"
#include "myqwtplotpicker.h"
#include "workout.h"
#include "shapefactory.h"
#include "zoneitem.h"
#include "account.h"





class QwtPlotPicker;


class myCreatorPlot : public QwtPlot
{
    Q_OBJECT

public:
    explicit myCreatorPlot(QWidget *parent = nullptr);
    ~myCreatorPlot() override;

    QString getSavePathExport();
    void savePathExport(QString path);



signals:
    void rightClickedGraph(QPointF);
    void shapeClicked(QString sourceRowIdentifier);

public slots:
    void pointClicked(QPointF);
    QwtPlotShapeItem* itemAt(const QPointF pos);

    void updateWorkout(Workout workout);
    void updateBackgroundHighlight(QSet<QString> setIntervalToHightlight);
    void removeHightlight();

private :
    void drawGraphIntervals();
    void addShapeFromPoints(const QString &title, const QColor &color, QList<QPointF> lstPoints,
                            int positionZ, bool antiliasing, bool isBackgroundItem);
    void init();
    void ajustScales();


private :
    int max_power;

    Workout workout;
    Account *account;


    QSet<QString> setSelectedInterval;

    MyQwtPlotPicker *d_picker;
    MyQwtPickerMachine *pickerMachine;

    QList<QwtPlotShapeItem*> lstBackgroundShape;



};

#endif // MYCREATORPLOT_H
