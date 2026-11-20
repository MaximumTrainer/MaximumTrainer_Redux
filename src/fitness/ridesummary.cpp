#include "ridesummary.h"

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
    const QString workoutName = FitActivityReader::workoutNameFromFileName(currentFile);
    const QString currentBase = QFileInfo(currentFile).fileName();
    // History file names start with the ride's date and time, so name order is time order.
    QStringList files = QDir(historyDir).entryList({QStringLiteral("*.fit")}, QDir::Files, QDir::Name | QDir::Reversed);
    constexpr int kMaxCandidates = 10;
    int checked = 0;
    for (const QString &file : files) {
        if (file >= currentBase || FitActivityReader::workoutNameFromFileName(file) != workoutName)
            continue;
        const QString path = QDir(historyDir).absoluteFilePath(file);
        const WorkoutHistorySummary previous = FitActivityReader::readFile(path);
        if (previous.valid && previous.durationSec * 5 >= currentDurationSec * 4)
            return path;
        if (++checked >= kMaxCandidates)
            break;
    }
    return {};
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
    return comparison;
}
