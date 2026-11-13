#include "groupworkoutcodec.h"

#include "util.h"
#include "xmlutil.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace GroupWorkoutCodec {

std::optional<GroupProtocol::SharedWorkout> encode(const Workout &workout) {
    QTemporaryDir scratchDir;
    if (!scratchDir.isValid()) return std::nullopt;
    const QString filePath = scratchDir.filePath(QStringLiteral("shared.workout"));
    if (!XmlUtil::createWorkoutXml(workout, filePath)) return std::nullopt;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return std::nullopt;
    return GroupProtocol::SharedWorkout{workout.getName(), file.readAll()};
}

std::optional<Workout> decode(const GroupProtocol::SharedWorkout &sharedWorkout) {
    if (sharedWorkout.xml.isEmpty()) return std::nullopt;

    const QString folder = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                         + QStringLiteral("/MaximumTrainer-group");
    if (!QDir().mkpath(folder)) return std::nullopt;
    QString baseName = Util::cleanForOsSaving(sharedWorkout.name);
    if (baseName.isEmpty()) baseName = QStringLiteral("Group workout");
    const QString filePath = folder + QLatin1Char('/') + baseName + QStringLiteral(".workout");

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return std::nullopt;
    file.write(sharedWorkout.xml);
    file.close();

    XmlUtil xmlUtil;
    Workout workout = xmlUtil.parseSingleWorkoutXml(filePath);
    if (workout.getLstInterval().isEmpty()) return std::nullopt;
    workout.setWorkout_name_enum(Workout::USER_MADE);
    return workout;
}

} // namespace GroupWorkoutCodec
