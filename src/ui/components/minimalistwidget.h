#ifndef MINIMALISTWIDGET_H
#define MINIMALISTWIDGET_H

#include <QWidget>
#include "util.h"

namespace Ui {
class MinimalistWidget;
}

class MinimalistWidget : public QWidget
{
    Q_OBJECT

public:
    enum TypeMinimalist
    {
        POWER,
        HEART_RATE,
        CADENCE,
        SPEED,
    };


    explicit MinimalistWidget(QWidget *parent = 0);
    ~MinimalistWidget();

    void setTypeWidget(TypeMinimalist type);
    void setUserData(double FTP, double LTHR);
    void setStopped(bool b);




public slots:
    void setTarget(double percentageTarget, int range);
    void setValue(int value);






private:
    // Applies the frame background only when the colour changes (setValue runs
    // per sensor packet; setStyleSheet forces a full style recompute).
    void applyFrameColor(Util::Color colorId);

    Ui::MinimalistWidget *ui;

    TypeMinimalist type;
    double FTP;
    double LTHR;
    QColor colorSquare;

    int low;
    int high;
    int extraAfterRange;

    int target;
    int range;

    bool isStopped;
    int currentColor = -1;
};

#endif // MINIMALISTWIDGET_H
