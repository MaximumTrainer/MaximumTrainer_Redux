#include "powerbests.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStandardPaths>

#include "mmpcalculator.h"

namespace PowerBests {

namespace {

// Bump when the cached values change meaning, so stale entries are recomputed.
constexpr int kCacheVersion = 1;

QMutex s_cacheMutex;
QString s_cachePathOverride;

QString cacheKey(const QFileInfo &info)
{
    return QStringLiteral("%1|%2|%3").arg(info.fileName()).arg(info.size())
        .arg(info.lastModified().toMSecsSinceEpoch());
}

QJsonObject readCache()
{
    QFile file(cacheFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root.value(QStringLiteral("version")).toInt() != kCacheVersion)
        return {};
    return root.value(QStringLiteral("rides")).toObject();
}

void writeCache(const QJsonObject &rides)
{
    const QString path = cacheFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    QJsonObject root;
    root.insert(QStringLiteral("version"), kCacheVersion);
    root.insert(QStringLiteral("rides"), rides);
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit();
}

} // namespace

QVector<double> bestAverages(const QVector<int> &power, const QVector<int> &durations)
{
    QVector<double> best(durations.size(), 0.0);
    const qsizetype samples = power.size();
    for (qsizetype index = 0; index < durations.size(); ++index) {
        const int duration = durations[index];
        if (duration <= 0 || duration > samples)
            continue;
        long long windowSum = 0;
        for (int i = 0; i < duration; ++i)
            windowSum += power[i];
        long long bestSum = windowSum;
        for (qsizetype i = duration; i < samples; ++i) {
            windowSum += power[i] - power[i - duration];
            bestSum = std::max(bestSum, windowSum);
        }
        best[index] = static_cast<double>(bestSum) / duration;
    }
    return best;
}

QVector<Effort> bestEfforts(const QList<Ride> &rides, const QDateTime &since)
{
    QVector<Effort> efforts(MMP_DURATIONS.size());
    for (const Ride &ride : rides) {
        if (since.isValid() && (!ride.summary.startTime.isValid() || ride.summary.startTime < since))
            continue;
        for (qsizetype index = 0; index < efforts.size() && index < ride.bests.size(); ++index) {
            if (ride.bests[index] > efforts[index].watts) {
                efforts[index].watts = ride.bests[index];
                efforts[index].date = ride.summary.startTime;
                efforts[index].workoutName = ride.summary.workoutName;
                efforts[index].filePath = ride.summary.filePath;
            }
        }
    }
    return efforts;
}

QList<Ride> loadRides(const QList<WorkoutHistorySummary> &history)
{
    QMutexLocker lock(&s_cacheMutex);
    const QJsonObject cached = readCache();
    QJsonObject updated;
    QList<Ride> rides;
    bool changed = false;

    for (const WorkoutHistorySummary &summary : history) {
        if (summary.filePath.isEmpty())
            continue;
        const QFileInfo info(summary.filePath);
        if (!info.exists())
            continue;
        const QString key = cacheKey(info);

        Ride ride{summary, {}};
        const QJsonArray values = cached.value(key).toArray();
        if (values.size() == MMP_DURATIONS.size()) {
            for (const auto &value : values)
                ride.bests.append(value.toDouble());
        } else {
            ride.bests = bestAverages(MmpCalculator::readPowerSeries(summary.filePath), MMP_DURATIONS);
            for (double &watts : ride.bests)
                watts = qRound(watts * 10.0) / 10.0;
            changed = true;
        }

        QJsonArray stored;
        for (double watts : ride.bests)
            stored.append(watts);
        updated.insert(key, stored);
        rides.append(ride);
    }

    // Also drops entries for deleted or rewritten rides.
    if (changed || updated.size() != cached.size())
        writeCache(updated);
    return rides;
}

QString cacheFilePath()
{
    if (!s_cachePathOverride.isEmpty())
        return s_cachePathOverride;
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/power-bests-cache.json");
}

void setCacheFilePathForTesting(const QString &path)
{
    s_cachePathOverride = path;
}

} // namespace PowerBests
