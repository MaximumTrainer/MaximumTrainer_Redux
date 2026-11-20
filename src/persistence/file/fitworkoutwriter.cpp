#include "fitworkoutwriter.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QtMath>
#include <fstream>

#include "fit_encode.hpp"
#include "fit_file_id_mesg.hpp"
#include "fit_workout_mesg.hpp"
#include "fit_workout_step_mesg.hpp"
#include "util.h"
#include "workout.h"

namespace FitWorkoutWriter {

namespace {
constexpr int kMaxRampSteps = 10;

Intensity intensityFor(int index, int count, int powerPct)
{
    if (index == 0 && powerPct < 75)
        return Intensity::Warmup;
    if (index == count - 1 && powerPct < 75)
        return Intensity::Cooldown;
    if (powerPct > 0 && powerPct < 60)
        return Intensity::Recovery;
    return Intensity::Active;
}

FIT_INTENSITY toFit(Intensity intensity)
{
    switch (intensity) {
    case Intensity::Warmup:   return FIT_INTENSITY_WARMUP;
    case Intensity::Cooldown: return FIT_INTENSITY_COOLDOWN;
    case Intensity::Recovery: return FIT_INTENSITY_RECOVERY;
    case Intensity::Active:   break;
    }
    return FIT_INTENSITY_ACTIVE;
}

std::wstring wide(const QString &text)
{
    return text.toStdWString();
}
} // namespace

QVector<Step> buildSteps(const Workout &workout, int ftpWatts, int lthrBpm)
{
    QVector<Step> steps;
    const QList<Interval> &intervals = workout.getLstInterval();
    const int ftp = qMax(1, ftpWatts);
    for (int index = 0; index < intervals.size(); ++index) {
        const Interval &interval = intervals.at(index);
        const int seconds = qRound(Util::convertQTimeToSecD(interval.getDurationQTime()));
        if (seconds <= 0)
            continue;
        const bool hasPower = interval.getPowerStepType() != Interval::NONE;
        const bool ramp = interval.getPowerStepType() == Interval::PROGRESSIVE
                          && !qFuzzyCompare(interval.getFTP_start(), interval.getFTP_end());
        const int rangePct = qMax(2, qRound(interval.getFTP_range() * 100.0 / ftp));
        const int parts = ramp ? qBound(2, seconds / 60, kMaxRampSteps) : 1;

        for (int part = 0; part < parts; ++part) {
            Step step;
            step.name = interval.getDisplayMessage().left(30);
            step.seconds = part < parts - 1 ? seconds / parts : seconds - (seconds / parts) * (parts - 1);
            if (hasPower) {
                // Mid-point of this slice of the ramp (the whole step when flat).
                const double fraction = (part + 0.5) / parts;
                const double target = interval.getFTP_start()
                                      + (interval.getFTP_end() - interval.getFTP_start()) * fraction;
                const int pct = qRound(target * 100);
                step.hasPower = true;
                step.powerLowPct = qMax(1, pct - rangePct);
                step.powerHighPct = pct + rangePct;
            }
            if (interval.getCadenceStepType() != Interval::NONE) {
                const int cadence = (interval.getCadence_start() + interval.getCadence_end()) / 2;
                step.hasCadence = true;
                step.cadenceLow = qMax(1, cadence - interval.getCadence_range());
                step.cadenceHigh = cadence + interval.getCadence_range();
            }
            if (interval.getHRStepType() != Interval::NONE && lthrBpm > 0) {
                const int bpm = qRound((interval.getHR_start() + interval.getHR_end()) / 2 * lthrBpm);
                step.hasHeartRate = true;
                step.heartRateLow = qMax(1, bpm - interval.getHR_range());
                step.heartRateHigh = bpm + interval.getHR_range();
            }
            const int midPct = step.hasPower ? (step.powerLowPct + step.powerHighPct) / 2 : 0;
            step.intensity = intensityFor(index, intervals.size(), midPct);
            steps.append(step);
        }
    }
    return steps;
}

bool write(const Workout &workout, int ftpWatts, int lthrBpm, const QString &path, QString *error)
{
    const QVector<Step> steps = buildSteps(workout, ftpWatts, lthrBpm);
    if (steps.isEmpty()) {
        if (error) *error = QCoreApplication::translate("FitWorkoutWriter", "This workout has no timed steps to export.");
        return false;
    }

    std::fstream file;
    file.open(path.toStdString(), std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        if (error) *error = QCoreApplication::translate("FitWorkoutWriter", "Could not create %1").arg(path);
        return false;
    }
    fit::Encode encode(fit::ProtocolVersion::V20);
    encode.Open(file);

    fit::FileIdMesg fileId;
    fileId.SetType(FIT_FILE_WORKOUT);
    fileId.SetManufacturer(FIT_MANUFACTURER_DEVELOPMENT);
    fileId.SetProduct(1);
    fileId.SetSerialNumber(12345);
    fileId.SetTimeCreated(static_cast<FIT_DATE_TIME>(QDateTime::currentSecsSinceEpoch() - 631065600));
    encode.Write(fileId);

    fit::WorkoutMesg workoutMesg;
    workoutMesg.SetWktName(wide(workout.getName().left(40)));
    workoutMesg.SetSport(FIT_SPORT_CYCLING);
    workoutMesg.SetNumValidSteps(static_cast<FIT_UINT16>(steps.size()));
    if (!workout.getDescription().isEmpty())
        workoutMesg.SetWktDescription(wide(workout.getDescription().left(120)));
    encode.Write(workoutMesg);

    for (int i = 0; i < steps.size(); ++i) {
        const Step &step = steps.at(i);
        fit::WorkoutStepMesg mesg;
        mesg.SetMessageIndex(static_cast<FIT_MESSAGE_INDEX>(i));
        if (!step.name.isEmpty())
            mesg.SetWktStepName(wide(step.name));
        mesg.SetDurationType(FIT_WKT_STEP_DURATION_TIME);
        mesg.SetDurationTime(static_cast<FIT_FLOAT32>(step.seconds));
        mesg.SetIntensity(toFit(step.intensity));
        if (step.hasPower) {
            // FIT custom power targets below 1000 are % of FTP.
            mesg.SetTargetType(FIT_WKT_STEP_TARGET_POWER);
            mesg.SetTargetValue(0);
            mesg.SetCustomTargetPowerLow(static_cast<FIT_WORKOUT_POWER>(step.powerLowPct));
            mesg.SetCustomTargetPowerHigh(static_cast<FIT_WORKOUT_POWER>(step.powerHighPct));
        } else if (step.hasHeartRate) {
            // FIT custom heart-rate targets above 100 are bpm + 100.
            mesg.SetTargetType(FIT_WKT_STEP_TARGET_HEART_RATE);
            mesg.SetTargetValue(0);
            mesg.SetCustomTargetHeartRateLow(static_cast<FIT_WORKOUT_HR>(step.heartRateLow + 100));
            mesg.SetCustomTargetHeartRateHigh(static_cast<FIT_WORKOUT_HR>(step.heartRateHigh + 100));
        } else {
            mesg.SetTargetType(FIT_WKT_STEP_TARGET_OPEN);
        }
        if (step.hasCadence) {
            mesg.SetSecondaryTargetType(FIT_WKT_STEP_TARGET_CADENCE);
            mesg.SetSecondaryCustomTargetCadenceLow(static_cast<FIT_UINT32>(step.cadenceLow));
            mesg.SetSecondaryCustomTargetCadenceHigh(static_cast<FIT_UINT32>(step.cadenceHigh));
        }
        encode.Write(mesg);
    }

    const bool ok = encode.Close();
    file.close();
    if (!ok && error)
        *error = QCoreApplication::translate("FitWorkoutWriter", "Could not finish writing %1").arg(path);
    return ok;
}

} // namespace FitWorkoutWriter
