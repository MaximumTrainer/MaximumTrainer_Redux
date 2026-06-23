#include "fitactivitydetailreader.h"

#include <fstream>

#include <QDebug>

#include "fit_decode.hpp"
#include "fit_mesg_broadcaster.hpp"
#include "fit_runtime_exception.hpp"

WorkoutHistoryDetail FitActivityDetailReader::readFile(const QString &filePath)
{
    FitActivityDetailReader reader;

    std::ifstream stream;
#ifdef Q_OS_WIN32
    stream.open(filePath.toStdWString(), std::ios::in | std::ios::binary);
#else
    stream.open(filePath.toStdString(), std::ios::in | std::ios::binary);
#endif

    if (!stream.is_open())
        return reader.m_detail;

    fit::Decode          decoder;
    fit::MesgBroadcaster broadcaster;
    broadcaster.AddListener(static_cast<fit::RecordMesgListener &>(reader));
    broadcaster.AddListener(static_cast<fit::LapMesgListener &>(reader));

    try {
        decoder.Read(stream, broadcaster, broadcaster);
    } catch (const fit::RuntimeException &) {
        // partial decode — return whatever we extracted
    } catch (...) {
        qWarning() << "[FitActivityDetailReader] Unexpected exception decoding" << filePath;
    }

    reader.m_detail.valid = !reader.m_detail.records.isEmpty();
    return reader.m_detail;
}

void FitActivityDetailReader::OnMesg(fit::RecordMesg &mesg)
{
    ActivityRecordPoint p;

    if (mesg.IsTimestampValid()) {
        const qint64 ts = static_cast<qint64>(mesg.GetTimestamp());
        if (m_firstTimestamp < 0)
            m_firstTimestamp = ts;
        p.elapsedSec = static_cast<int>(ts - m_firstTimestamp);
    } else {
        p.elapsedSec = m_detail.records.size();  // fall back to sample index
    }

    if (mesg.IsPowerValid())     p.power      = static_cast<int>(mesg.GetPower());
    if (mesg.IsHeartRateValid()) p.heartRate  = static_cast<int>(mesg.GetHeartRate());
    if (mesg.IsCadenceValid())   p.cadence    = static_cast<int>(mesg.GetCadence());
    if (mesg.IsDistanceValid())  p.distanceKm = static_cast<double>(mesg.GetDistance()) / 1000.0;

    m_detail.records.append(p);
}

void FitActivityDetailReader::OnMesg(fit::LapMesg &mesg)
{
    ActivityLap lap;

    if (mesg.IsTotalElapsedTimeValid()) lap.durationSec     = static_cast<int>(mesg.GetTotalElapsedTime());
    if (mesg.IsAvgPowerValid())         lap.avgPowerW       = static_cast<int>(mesg.GetAvgPower());
    if (mesg.IsMaxPowerValid())         lap.maxPowerW       = static_cast<int>(mesg.GetMaxPower());
    if (mesg.IsNormalizedPowerValid())  lap.normalizedPower = static_cast<int>(mesg.GetNormalizedPower());
    if (mesg.IsAvgHeartRateValid())     lap.avgHrBpm        = static_cast<int>(mesg.GetAvgHeartRate());
    if (mesg.IsMaxHeartRateValid())     lap.maxHrBpm        = static_cast<int>(mesg.GetMaxHeartRate());
    if (mesg.IsAvgCadenceValid())       lap.avgCadence      = static_cast<int>(mesg.GetAvgCadence());
    if (mesg.IsTotalDistanceValid())    lap.distanceKm      = static_cast<double>(mesg.GetTotalDistance()) / 1000.0;

    m_detail.laps.append(lap);
}
