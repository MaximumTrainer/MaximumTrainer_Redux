#ifndef METRICSTRIPMODEL_H
#define METRICSTRIPMODEL_H

#include <QObject>

/// Data bridge between WorkoutDialog's live sensor slots and the QML metric
/// dashboard (qml/MetricDashboard.qml).  Plain NOTIFY properties — QML
/// animates the transitions, so the setters just store and signal.
class MetricStripModel : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int power       READ power       NOTIFY powerChanged)
    Q_PROPERTY(int cadence     READ cadence     NOTIFY cadenceChanged)
    Q_PROPERTY(int hr          READ hr          NOTIFY hrChanged)
    Q_PROPERTY(double speed    READ speed       NOTIFY speedChanged)
    Q_PROPERTY(int targetPower READ targetPower NOTIFY targetPowerChanged)
    Q_PROPERTY(int targetRange READ targetRange NOTIFY targetPowerChanged)
    Q_PROPERTY(int avgPower    READ avgPower    NOTIFY statsChanged)
    Q_PROPERTY(int maxPower    READ maxPower    NOTIFY statsChanged)
    Q_PROPERTY(int avgHr       READ avgHr       NOTIFY statsChanged)
    Q_PROPERTY(int maxHr       READ maxHr       NOTIFY statsChanged)
    Q_PROPERTY(int avgCadence  READ avgCadence  NOTIFY statsChanged)
    Q_PROPERTY(int maxCadence  READ maxCadence  NOTIFY statsChanged)
    Q_PROPERTY(bool hasSpeed   READ hasSpeed    NOTIFY speedChanged)
    Q_PROPERTY(double np       READ np          NOTIFY sessionChanged)
    Q_PROPERTY(double intensity READ intensity  NOTIFY sessionChanged)
    Q_PROPERTY(double tss      READ tss         NOTIFY sessionChanged)
    Q_PROPERTY(double kcal     READ kcal        NOTIFY sessionChanged)
    Q_PROPERTY(double distanceKm READ distanceKm NOTIFY sessionChanged)

public:
    explicit MetricStripModel(QObject *parent = nullptr) : QObject(parent) {}

    int    power()       const { return m_power; }
    int    cadence()     const { return m_cadence; }
    int    hr()          const { return m_hr; }
    double speed()       const { return m_speed; }
    int    targetPower() const { return m_targetPower; }
    int    targetRange() const { return m_targetRange; }
    int    avgPower()    const { return m_avgPower; }
    int    maxPower()    const { return m_maxPower; }
    int    avgHr()       const { return m_avgHr; }
    int    maxHr()       const { return m_maxHr; }
    int    avgCadence()  const { return m_avgCadence; }
    int    maxCadence()  const { return m_maxCadence; }
    bool   hasSpeed()    const { return m_speed > 0.05; }
    double np()          const { return m_np; }
    double intensity()   const { return m_if; }
    double tss()         const { return m_tss; }
    double kcal()        const { return m_kcal; }
    double distanceKm()  const { return m_distanceM / 1000.0; }

public slots:
    /// Reset the avg/max accumulators (called when the workout actually starts).
    void resetStats() {
        m_powerSum = m_hrSum = m_cadSum = 0;
        m_powerN = m_hrN = m_cadN = 0;
        m_avgPower = m_maxPower = m_avgHr = m_maxHr = m_avgCadence = m_maxCadence = 0;
        m_accumulate = true;
        emit statsChanged();
    }
    void setAccumulating(bool on) { m_accumulate = on; }

    void setPower(int w) {
        if (w != m_power) { m_power = w; emit powerChanged(); }
        if (m_accumulate && w > 0) {
            m_powerSum += w; ++m_powerN;
            m_avgPower = int(m_powerSum / m_powerN);
            if (w > m_maxPower) m_maxPower = w;
            emit statsChanged();
        }
    }
    void setCadence(int rpm) {
        if (rpm != m_cadence) { m_cadence = rpm; emit cadenceChanged(); }
        if (m_accumulate && rpm > 0) {
            m_cadSum += rpm; ++m_cadN;
            m_avgCadence = int(m_cadSum / m_cadN);
            if (rpm > m_maxCadence) m_maxCadence = rpm;
            emit statsChanged();
        }
    }
    void setHr(int bpm) {
        if (bpm != m_hr) { m_hr = bpm; emit hrChanged(); }
        if (m_accumulate && bpm > 0) {
            m_hrSum += bpm; ++m_hrN;
            m_avgHr = int(m_hrSum / m_hrN);
            if (bpm > m_maxHr) m_maxHr = bpm;
            emit statsChanged();
        }
    }
    void setSpeed(double kmh)   { m_speed = kmh; emit speedChanged(); }

    // Session metrics — same signal shapes as the classic infosWorkout slots
    // so they connect straight to DataWorkout's signals.
    void NP_Changed(double v)        { m_np = v;        emit sessionChanged(); }
    void IF_Changed(double v)        { m_if = v;        emit sessionChanged(); }
    void TSS_Changed(double v)       { m_tss = v;       emit sessionChanged(); }
    void calories_Changed(double v)  { m_kcal = v;      emit sessionChanged(); }
    void distanceChanged(double m)   { m_distanceM = m; emit sessionChanged(); }
    void setTargetPower(int watts, int range) {
        if (watts != m_targetPower || range != m_targetRange) {
            m_targetPower = watts; m_targetRange = range; emit targetPowerChanged();
        }
    }

signals:
    void powerChanged();
    void cadenceChanged();
    void hrChanged();
    void speedChanged();
    void targetPowerChanged();
    void statsChanged();
    void sessionChanged();

private:
    int    m_power = 0,  m_cadence = 0, m_hr = 0;
    double m_speed = 0.0;
    int    m_targetPower = 0, m_targetRange = 0;
    int    m_avgPower = 0, m_maxPower = 0;
    int    m_avgHr = 0,    m_maxHr = 0;
    int    m_avgCadence = 0, m_maxCadence = 0;

    double m_np = 0, m_if = 0, m_tss = 0, m_kcal = 0, m_distanceM = 0;
    bool   m_accumulate = true;
    qint64 m_powerSum = 0, m_hrSum = 0, m_cadSum = 0;
    int    m_powerN = 0,   m_hrN = 0,   m_cadN = 0;
};

#endif // METRICSTRIPMODEL_H
