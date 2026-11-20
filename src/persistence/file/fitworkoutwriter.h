#ifndef FITWORKOUTWRITER_H
#define FITWORKOUTWRITER_H

#include <QString>
#include <QVector>

class Workout;

/// Exports a workout as a FIT workout file that bike computers (Garmin,
/// Wahoo, Hammerhead...) can run outdoors. Power targets are written as % of
/// FTP, so the device scales them with the FTP it knows. FIT workouts have no
/// ramps, so a ramp becomes several short steps.
namespace FitWorkoutWriter {

enum class Intensity { Warmup, Active, Recovery, Cooldown };

struct Step {
    QString   name;
    int       seconds = 0;
    bool      hasPower = false;
    int       powerLowPct = 0, powerHighPct = 0;    ///< % of FTP
    bool      hasCadence = false;
    int       cadenceLow = 0, cadenceHigh = 0;      ///< rpm
    bool      hasHeartRate = false;
    int       heartRateLow = 0, heartRateHigh = 0;  ///< bpm
    Intensity intensity = Intensity::Active;
};

/// The device steps for \a workout. \a ftpWatts and \a lthrBpm turn the
/// workout's watt ranges and LTHR fractions into the units FIT uses.
QVector<Step> buildSteps(const Workout &workout, int ftpWatts, int lthrBpm);

/// Writes \a workout to \a path; false with \a error set on failure.
bool write(const Workout &workout, int ftpWatts, int lthrBpm, const QString &path, QString *error = nullptr);

} // namespace FitWorkoutWriter

#endif // FITWORKOUTWRITER_H
