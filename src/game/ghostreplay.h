#ifndef GHOSTREPLAY_H
#define GHOSTREPLAY_H

#include <QString>
#include <QVector>

/*
 * GhostReplay
 *
 * Loads the per-second record stream of a past ride (.fit) into a
 * distance-over-time curve so it can be replayed as a "ghost" you race against.
 * Fully offline — reads a local FIT via the vendored FIT SDK, no backend.
 *
 * FitActivityReader only listens to session (summary) messages; the ghost needs
 * the record messages (timestamp + distance + speed per sample), hence this is a
 * separate, lighter reader.
 */
class GhostReplay
{
public:
    struct Sample {
        double elapsedSec = 0.0;   // seconds since the first record
        double distanceM  = 0.0;   // accumulated distance, metres
        double speedMps   = 0.0;   // instantaneous speed, m/s
        double powerW     = 0.0;   // recorded power, watts
        double cadenceRpm = 0.0;   // recorded cadence, rpm
    };

    static GhostReplay fromFitFile(const QString &filePath);

    bool   isValid() const        { return m_samples.size() >= 2; }
    double totalTimeSec() const   { return m_samples.isEmpty() ? 0.0 : m_samples.last().elapsedSec; }
    double totalDistanceM() const { return m_samples.isEmpty() ? 0.0 : m_samples.last().distanceM; }
    const QVector<Sample> &samples() const { return m_samples; }

    /// Interpolated distance (metres) the ghost has covered at \a elapsedSec,
    /// clamped to the recorded range.
    double distanceAt(double elapsedSec) const;

    /// Recorded power (watts) at \a elapsedSec — the sample at-or-before, since
    /// power is a noisy step signal rather than something to interpolate.
    double powerAt(double elapsedSec) const;

    /// Recorded cadence (rpm) at \a elapsedSec (sample at-or-before).
    double cadenceAt(double elapsedSec) const;

private:
    QVector<Sample> m_samples;
};

#endif // GHOSTREPLAY_H
