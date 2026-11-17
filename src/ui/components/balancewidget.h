#ifndef BALANCEWIDGET_H
#define BALANCEWIDGET_H

#include <QWidget>

namespace Ui {
class BalanceWidget;
}

class BalanceWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BalanceWidget(QWidget *parent = nullptr);
    ~BalanceWidget() override;

    void setZone(int target, int range);
    void removeZone();
    void setValue(int value);

    void setStopped(bool b);
    void setShowMode(int display);


public slots:
    void pedalMetricChanged(double leftTorqueEff, double rightTorqueEff,
                            double leftPedalSmooth, double rightPedalSmooth, double combienedPedalSmooth);


private:
    Ui::BalanceWidget *ui;


    int zoneTarget;
    int zoneRange;

    bool isStopped;

    //// 0 = Always, 1 = Target, 2 = Never
    int showMode;


};

#endif // BALANCEWIDGET_H
