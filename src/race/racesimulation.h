#ifndef RACESIMULATION_H
#define RACESIMULATION_H

#include "racebot.h"
#include "racecourse.h"

#include <QString>
#include <QVector>

#include <optional>

/// You and a field of bots on a course. The app feeds your live power and
/// steps the race; every rider's speed comes from RacePhysics with drafting
/// worked out from everyone's position.
class RaceSimulation
{
public:
    struct Rider {
        QString name;
        bool isYou = false;
        double riderKg = 75.0;         // body weight, for W/kg
        double massKg = 83.0;          // rider plus bike, for the physics
        double cda = 0.30;
        double distanceM = 0.0;
        double speedMps = 0.0;
        double powerW = 0.0;           // bots push less while sheltered
        double draftReduction = 0.0;   // share of air drag saved
        double powerSaving = 0.0;      // share of power saved ("saving 24 %")
        int draftingBehind = -1;       // rider index of the wheel being followed
        int ridersSheltering = 0;      // riders within draft range ahead
        std::optional<double> finishTimeSec;
    };

    RaceSimulation(RaceCourse course, QString yourName, double yourRiderKg, double yourBikeKg, double yourCda);

    static constexpr double kBotBikeMassKg = 8.0;

    /// Bots ride your bike in your position (same bike mass and CdA), so equal
    /// W/kg means equal speed.
    void addBot(const RaceBot &bot);
    /// Your live power. Shown at once (warming up before the start too); it
    /// only moves you when the race steps.
    void setYourPower(double watts);

    /// Advance the race by \a dtSec of riding.
    void step(double dtSec);

    const RaceCourse &course() const { return m_course; }
    const QVector<Rider> &riders() const { return m_riders; }
    const Rider &you() const { return m_riders.constFirst(); }
    double elapsedSec() const { return m_elapsedSec; }
    bool youFinished() const { return you().finishTimeSec.has_value(); }

    /// Rider indexes in race order: finishers by time, then by distance.
    QVector<int> standings() const;
    /// Your position in the race, 1-based.
    int yourPosition() const;
    /// Time gap from you to \a riderIndex in seconds: negative = ahead of you.
    double gapToYouSec(int riderIndex) const;

private:
    void updateDrafting();
    double speedAfter(int riderIndex, double watts, double dtSec, double grade) const;

    RaceCourse m_course;
    QVector<Rider> m_riders;          // index 0 is you
    QVector<RaceBot> m_bots;          // m_bots[i] drives m_riders[i + 1]
    double m_yourPowerW = 0.0;
    QVector<double> m_smoothedPowerW;   // per rider, of what they are willing to push
    double m_elapsedSec = 0.0;
};

#endif // RACESIMULATION_H
