#include "sensorsecondaccumulator.h"

SensorSecondAccumulator::SensorSecondAccumulator(int riderCount)
    : m_riders(riderCount), m_means(riderCount)
{
}

SensorSecondAccumulator::Mean &SensorSecondAccumulator::mean(int riderIndex, Metric metric)
{
    RiderMeans &means = m_means[riderIndex];
    switch (metric) {
    case Metric::HeartRate: return means.heartRate;
    case Metric::Cadence: return means.cadence;
    case Metric::Speed: return means.speed;
    case Metric::Power: break;
    }
    return means.power;
}

void SensorSecondAccumulator::addSample(int riderIndex, Metric metric, double value)
{
    Mean &running = mean(riderIndex, metric);
    if (running.samples == 0) {
        running.value = value;
    } else {
        const int samples = running.samples;
        running.value = running.value * (double(samples) / (samples + 1)) + value / (samples + 1);
    }
    ++running.samples;

    RiderSecond &rider = m_riders[riderIndex];
    switch (metric) {
    case Metric::HeartRate: rider.heartRate = running.value; break;
    case Metric::Cadence: rider.cadence = running.value; break;
    case Metric::Speed: rider.speed = running.value; break;
    case Metric::Power: rider.power = running.value; break;
    }
}

void SensorSecondAccumulator::setRightPedalPercent(int riderIndex, double percent)
{
    m_riders[riderIndex].rightPedalPercent = percent;
}

void SensorSecondAccumulator::setHemoglobin(int riderIndex, double saturatedPercent, double totalConcentration)
{
    m_riders[riderIndex].saturatedHemoglobinPercent = saturatedPercent;
    m_riders[riderIndex].totalHemoglobinConcentration = totalConcentration;
}

void SensorSecondAccumulator::reset()
{
    m_riders.fill(RiderSecond());
    m_means.fill(RiderMeans());
}
