#include "racecourse.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace {
constexpr double kGradeHalfWindowM = 20.0;
constexpr double kClimbScanStepM = 20.0;
}

std::optional<RaceCourse> RaceCourse::fromJson(const QByteArray &json)
{
    const QJsonObject object = QJsonDocument::fromJson(json).object();
    const QJsonArray rawPoints = object.value(QStringLiteral("points")).toArray();
    if (rawPoints.size() < 2)
        return std::nullopt;

    RaceCourse course;
    course.m_id = object.value(QStringLiteral("id")).toString();
    course.m_name = object.value(QStringLiteral("name")).toString();
    course.m_place = object.value(QStringLiteral("place")).toString();
    course.m_attribution = object.value(QStringLiteral("attribution")).toString();
    course.m_laps = std::max(1, object.value(QStringLiteral("laps")).toInt(1));
    course.m_loop = object.value(QStringLiteral("loop")).toBool(false);
    course.m_points.reserve(rawPoints.size());
    for (const auto &value : rawPoints) {
        const QJsonArray row = value.toArray();
        if (row.size() < 4)
            return std::nullopt;
        course.m_points.append({row.at(0).toDouble(), row.at(1).toDouble(),
                                row.at(2).toDouble(), row.at(3).toDouble()});
    }
    for (qsizetype index = 1; index < course.m_points.size(); ++index)
        if (course.m_points.at(index).distanceM <= course.m_points.at(index - 1).distanceM)
            return std::nullopt;
    course.m_lapDistanceM = course.m_points.constLast().distanceM;
    if (!course.m_loop)
        course.m_laps = 1;
    return course;
}

std::optional<RaceCourse> RaceCourse::fromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    return fromJson(file.readAll());
}

double RaceCourse::lapPosition(double raceDistanceM) const
{
    if (raceDistanceM <= 0.0)
        return m_loop ? std::fmod(std::fmod(raceDistanceM, m_lapDistanceM) + m_lapDistanceM, m_lapDistanceM)
                      : 0.0;
    if (m_loop) {
        if (raceDistanceM >= totalDistanceM())
            return m_lapDistanceM;
        return std::fmod(raceDistanceM, m_lapDistanceM);
    }
    return std::min(raceDistanceM, m_lapDistanceM);
}

RaceCourse::Point RaceCourse::pointAt(double raceDistanceM) const
{
    const double position = lapPosition(raceDistanceM);
    const auto after = std::lower_bound(m_points.cbegin(), m_points.cend(), position,
                                        [](const Point &point, double distance) { return point.distanceM < distance; });
    if (after == m_points.cbegin())
        return m_points.constFirst();
    if (after == m_points.cend())
        return m_points.constLast();
    const Point &next = *after;
    const Point &previous = *(after - 1);
    const double t = (position - previous.distanceM) / (next.distanceM - previous.distanceM);
    return {position,
            previous.latitude + t * (next.latitude - previous.latitude),
            previous.longitude + t * (next.longitude - previous.longitude),
            previous.elevationM + t * (next.elevationM - previous.elevationM)};
}

double RaceCourse::gradeAt(double raceDistanceM) const
{
    double from = raceDistanceM - kGradeHalfWindowM;
    double to = raceDistanceM + kGradeHalfWindowM;
    if (!m_loop) {
        from = std::max(0.0, from);
        to = std::min(m_lapDistanceM, to);
    }
    if (to - from < 1.0)
        return 0.0;
    // On a loop, sample across the lap seam with unwrapped distances so the
    // grade just before the line looks at the start of the next lap.
    const double rise = pointAt(to).elevationM - pointAt(from).elevationM;
    return rise / (to - from);
}

std::optional<RaceCourse::Climb> RaceCourse::nextClimb(double raceDistanceM, double minGrade,
                                                       double minLengthM, double lookAheadM) const
{
    const double end = std::min(totalDistanceM(), raceDistanceM + lookAheadM + 5000.0);
    double climbStart = -1.0;
    const int steps = static_cast<int>((end - raceDistanceM) / kClimbScanStepM);
    for (int step = 0; step <= steps; ++step) {
        const double distance = raceDistanceM + step * kClimbScanStepM;
        const bool steep = gradeAt(distance) >= minGrade;
        if (steep && climbStart < 0.0) {
            if (distance - raceDistanceM > lookAheadM)
                return std::nullopt;
            climbStart = distance;
        }
        if (!steep && climbStart >= 0.0) {
            const double length = distance - climbStart;
            if (length >= minLengthM) {
                const double rise = elevationAt(distance) - elevationAt(climbStart);
                return Climb{climbStart - raceDistanceM, length, rise / length};
            }
            climbStart = -1.0;
        }
    }
    if (climbStart >= 0.0 && end - climbStart >= minLengthM) {
        const double rise = elevationAt(end) - elevationAt(climbStart);
        return Climb{climbStart - raceDistanceM, end - climbStart, rise / (end - climbStart)};
    }
    return std::nullopt;
}
