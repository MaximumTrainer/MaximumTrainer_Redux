#ifndef MMPCALCULATOR_H
#define MMPCALCULATOR_H

#include <QVector>
#include <QString>
#include <QList>

class WorkoutHistorySummary;

/// Standard MMP durations in seconds.
static const QVector<int> MMP_DURATIONS = {
    1, 5, 10, 20, 30, 60, 120, 300, 600, 1200, 1800, 3600, 5400, 7200
};

struct CriticalPowerModel
{
    double cp   = 0.0;   ///< Critical Power in watts
    double wPrime = 0.0; ///< W' (anaerobic work capacity) in joules
    bool   valid  = false;
};

/**
 * @brief Computes Mean Maximal Power (MMP) for a set of activity FIT files
 *        and fits the CP + W' critical power model.
 */
class MmpCalculator
{
public:
    /**
     * @brief Reads per-second power from a FIT file.
     * @return Vector of power values (watts), one entry per FIT Record message.
     */
    static QVector<int> readPowerSeries(const QString &fitFilePath);

    /**
     * @brief Computes MMP values over standard durations from all FIT files.
     * @param fitFilePaths  List of absolute paths to .fit files.
     * @param durations     Durations in seconds; defaults to MMP_DURATIONS.
     * @return MMP in watts for each requested duration (0 if no data).
     */
    static QVector<double> computeMmp(const QList<QString> &fitFilePaths,
                                      const QVector<int>   &durations = MMP_DURATIONS);

    /**
     * @brief Fits the CP + W' model to MMP data.
     *
     * Uses linearised regression: Total_Work = W' + CP * Duration.
     * Only durations in the range [120, 1800] s are used for fitting.
     *
     * @param durations  Duration vector (seconds).
     * @param mmps       Corresponding MMP values (watts); 0 means no data.
     * @return Fitted CP and W' (invalid if fewer than 2 valid points).
     */
    static CriticalPowerModel fitCriticalPower(const QVector<int>    &durations,
                                               const QVector<double> &mmps);
};

#endif // MMPCALCULATOR_H
