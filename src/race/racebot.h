#ifndef RACEBOT_H
#define RACEBOT_H

#include <QRandomGenerator>
#include <QString>
#include <QVector>

/// A computer rider. It holds a W/kg target and rides a little like a person:
/// its power wanders by about +/-5 % over tens of seconds and it soft-pedals
/// downhill. It does not push harder on climbs, so a 3.0 W/kg bot is one you
/// can follow at 3.0 W/kg all the way up.
class RaceBot
{
public:
    RaceBot(QString name, double wattsPerKg, double massKg, quint32 seed);

    QString name() const { return m_name; }
    double wattsPerKg() const { return m_wattsPerKg; }
    double massKg() const { return m_massKg; }   // rider only; the race adds the bike

    /// Power for the next \a dtSec on a road of \a grade, \a metresToFinish
    /// from the line (the bot sprints over the last 200-300 m). Call once per step.
    double nextPower(double grade, double dtSec, double metresToFinish = 1e9);
    static constexpr double kMaxSprintWattsPerKg = 15.0;

    struct FieldSettings {
        int count = 20;
        double minWattsPerKg = 2.2;
        double maxWattsPerKg = 4.0;
    };
    static constexpr double kPaceWattsPerKg = 3.0;
    static constexpr int kMaxBots = 50;

    /// A field of bots with real first names: about 30 % ride together at
    /// 3.0 W/kg (held inside the range) for you to follow, the rest are
    /// spread evenly from the minimum to the maximum W/kg.
    static QVector<RaceBot> field(const FieldSettings &settings, quint32 seed);
    /// The standard field: 20 bots, 2.2 to 4.0 W/kg, six of them at 3.0.
    static QVector<RaceBot> defaultField(quint32 seed) { return field(FieldSettings(), seed); }

private:
    QString m_name;
    double m_wattsPerKg;
    double m_massKg;
    double m_wander = 0.0;
    double m_sprintFromM = 250.0;
    double m_sprintFactor = 2.3;   // slow random offset, as a fraction of target power
    QRandomGenerator m_random;
};

#endif // RACEBOT_H
