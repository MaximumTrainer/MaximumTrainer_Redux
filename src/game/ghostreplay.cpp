#include "ghostreplay.h"

#include <fstream>
#include <algorithm>

#include <QtGlobal>

#include "fit_decode.hpp"
#include "fit_mesg_broadcaster.hpp"
#include "fit_record_mesg_listener.hpp"
#include "fit_runtime_exception.hpp"

namespace {
// Collects the (elapsed, distance, speed) triple from each record message.
class RecordCollector : public fit::RecordMesgListener
{
public:
    QVector<GhostReplay::Sample> samples;

    void OnMesg(fit::RecordMesg &mesg) override
    {
        if (!mesg.IsTimestampValid() || !mesg.IsDistanceValid())
            return;

        const double t = static_cast<double>(mesg.GetTimestamp());
        if (!m_haveT0) { m_t0 = t; m_haveT0 = true; }

        GhostReplay::Sample s;
        s.elapsedSec = t - m_t0;
        s.distanceM  = static_cast<double>(mesg.GetDistance());
        s.speedMps   = mesg.IsSpeedValid() ? static_cast<double>(mesg.GetSpeed()) : 0.0;
        s.powerW     = mesg.IsPowerValid() ? static_cast<double>(mesg.GetPower()) : 0.0;
        s.cadenceRpm = mesg.IsCadenceValid() ? static_cast<double>(mesg.GetCadence()) : 0.0;
        samples.append(s);
    }

private:
    bool   m_haveT0 = false;
    double m_t0     = 0.0;
};
} // namespace

GhostReplay GhostReplay::fromFitFile(const QString &filePath)
{
    GhostReplay replay;

    std::ifstream stream;
#ifdef Q_OS_WIN32
    stream.open(filePath.toStdWString(), std::ios::in | std::ios::binary);
#else
    stream.open(filePath.toStdString(), std::ios::in | std::ios::binary);
#endif
    if (!stream.is_open())
        return replay;

    RecordCollector collector;
    fit::Decode decoder;
    fit::MesgBroadcaster broadcaster;
    broadcaster.AddListener(static_cast<fit::RecordMesgListener &>(collector));

    try {
        decoder.Read(stream, broadcaster, broadcaster);
    } catch (const fit::RuntimeException &) {
        // partial decode — keep whatever we collected
    } catch (...) {
    }

    replay.m_samples = collector.samples;
    // Activity records are chronological, but guard against any reordering.
    std::sort(replay.m_samples.begin(), replay.m_samples.end(),
              [](const Sample &a, const Sample &b) { return a.elapsedSec < b.elapsedSec; });
    return replay;
}

double GhostReplay::distanceAt(double elapsedSec) const
{
    if (m_samples.isEmpty())
        return 0.0;
    if (elapsedSec <= m_samples.first().elapsedSec)
        return m_samples.first().distanceM;
    if (elapsedSec >= m_samples.last().elapsedSec)
        return m_samples.last().distanceM;

    int lo = 0;
    int hi = m_samples.size() - 1;
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        if (m_samples[mid].elapsedSec <= elapsedSec) lo = mid; else hi = mid;
    }

    const Sample &a = m_samples[lo];
    const Sample &b = m_samples[hi];
    const double span = b.elapsedSec - a.elapsedSec;
    if (span <= 0.0)
        return a.distanceM;
    const double f = (elapsedSec - a.elapsedSec) / span;
    return a.distanceM + f * (b.distanceM - a.distanceM);
}

double GhostReplay::powerAt(double elapsedSec) const
{
    if (m_samples.isEmpty())
        return 0.0;
    if (elapsedSec <= m_samples.first().elapsedSec)
        return m_samples.first().powerW;
    if (elapsedSec >= m_samples.last().elapsedSec)
        return m_samples.last().powerW;

    int lo = 0;
    int hi = m_samples.size() - 1;
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        if (m_samples[mid].elapsedSec <= elapsedSec) lo = mid; else hi = mid;
    }
    return m_samples[lo].powerW;   // sample at-or-before (step signal)
}

double GhostReplay::cadenceAt(double elapsedSec) const
{
    if (m_samples.isEmpty())
        return 0.0;
    if (elapsedSec <= m_samples.first().elapsedSec)
        return m_samples.first().cadenceRpm;
    if (elapsedSec >= m_samples.last().elapsedSec)
        return m_samples.last().cadenceRpm;

    int lo = 0;
    int hi = m_samples.size() - 1;
    while (hi - lo > 1) {
        const int mid = (lo + hi) / 2;
        if (m_samples[mid].elapsedSec <= elapsedSec) lo = mid; else hi = mid;
    }
    return m_samples[lo].cadenceRpm;
}
