#ifndef RACECOURSE_H
#define RACECOURSE_H

#include <QString>
#include <QVector>

#include <optional>

/// A race course built by tools/courses/build_course.py: a road sampled every
/// ~20 m with position and smoothed elevation. Distances are measured along
/// the whole race, so on a loop course they keep growing lap after lap.
class RaceCourse
{
public:
    struct Point {
        double distanceM = 0.0;   // along one lap
        double latitude = 0.0;
        double longitude = 0.0;
        double elevationM = 0.0;
    };

    struct Climb {
        double startsInM = 0.0;   // distance from the rider to its foot
        double lengthM = 0.0;
        double averageGrade = 0.0;   // 0.08 = 8 %
    };

    static std::optional<RaceCourse> fromJson(const QByteArray &json);
    static std::optional<RaceCourse> fromFile(const QString &path);

    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QString place() const { return m_place; }
    QString attribution() const { return m_attribution; }
    int laps() const { return m_laps; }
    bool isLoop() const { return m_loop; }
    double lapDistanceM() const { return m_lapDistanceM; }
    double totalDistanceM() const { return m_lapDistanceM * m_laps; }
    const QVector<Point> &points() const { return m_points; }

    /// Position on the road at a race distance (laps wrap on a loop course;
    /// clamped to the finish otherwise).
    Point pointAt(double raceDistanceM) const;
    double elevationAt(double raceDistanceM) const { return pointAt(raceDistanceM).elevationM; }
    /// Road grade at a race distance, as a fraction (0.08 = 8 %), measured
    /// over the surrounding 40 m.
    double gradeAt(double raceDistanceM) const;
    /// The next section of at least \a minLengthM averaging \a minGrade or
    /// more, starting within \a lookAheadM of the rider. Empty when none.
    std::optional<Climb> nextClimb(double raceDistanceM, double minGrade = 0.06,
                                   double minLengthM = 200.0, double lookAheadM = 3000.0) const;

private:
    double lapPosition(double raceDistanceM) const;

    QString m_id;
    QString m_name;
    QString m_place;
    QString m_attribution;
    int m_laps = 1;
    bool m_loop = false;
    double m_lapDistanceM = 0.0;
    QVector<Point> m_points;
};

#endif // RACECOURSE_H
