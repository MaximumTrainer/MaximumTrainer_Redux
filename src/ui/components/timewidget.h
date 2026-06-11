#ifndef TIMEWIDGET_H
#define TIMEWIDGET_H

#include <QWidget>

namespace Ui {
class TimeWidget;
}

class TimeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimeWidget(QWidget *parent = 0);
    ~TimeWidget();

    void showIntervalRemaining(bool);
    void showWorkoutRemaining(bool);
    void showWorkoutElapsed(bool);
    void showCurrentTarget(bool);


    void setFreeRideMode();
    void setMAPMode();
    void setIntervalTime(QTime time);
    void setWorkoutRemainingTime(QTime time);
    void setWorkoutTime(QTime time);


    void setTargetPower(double percentageFTP, int range);
    void setTargetCadence(int cadence, int range);
    void setTargetHeartRate(double percentageLTHR, int range);





private:
    Ui::TimeWidget *ui;

    bool hasTargetPower;
    bool hasTargetCad;
    bool hasTargetHr;
    bool showTargetEnabled = true;


};

#endif // TIMEWIDGET_H
