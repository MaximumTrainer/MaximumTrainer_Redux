#include "racesimulation.h"

#include "racephysics.h"

#include <algorithm>
#include <utility>

namespace {
constexpr double kMinGapSpeedMps = 1.0;

// Sitting on a wheel, for every rider. Speed always comes from watts and draft;
// near a wheel a rider may only ease off (soft-pedal, or feather the brakes) to
// stay behind it instead of running past. Nobody is ever pushed along, so too
// few watts means slipping back.
constexpr double kWheelGapM = 1.5;
constexpr double kOnWheelGapM = 1.0;      // where a wheel gives its full draft
constexpr double kPowerSmoothingSec = 3.0;
constexpr double kHoldRangeM = 15.0;      // ease off early enough to arrive smoothly
constexpr double kEnoughToPass = 1.08;    // of the watts their speed takes without draft
constexpr double kMinHoldWatts = 20.0;    // nearly coasting downhill: plain physics
constexpr double kLettingGoMps = 0.15;    // a wheel this much slower than one further up is losing it
constexpr double kLostWheelM = 3.0;        // ...and it has really opened a gap
constexpr double kGapPull = 0.06;         // 1/s^2
constexpr double kSpeedPull = 0.5;        // 1/s
constexpr double kMaxEaseOff = 0.6;       // m/s^2
}

RaceSimulation::RaceSimulation(RaceCourse course, QString yourName, double yourRiderKg, double yourBikeKg,
                               double yourCda)
    : m_course(std::move(course))
{
    Rider you;
    you.name = std::move(yourName);
    you.isYou = true;
    you.riderKg = yourRiderKg;
    you.massKg = yourRiderKg + yourBikeKg;
    you.cda = yourCda;
    m_riders.append(you);
}

void RaceSimulation::setYourPower(double watts)
{
    m_yourPowerW = watts;
    if (!m_riders.constFirst().finishTimeSec)
        m_riders.first().powerW = watts;
}

void RaceSimulation::addBot(const RaceBot &bot)
{
    Rider rider;
    rider.name = bot.name();
    rider.riderKg = bot.massKg();
    rider.ftpWattsPerKg = bot.wattsPerKg();
    rider.massKg = bot.massKg() + you().massKg - you().riderKg;
    rider.cda = you().cda;
    rider.powerW = bot.wattsPerKg() * bot.massKg();   // on the start line, before the first step
    // Everyone starts together on the line.
    rider.distanceM = 0.0;
    m_riders.append(rider);
    m_bots.append(bot);
}

void RaceSimulation::updateDrafting()
{
    for (Rider &rider : m_riders) {
        QVector<double> gaps;
        double nearestGap = RacePhysics::kDraftRangeM + 1.0;
        rider.draftingBehind = -1;
        for (int other = 0; other < m_riders.size(); ++other) {
            const Rider &ahead = m_riders.at(other);
            if (&ahead == &rider || ahead.finishTimeSec)
                continue;
            const double gap = ahead.distanceM - rider.distanceM;
            if (gap <= 0.0 || gap > RacePhysics::kDraftRangeM)
                continue;
            gaps.append(gap);
            if (gap < nearestGap) {
                nearestGap = gap;
                rider.draftingBehind = other;
            }
        }
        rider.draftReduction = RacePhysics::draftReduction(gaps);
        rider.ridersSheltering = static_cast<int>(gaps.size());
    }
}

