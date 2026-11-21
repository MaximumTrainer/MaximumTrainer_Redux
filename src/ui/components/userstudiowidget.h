#ifndef USERSTUDIOWIDGET_H
#define USERSTUDIOWIDGET_H

#include <QFrame>
#include <QMap>

#include "sensorstatusstrip.h"

class QLabel;
class QHBoxLayout;
class QWidget;
class FaderLabel;

/// One Studio rider's card in the workout window: name, FTP and a dot per
/// sensor in the header; power (with the rider's own target and zone),
/// cadence and heart rate below; a status line and a coloured left edge when
/// the rider is off target or a sensor is lost.
class UserStudioWidget : public QFrame
{
    Q_OBJECT

public:
    explicit UserStudioWidget(int userID, QString displayName, double FTP, double LTHR, QWidget *parent = nullptr);

    void setStopped(bool b);

    void setPowerSectionHidden();
    void setCadSectionHidden();
    void setHrSectionHidden();

    void setResultTest(QString text);

    /// A sensor being connected for this rider (one dot in the header).
    void addSensor(int key, const QString &name);
    void setSensorState(int key, SensorStatusStrip::State state);

    static constexpr int kCardHeight = 112;

signals:
    void sensorRetryRequested(int key);

public slots:
    void setPowerValue(int value);
    void setCadenceValue(int value);
    void setHrValue(int value);

    void setTargetPower(double percentageFTP, int range);
    void setTargetCadence(int value, int range);
    void setTargetHr(double percentageLTHR, int range);

    void setMaxIntervalPower(double val);
    void setMaxWorkoutPower(double val);
    void setAvgIntervalPower(double val);
    void setAvgWorkoutPower(double val);

    void setMaxIntervalCad(double val);
    void setMaxWorkoutCad(double val);
    void setAvgIntervalCad(double val);
    void setAvgWorkoutCad(double val);

    void setMaxIntervalHr(double val);
    void setMaxWorkoutHr(double val);
    void setAvgIntervalHr(double val);
    void setAvgWorkoutHr(double val);

    void setNormalizedPower(double normalizedPower);
    void setIntensityFactor(double intensityFactor);
    void setTrainingStressScore(double trainingStressScore);
    void setCalories(double calories);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Target { None, Below, On, Above };

    QWidget *metricBlock(const QString &caption, QLabel *&value, const QString &unit);
    void updatePowerState();
    void updateStatus();
    void setEdge(const QString &state);
    void updateRideLine();
    void updateTooltips();

    int     userID;
    QString displayName;
    double  FTP;
    double  LTHR;
    bool    isStopped = true;

    int powerValue = -1;
    int targetPower = -1;
    int targetPowerRange = 0;
    int targetCad = -1;
    int targetCadRange = 0;
    int targetHr = -1;
    int targetHrRange = 0;
    double targetPowerPct = -1.0;
    Target powerState = Target::None;

    double avgIntervalPower = 0, maxIntervalPower = 0, avgWorkoutPower = 0, maxWorkoutPower = 0;
    double avgIntervalCad = 0, maxIntervalCad = 0, avgWorkoutCad = 0, maxWorkoutCad = 0;
    double avgIntervalHr = 0, maxIntervalHr = 0, avgWorkoutHr = 0, maxWorkoutHr = 0;
    double normalizedPower = 0, intensityFactor = 0, tss = 0, kcal = 0;

    QLabel      *m_ride = nullptr;
    QHBoxLayout *m_dots = nullptr;
    QLabel      *m_power = nullptr;
    QLabel      *m_powerUnit = nullptr;
    QLabel      *m_target = nullptr;
    QLabel      *m_zone = nullptr;
    QWidget     *m_powerBlock = nullptr;
    QWidget     *m_cadBlock = nullptr;
    QWidget     *m_hrBlock = nullptr;
    QLabel      *m_cad = nullptr;
    QLabel      *m_hr = nullptr;
    QLabel      *m_status = nullptr;
    FaderLabel  *m_resultTest = nullptr;

    QMap<int, QLabel*>                       m_sensorDots;
    QMap<int, QString>                       m_sensorNames;
    QMap<int, SensorStatusStrip::State>      m_sensorStates;
};

#endif // USERSTUDIOWIDGET_H
