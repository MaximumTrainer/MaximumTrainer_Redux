#ifndef INFOWIDGET_H
#define INFOWIDGET_H

#include <QWidget>
#include <QLocale>

namespace Ui {
class InfoWidget;
}

class InfoWidget : public QWidget
{
    Q_OBJECT

public:

    enum TypeInfoBox
    {
        POWER,
        HEART_RATE,
        CADENCE,
        SPEED,
        //        SPEED_MPH,
    };

    explicit InfoWidget(QWidget *parent = 0);
    ~InfoWidget();
    void setTypeInfoBox(TypeInfoBox type);
    /// Rowing (Beta): CADENCE boxes read as Stroke Rate (spm) and SPEED boxes
    /// format their km/h input as Pace per 500 m (m:ss).
    void setRowingMode(bool rowing);
    void setUserData(double FTP, double LTHR);


    void setStopped(bool b);
    void setUseMiles(bool b);

    void setTrainerSpeedVisible(bool b);






    /////////////////////////////////////////////////////////////////////////////////////////////
public slots:

    void setValue( int v );
    void setValue( double v );
    void setTrainerSpeed(double v); //for trainer speed
    void targetChanged(double percentageValue, int range);
    void maxIntervalChanged(double avg);
    void maxWorkoutChanged(double avg);
    void avgIntervalChanged(double avg);
    void avgWorkoutChanged(double avg);


private:
    Ui::InfoWidget *ui;
    QLocale locale;
    double FTP;
    double LTHR;

    TypeInfoBox type;
    bool rowingMode = false;
    bool useMile;

    int target;
    int targetRange;


    bool isStopped;
    int currentColor;  // 0 = Okay, -1= TooLow, 1=TooHigh

};

#endif // INFOWIDGET_H