double RaceSimulation::speedAfter(int riderIndex, double watts, double dtSec, double grade) const
{
    const Rider &me = m_riders.at(riderIndex);
    const double smoothedW = m_smoothedPowerW.at(riderIndex);
    const double plain = RacePhysics::stepSpeed(me.speedMps, watts, me.massKg, me.cda, grade,
                                                me.draftReduction, dtSec);
    QVector<int> wheelsAhead;
    for (int index = 0; index < m_riders.size(); ++index) {
        const double gapAhead = m_riders.at(index).distanceM - me.distanceM;
        if (index != riderIndex && gapAhead > 0.0 && gapAhead <= kHoldRangeM && !m_riders.at(index).finishTimeSec)
            wheelsAhead.append(index);
    }
    if (wheelsAhead.isEmpty())
        return plain;
    std::sort(wheelsAhead.begin(), wheelsAhead.end(), [this](int left, int right) {
        return m_riders.at(left).distanceM < m_riders.at(right).distanceM;
    });
    const auto wattsAtSpeedOf = [&](const Rider &rider, double draft) {
        return RacePhysics::steadyPower(rider.speedMps, me.massKg, me.cda, grade, draft);
    };
    // On that wheel you are sheltered by everyone ahead of it too.
    const auto wattsOnWheelOf = [&](const Rider &rider) {
        QVector<double> gaps;
        for (const int index : std::as_const(wheelsAhead))
            if (m_riders.at(index).distanceM >= rider.distanceM)
                gaps.append(m_riders.at(index).distanceM - rider.distanceM + kOnWheelGapM);
        return wattsAtSpeedOf(rider, RacePhysics::draftReduction(gaps));
    };
    // The nearest wheel, unless it is letting a faster one go whose draft these
    // watts fully cover (a wheel you would lose again is not worth leaving this one for).
    int chosen = wheelsAhead.constFirst();
    for (const int candidate : std::as_const(wheelsAhead))
        if (m_riders.at(candidate).speedMps > m_riders.at(chosen).speedMps + kLettingGoMps
            && m_riders.at(candidate).distanceM > m_riders.at(chosen).distanceM + kLostWheelM
            && smoothedW >= wattsOnWheelOf(m_riders.at(candidate)))
            chosen = candidate;
    const Rider &wheel = m_riders.at(chosen);
    if (wattsOnWheelOf(wheel) < kMinHoldWatts || smoothedW > kEnoughToPass * wattsAtSpeedOf(wheel, 0.0))
        return plain;
    const double gap = wheel.distanceM - me.distanceM;
    const double accel = std::max(kGapPull * (gap - kWheelGapM) + kSpeedPull * (wheel.speedMps - me.speedMps),
                                  -kMaxEaseOff);
    return std::clamp(me.speedMps + accel * dtSec, 0.0, plain);
}

void RaceSimulation::step(double dtSec)
{
    if (dtSec <= 0.0)
        return;
    updateDrafting();
    m_smoothedPowerW.resize(m_riders.size());
    const double total = m_course.totalDistanceM();
    for (int index = 0; index < m_riders.size(); ++index) {
        Rider &rider = m_riders[index];
        if (rider.finishTimeSec)
            continue;
        const double grade = m_course.gradeAt(std::max(0.0, rider.distanceM));
        const double watts = rider.isYou ? m_yourPowerW
                                         : m_bots[index - 1].nextPower(grade, dtSec, total - rider.distanceM);
        m_smoothedPowerW[index] += (watts - m_smoothedPowerW.at(index)) * std::min(1.0, dtSec / kPowerSmoothingSec);
        const double nextSpeed = speedAfter(index, watts, dtSec, grade);
        rider.powerW = rider.isYou
            ? watts
            : std::min(watts, RacePhysics::powerForStep(rider.speedMps, nextSpeed, rider.massKg, rider.cda, grade,
                                                        rider.draftReduction, dtSec));
        rider.speedMps = nextSpeed;
        rider.powerSaving = RacePhysics::powerSaving(rider.speedMps, rider.massKg, rider.cda,
                                                     grade, rider.draftReduction);
        const double before = rider.distanceM;
        rider.distanceM += rider.speedMps * dtSec;
        if (rider.distanceM >= total) {
            const double share = rider.speedMps > 0.0 ? (total - before) / (rider.speedMps * dtSec) : 1.0;
            rider.finishTimeSec = m_elapsedSec + std::clamp(share, 0.0, 1.0) * dtSec;
            rider.distanceM = total;
        }
    }
    m_elapsedSec += dtSec;
}

QVector<int> RaceSimulation::standings() const
{
    QVector<int> order(m_riders.size());
    for (int index = 0; index < order.size(); ++index)
        order[index] = index;
    std::stable_sort(order.begin(), order.end(), [this](int left, int right) {
        const Rider &a = m_riders.at(left);
        const Rider &b = m_riders.at(right);
        if (a.finishTimeSec && b.finishTimeSec)
            return *a.finishTimeSec < *b.finishTimeSec;
        if (a.finishTimeSec != b.finishTimeSec)
            return a.finishTimeSec.has_value();
        return a.distanceM > b.distanceM;
    });
    return order;
}

int RaceSimulation::yourPosition() const
{
    return static_cast<int>(standings().indexOf(0)) + 1;
}

double RaceSimulation::gapToYouSec(int riderIndex) const
{
    const Rider &other = m_riders.at(riderIndex);
    const Rider &me = you();
    if (other.finishTimeSec && me.finishTimeSec)
        return *other.finishTimeSec - *me.finishTimeSec;
    const double speed = std::max(kMinGapSpeedMps, me.finishTimeSec ? other.speedMps : me.speedMps);
    return (me.distanceM - other.distanceM) / speed;
}
