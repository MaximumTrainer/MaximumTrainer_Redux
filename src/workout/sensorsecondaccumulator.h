#ifndef SENSORSECONDACCUMULATOR_H
#define SENSORSECONDACCUMULATOR_H

#include <QVector>

/// One rider's sensor values for the current second; -1 means "no data".
struct RiderSecond {
    double heartRate = -1;
    double cadence = -1;
    double speed = -1;
    double power = -1;
    double rightPedalPercent = -1;
    double saturatedHemoglobinPercent = -1;
    double totalHemoglobinConcentration = -1;
};

/// Collects each rider's sensor samples between two workout seconds. Heart
/// rate, cadence, speed and power are averaged over the second; pedal balance
/// and muscle oxygen keep their latest value. reset() starts the next second.
class SensorSecondAccumulator
{
public:
    enum class Metric { HeartRate, Cadence, Speed, Power };

    explicit SensorSecondAccumulator(int riderCount);

    void addSample(int riderIndex, Metric metric, double value);
    void setRightPedalPercent(int riderIndex, double percent);
    void setHemoglobin(int riderIndex, double saturatedPercent, double totalConcentration);

    const RiderSecond &rider(int riderIndex) const { return m_riders.at(riderIndex); }
    void reset();

private:
    struct Mean {
        double value = -1;
        int samples = 0;
    };
    struct RiderMeans {
        Mean heartRate, cadence, speed, power;
    };

    Mean &mean(int riderIndex, Metric metric);

    QVector<RiderSecond> m_riders;
    QVector<RiderMeans> m_means;
};

#endif // SENSORSECONDACCUMULATOR_H
