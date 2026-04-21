#include "mmpcalculator.h"

#include <algorithm>
#include <numeric>
#include <fstream>
#include <cmath>

#include <QDebug>

#include "fit_decode.hpp"
#include "fit_mesg_broadcaster.hpp"
#include "fit_profile.hpp"
#include "fit_record_mesg.hpp"
#include "fit_record_mesg_listener.hpp"
#include "fit_runtime_exception.hpp"

// ---------------------------------------------------------------------------
// Internal FIT Record listener
// ---------------------------------------------------------------------------
namespace {

class PowerSeriesReader : public fit::RecordMesgListener
{
public:
    QVector<int> powers;   ///< 1-Hz power series (0 for seconds with no valid power)

    void OnMesg(fit::RecordMesg &mesg) override
    {
        if (!mesg.IsTimestampValid())
            return;

        FIT_DATE_TIME ts = mesg.GetTimestamp();

        if (m_lastTs == 0) {
            // First record — seed the series
            m_lastTs = ts;
            int w = (mesg.IsPowerValid()) ? static_cast<int>(mesg.GetPower()) : 0;
            powers.append(qBound(0, w, 4999));
            return;
        }

        // Fill any gaps with 0 (dropout / pause)
        int gap = static_cast<int>(ts - m_lastTs);
        if (gap <= 0) gap = 1; // guard against duplicate / out-of-order timestamps
        if (gap > 3600) gap = 1; // guard against huge jumps (e.g. multi-day file merge)

        for (int i = 1; i < gap; ++i)
            powers.append(0);

        int w = (mesg.IsPowerValid()) ? static_cast<int>(mesg.GetPower()) : 0;
        powers.append(qBound(0, w, 4999));
        m_lastTs = ts;
    }

private:
    FIT_DATE_TIME m_lastTs = 0;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// MmpCalculator
// ---------------------------------------------------------------------------

QVector<int> MmpCalculator::readPowerSeries(const QString &fitFilePath)
{
    PowerSeriesReader reader;

    std::ifstream stream;
#ifdef Q_OS_WIN32
    stream.open(fitFilePath.toStdWString(), std::ios::in | std::ios::binary);
#else
    stream.open(fitFilePath.toStdString(), std::ios::in | std::ios::binary);
#endif

    if (!stream.is_open())
        return {};

    fit::Decode decoder;
    fit::MesgBroadcaster broadcaster;
    broadcaster.AddListener(static_cast<fit::RecordMesgListener &>(reader));

    try {
        decoder.Read(stream, broadcaster, broadcaster);
    } catch (const fit::RuntimeException &) {
        // partial decode — return whatever we extracted
    } catch (...) {}

    return reader.powers;
}

QVector<double> MmpCalculator::computeMmp(const QList<QString>  &fitFilePaths,
                                           const QVector<int>    &durations)
{
    const int nDur = durations.size();
    QVector<double> best(nDur, 0.0);

    for (const QString &path : fitFilePaths) {
        QVector<int> power = readPowerSeries(path);
        const int n = power.size();
        if (n == 0) continue;

        for (int di = 0; di < nDur; ++di) {
            const int D = durations[di];
            if (D > n) continue; // file too short

            // Compute initial window sum
            long long windowSum = 0;
            for (int i = 0; i < D; ++i)
                windowSum += power[i];

            double maxAvg = static_cast<double>(windowSum) / D;

            // Slide window
            for (int i = D; i < n; ++i) {
                windowSum += power[i] - power[i - D];
                double avg = static_cast<double>(windowSum) / D;
                if (avg > maxAvg)
                    maxAvg = avg;
            }

            if (maxAvg > best[di])
                best[di] = maxAvg;
        }
    }

    return best;
}

CriticalPowerModel MmpCalculator::fitCriticalPower(const QVector<int>    &durations,
                                                    const QVector<double> &mmps)
{
    // Fit using durations in [120 s, 1800 s] (2 min to 30 min).
    // Model: TotalWork = W' + CP * Duration
    // Linear regression Y = a + b*X, Y = MMP*Duration, X = Duration
    QVector<double> xs, ys;
    for (int i = 0; i < durations.size() && i < mmps.size(); ++i) {
        int D = durations[i];
        double mmp = mmps[i];
        if (mmp <= 0.0) continue;
        if (D < 120 || D > 1800) continue;
        xs.append(static_cast<double>(D));
        ys.append(mmp * D); // total work (joules)
    }

    if (xs.size() < 3)
        return {};

    const int n = xs.size();
    double sumX = 0, sumY = 0, sumXX = 0, sumXY = 0;
    for (int i = 0; i < n; ++i) {
        sumX  += xs[i];
        sumY  += ys[i];
        sumXX += xs[i] * xs[i];
        sumXY += xs[i] * ys[i];
    }

    double denom = n * sumXX - sumX * sumX;
    if (std::abs(denom) < 1e-9)
        return {};

    CriticalPowerModel model;
    model.cp     = (n * sumXY - sumX * sumY) / denom;   // slope = CP
    model.wPrime = (sumY - model.cp * sumX) / n;         // intercept = W'
    model.valid  = (model.cp > 0 && model.wPrime > 0);
    return model;
}
