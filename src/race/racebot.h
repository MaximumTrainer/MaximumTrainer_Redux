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

    /// Power for the next \a dtSec on a road of \a grade. Call once per step.
    double nextPower(double grade, double dtSec);

    /// The standard bot-race field: a pace group of six at 3.0 W/kg to follow,
    /// and fourteen riders spread from 2.2 to 4.0 W/kg.
    static QVector<RaceBot> defaultField(quint32 seed);

private:
    QString m_name;
    double m_wattsPerKg;
    double m_massKg;
    double m_wander = 0.0;   // slow random offset, as a fraction of target power
    QRandomGenerator m_random;
};

#endif // RACEBOT_H
