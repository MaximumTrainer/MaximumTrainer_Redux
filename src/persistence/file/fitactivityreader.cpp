#include "fitactivityreader.h"

#include <fstream>

#include <QFileInfo>

#include "fit_decode.hpp"
#include "fit_mesg_broadcaster.hpp"
#include "fit_runtime_exception.hpp"

// Garmin FIT epoch is Dec 31 1989 00:00:00 UTC.
// Unix epoch is Jan 1 1970 00:00:00 UTC.
// Delta = 631065600 seconds.
static constexpr qint64 FIT_EPOCH_OFFSET = 631065600LL;

WorkoutHistorySummary FitActivityReader::readFile(const QString &filePath)
{
    FitActivityReader reader;
    reader.m_summary.filePath = filePath;

    std::ifstream stream;
#ifdef Q_OS_WIN32
    stream.open(filePath.toStdWString(), std::ios::in | std::ios::binary);
#else
    stream.open(filePath.toStdString(), std::ios::in | std::ios::binary);
#endif

    if (!stream.is_open())
        return reader.m_summary;

    fit::Decode       decoder;
    fit::MesgBroadcaster broadcaster;
    broadcaster.AddListener(static_cast<fit::SessionMesgListener &>(reader));

    try {
        decoder.Read(stream, broadcaster, broadcaster);
    } catch (const fit::RuntimeException &) {
        // partial decode — return whatever we extracted
    } catch (...) {
        qWarning() << "[FitActivityReader] Unexpected exception decoding" << filePath;
    }

    // Derive workout name from filename if not populated from session data
    if (reader.m_summary.workoutName.isEmpty()) {
        QString base = QFileInfo(filePath).completeBaseName();
        // Filename format: yyyy-MM-dd(hh-mm-ss)-MT-username-WorkoutName
        int idx = base.indexOf(QLatin1String("-MT-"));
        if (idx >= 0) {
            QString rest = base.mid(idx + 4); // skip "-MT-"
            int dash = rest.indexOf(QLatin1Char('-'));
            if (dash >= 0)
                reader.m_summary.workoutName = rest.mid(dash + 1);
            else
                reader.m_summary.workoutName = rest;
        } else {
            reader.m_summary.workoutName = base;
        }
    }

    return reader.m_summary;
}

void FitActivityReader::OnMesg(fit::SessionMesg &mesg)
{
    m_summary.valid = true;

    if (mesg.IsStartTimeValid()) {
        qint64 unixSec = static_cast<qint64>(mesg.GetStartTime()) + FIT_EPOCH_OFFSET;
        m_summary.startTime = QDateTime::fromSecsSinceEpoch(unixSec, Qt::UTC).toLocalTime();
    }

    if (mesg.IsTotalTimerTimeValid())
        m_summary.durationSec = static_cast<int>(mesg.GetTotalTimerTime());

    if (mesg.IsAvgPowerValid())
        m_summary.avgPowerW = static_cast<int>(mesg.GetAvgPower());

    if (mesg.IsNormalizedPowerValid())
        m_summary.normalizedPower = static_cast<int>(mesg.GetNormalizedPower());

    if (mesg.IsAvgHeartRateValid())
        m_summary.avgHrBpm = static_cast<int>(mesg.GetAvgHeartRate());

    if (mesg.IsAvgCadenceValid())
        m_summary.avgCadence = static_cast<int>(mesg.GetAvgCadence());

    if (mesg.IsTrainingStressScoreValid())
        m_summary.tss = static_cast<double>(mesg.GetTrainingStressScore());

    if (mesg.IsTotalDistanceValid())
        m_summary.totalDistanceKm = static_cast<double>(mesg.GetTotalDistance()) / 1000.0;

    if (mesg.IsTotalCaloriesValid())
        m_summary.calories = static_cast<int>(mesg.GetTotalCalories());
}
