#include "ridesummary.h"

#include <cmath>

#include <QDir>
#include <QFileInfo>
#include <QtGlobal>

#include "fitactivityreader.h"
#include "powerzones.h"

int RideSummary::zoneTotalSeconds() const
{
    int total = 0;
    for (const int seconds : zoneSeconds)
        total += seconds;
    return total;
}

RideSummary RideSummary::build(const WorkoutHistorySummary &summary, const WorkoutHistoryDetail &detail,
                               int ftpWatts, const QVector<int> &zoneBoundsPct)
{
    RideSummary ride;
    const QVector<int> bounds = zoneBoundsPct.isEmpty() ? PowerZones::boundsPct({}) : zoneBoundsPct;
    const double ftp = qMax(1, ftpWatts);
    ride.zoneSeconds.fill(0, bounds.size() + 1);
    ride.valid           = summary.valid;
    ride.durationSec     = summary.durationSec;
    ride.avgPowerW       = summary.avgPowerW;
    ride.normalizedPower = summary.normalizedPower;
    ride.tss             = summary.tss;
    ride.avgHrBpm        = summary.avgHrBpm;
    ride.avgCadence      = summary.avgCadence;
    ride.calories        = summary.calories;
    ride.distanceKm      = summary.totalDistanceKm;
    if (ftpWatts > 0 && summary.normalizedPower > 0)
        ride.intensityFactor = summary.normalizedPower / double(ftpWatts);

    // Each record counts for the time until the next one (1 s at the end);
    // gaps from a pause are capped so they do not land in a zone.
    constexpr int kMaxGapSec = 5;
    const auto &records = detail.records;
    for (int i = 0; i < records.size(); ++i) {
        const ActivityRecordPoint &point = records.at(i);
        ride.maxHrBpm = qMax(ride.maxHrBpm, point.heartRate);
        if (point.power < 0)
            continue;
        ride.hasPower = true;
        ride.maxPowerW = qMax(ride.maxPowerW, point.power);
        const int gap = i + 1 < records.size() ? records.at(i + 1).elapsedSec - point.elapsedSec : 1;
        ride.zoneSeconds[PowerZones::zoneForPct(point.power * 100.0 / ftp, bounds)] += qBound(0, gap, kMaxGapSec);
    }
    return ride;
}

QString RideComparison::previousRideFile(const QString &historyDir, const QString &currentFile,
                                         int currentDurationSec)
{
    const QStringList files = previousRideFiles(historyDir, currentFile, currentDurationSec, 1);
    return files.isEmpty() ? QString() : files.first();
}

QStringList RideComparison::previousRideFiles(const QString &historyDir, const QString &currentFile,
                                              int currentDurationSec, int count)
{
    const QString workoutName = FitActivityReader::workoutNameFromFileName(currentFile);
    const QString currentBase = QFileInfo(currentFile).fileName();
    // History file names start with the ride's date and time, so name order is time order.
    const QStringList files = QDir(historyDir).entryList({QStringLiteral("*.fit")}, QDir::Files,
                                                         QDir::Name | QDir::Reversed);
    const int maxCandidates = count * 4 + 6;
    int checked = 0;
    QStringList result;
    for (const QString &file : files) {
        if (file >= currentBase || FitActivityReader::workoutNameFromFileName(file) != workoutName)
            continue;
        const QString path = QDir(historyDir).absoluteFilePath(file);
        const WorkoutHistorySummary previous = FitActivityReader::readFile(path);
        if (previous.valid && previous.durationSec * 5 >= currentDurationSec * 4) {
            result.append(path);
            if (result.size() >= count)
                break;
        }
        if (++checked >= maxCandidates)
            break;
    }
    return result;
}

int RideComparison::impliedFtp(const WorkoutHistorySummary &ride)
{
    if (ride.normalizedPower <= 0 || ride.tss <= 0.0 || ride.durationSec <= 0)
        return 0;
    return qRound(ride.normalizedPower * std::sqrt(ride.durationSec / (36.0 * ride.tss)));
}

double RideComparison::efficiency(const WorkoutHistorySummary &ride)
{
    const int power = ride.normalizedPower > 0 ? ride.normalizedPower : ride.avgPowerW;
    return power > 0 && ride.avgHrBpm > 0 ? double(power) / ride.avgHrBpm : 0.0;
}

RideComparison RideComparison::compare(const WorkoutHistorySummary &current,
                                       const WorkoutHistorySummary &previous)
{
    RideComparison comparison;
    if (!current.valid || !previous.valid)
        return comparison;
    comparison.valid         = true;
    comparison.previousStart = previous.startTime;
    comparison.hasPower      = current.normalizedPower > 0 && previous.normalizedPower > 0;
    comparison.hasHeartRate  = current.avgHrBpm > 0 && previous.avgHrBpm > 0;
    comparison.npDeltaW       = current.normalizedPower - previous.normalizedPower;
    comparison.avgPowerDeltaW = current.avgPowerW - previous.avgPowerW;
    comparison.hrDeltaBpm     = current.avgHrBpm - previous.avgHrBpm;
    comparison.tssDelta       = current.tss - previous.tss;
    const double currentEfficiency = efficiency(current);
    const double previousEfficiency = efficiency(previous);
    comparison.hasEfficiency   = currentEfficiency > 0.0 && previousEfficiency > 0.0;
    comparison.efficiencyDelta = currentEfficiency - previousEfficiency;
    comparison.currentFtpW     = impliedFtp(current);
    comparison.previousFtpW    = impliedFtp(previous);
    return comparison;
}

RideTrend RideTrend::build(const WorkoutHistorySummary &current,
                           const QList<WorkoutHistorySummary> &previousNewestFirst)
{
    RideTrend trend;
    if (!current.valid || current.avgHrBpm <= 0)
        return trend;
    for (auto it = previousNewestFirst.crbegin(); it != previousNewestFirst.crend(); ++it) {
        if (!it->valid || it->avgHrBpm <= 0)
            continue;
        trend.rides.append({it->startTime.date(), it->avgHrBpm, RideComparison::efficiency(*it), false});
    }
    trend.rides.append({current.startTime.date(), current.avgHrBpm, RideComparison::efficiency(current), true});
    trend.valid = trend.rides.size() >= 2;
    if (!trend.valid)
        return trend;

    const Ride &today = trend.rides.last();
    trend.bestIndex = int(trend.rides.size()) - 1;
    trend.currentHrRank = 1;
    trend.currentEfficiencyRank = 1;
    for (int index = 0; index + 1 < trend.rides.size(); ++index) {
        const Ride &ride = trend.rides.at(index);
        if (ride.avgHrBpm < trend.rides.at(trend.bestIndex).avgHrBpm)
            trend.bestIndex = index;
        if (ride.avgHrBpm < today.avgHrBpm)
            ++trend.currentHrRank;
        if (ride.efficiency > today.efficiency)
            ++trend.currentEfficiencyRank;
    }
    trend.currentPowerW = current.avgPowerW;
    return trend;
}
