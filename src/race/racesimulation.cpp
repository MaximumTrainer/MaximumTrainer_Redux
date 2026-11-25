#include "racesimulation.h"

#include "racephysics.h"

#include <algorithm>
#include <utility>

namespace {
constexpr double kMinGapSpeedMps = 1.0;

// Holding a wheel. A real rider feathers the pedals to sit behind someone; raw
// power alone either drifts off the back or slingshots past. While your power
// is enough for the draft but not enough to ride away, you settle on the wheel.
constexpr double kWheelGapM = 1.5;
constexpr double kOnWheelGapM = 1.0;   // where a wheel gives its full draft
constexpr double kPowerSmoothingSec = 3.0;
constexpr double kEnoughToHold = 0.95;    // of the watts needed on the wheel...
constexpr double kCloseToTheirs = 0.90;   // ...or of the W/kg they are pushing (they may come past with momentum)
constexpr double kHoldRangeM = 15.0;      // a little beyond the draft: time to jump on as someone passes
constexpr double kEnoughToPass = 1.08;    // of the watts needed without it
constexpr double kMinHoldWatts = 20.0;    // nearly coasting downhill: plain physics
constexpr double kLettingGoMps = 0.15;     // a wheel this much slower than one further up is losing it
constexpr double kGapPull = 0.06;         // 1/s^2
constexpr double kSpeedPull = 0.5;        // 1/s
constexpr double kMaxHoldAccel = 0.6;     // m/s^2
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

double RaceSimulation::yourSpeedAfter(double dtSec, double grade) const
{
    const Rider &me = you();
    const double plain = RacePhysics::stepSpeed(me.speedMps, m_yourPowerW, me.massKg, me.cda, grade,
                                                me.draftReduction, dtSec);
    if (m_yourPowerW <= 0.0)
        return plain;
    QVector<int> wheelsAhead;
    for (int index = 1; index < m_riders.size(); ++index) {
        const double gapAhead = m_riders.at(index).distanceM - me.distanceM;
        if (gapAhead > 0.0 && gapAhead <= kHoldRangeM && !m_riders.at(index).finishTimeSec)
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
        for (int index = 1; index < m_riders.size(); ++index)
            if (!m_riders.at(index).finishTimeSec && m_riders.at(index).distanceM >= rider.distanceM)
                gaps.append(m_riders.at(index).distanceM - rider.distanceM + kOnWheelGapM);
        return wattsAtSpeedOf(rider, RacePhysics::draftReduction(gaps));
    };
    const auto strongEnoughFor = [&](const Rider &rider) {
        return m_yourSmoothedPowerW >= kEnoughToHold * wattsOnWheelOf(rider)
            || m_yourSmoothedPowerW / me.riderKg >= kCloseToTheirs * rider.powerW / rider.riderKg;
    };
    // The nearest wheel, unless it is letting a faster one go whose draft your watts
    // fully cover (a wheel you would lose again is not worth leaving this one for).
    int chosen = wheelsAhead.constFirst();
    for (const int candidate : std::as_const(wheelsAhead))
        if (m_riders.at(candidate).speedMps > m_riders.at(chosen).speedMps + kLettingGoMps
            && m_yourSmoothedPowerW >= wattsAtSpeedOf(m_riders.at(candidate), RacePhysics::kWheelDraftReduction))
            chosen = candidate;
    const Rider &wheel = m_riders.at(chosen);
    const double alone = wattsAtSpeedOf(wheel, 0.0);
    const double onWheel = wattsOnWheelOf(wheel);
    if (onWheel < kMinHoldWatts || !strongEnoughFor(wheel)
        || m_yourSmoothedPowerW > kEnoughToPass * alone)
        return plain;
    const double gap = wheel.distanceM - me.distanceM;
    const double accel = std::clamp(kGapPull * (gap - kWheelGapM) + kSpeedPull * (wheel.speedMps - me.speedMps),
                                    -kMaxHoldAccel, kMaxHoldAccel);
    return std::max(0.0, me.speedMps + accel * dtSec);
}

void RaceSimulation::step(double dtSec)
{
    if (dtSec <= 0.0)
        return;
    updateDrafting();
    m_yourSmoothedPowerW += (m_yourPowerW - m_yourSmoothedPowerW) * std::min(1.0, dtSec / kPowerSmoothingSec);
    const double total = m_course.totalDistanceM();
    for (int index = 0; index < m_riders.size(); ++index) {
        Rider &rider = m_riders[index];
        if (rider.finishTimeSec)
            continue;
        const double grade = m_course.gradeAt(std::max(0.0, rider.distanceM));
        rider.powerW = rider.isYou ? m_yourPowerW
                                   : m_bots[index - 1].nextPower(grade, dtSec, total - rider.distanceM);
        rider.speedMps = rider.isYou ? yourSpeedAfter(dtSec, grade)
                                     : RacePhysics::stepSpeed(rider.speedMps, rider.powerW, rider.massKg, rider.cda,
                                                grade, rider.draftReduction, dtSec);
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
