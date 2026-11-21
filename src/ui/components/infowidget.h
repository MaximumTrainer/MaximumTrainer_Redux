#ifndef INFOWIDGET_H
#define INFOWIDGET_H

#include <QWidget>
class QLabel;
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

    explicit InfoWidget(QWidget *parent = nullptr);
    ~InfoWidget() override;
    void setTypeInfoBox(TypeInfoBox type);
    /// Rowing (Beta): CADENCE boxes read as Stroke Rate (spm) and SPEED boxes
    /// format their km/h input as Pace per 500 m (m:ss).
    void setRowingMode(bool rowing);
    void setUserData(double FTP, double LTHR);


    void setStopped(bool b);
    void setUseMiles(bool b);

    void setTrainerSpeedVisible(bool b);
    /// Shows or hides the Avg / Max rows (interval and workout); without them
    /// the box is just its title and current value, and can be much shorter.
    void setStatsVisible(bool visible);


signals:
    /// -1 below target, 0 on target, 1 above, 2 no target or stopped.
    void targetStateChanged(int state);

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

    TypeInfoBox type = POWER;
    bool rowingMode = false;
    QLabel *m_caption    = nullptr;   ///< "POWER", "CADENCE", ... in the title row
    QLabel *m_targetPill = nullptr;   ///< "Target 143 W ±10"
    QLabel *m_zonePill = nullptr;     ///< power card: "Z3 Tempo" in the zone colour
    void updateZonePill(double targetFraction);
    QLabel *m_unit       = nullptr;   ///< unit next to the value
    void refreshUnit();
    QString unitText() const;
    bool useMile;

    int target;
    int targetRange;


    bool isStopped;
    int currentColor;  // -1 below, 0 on target, 1 above, 2 no target

};

#endif // INFOWIDGET_H
