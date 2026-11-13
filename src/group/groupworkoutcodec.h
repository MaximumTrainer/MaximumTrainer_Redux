#ifndef GROUPWORKOUTCODEC_H
#define GROUPWORKOUTCODEC_H

#include "groupprotocol.h"
#include "workout.h"

#include <optional>

/// Converts between a Workout and the `.workout` XML the host shares. Targets
/// stay relative to FTP/LTHR, so each rider's trainer scales them locally.
namespace GroupWorkoutCodec {

std::optional<GroupProtocol::SharedWorkout> encode(const Workout &workout);

/// Writes the shared XML to the app's temp folder (the name comes from the file
/// name, as for any local workout) and parses it back.
std::optional<Workout> decode(const GroupProtocol::SharedWorkout &sharedWorkout);

} // namespace GroupWorkoutCodec

#endif // GROUPWORKOUTCODEC_H
