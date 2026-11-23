#include "racebot.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr double kWanderLimit = 0.05;
constexpr double kWanderPullPerSec = 0.05;   // how fast the wander drifts back to 0
constexpr double kWanderKickPerSec = 0.02;
constexpr double kDescentGrade = -0.02;
constexpr double kDescentEase = 0.45;
}

RaceBot::RaceBot(QString name, double wattsPerKg, double massKg, quint32 seed)
    : m_name(std::move(name)), m_wattsPerKg(wattsPerKg), m_massKg(massKg), m_random(seed)
{
}

double RaceBot::nextPower(double grade, double dtSec)
{
    const double kick = (m_random.generateDouble() * 2.0 - 1.0) * kWanderKickPerSec * std::sqrt(dtSec);
    m_wander += kick - m_wander * kWanderPullPerSec * dtSec;
    m_wander = std::clamp(m_wander, -kWanderLimit, kWanderLimit);

    const double terrain = grade <= kDescentGrade ? kDescentEase : 1.0;
    return m_wattsPerKg * m_massKg * terrain * (1.0 + m_wander);
}

QVector<RaceBot> RaceBot::defaultField(quint32 seed)
{
    QRandomGenerator random(seed);
    const auto massKg = [&random] { return 62.0 + random.generateDouble() * 20.0; };

    QVector<RaceBot> field;
    for (int index = 1; index <= 6; ++index)
        field.append(RaceBot(QStringLiteral("Pace %1").arg(index), 3.0, massKg(), random.generate()));

    static const char *const names[] = {"Ana", "Ben", "Carla", "Dev", "Elio", "Fatou", "Gus",
                                        "Hana", "Ivo", "Jade", "Kenji", "Lena", "Milo", "Nora"};
    constexpr int kOthers = 14;
    for (int index = 0; index < kOthers; ++index) {
        const double wattsPerKg = 2.2 + (4.0 - 2.2) * index / (kOthers - 1);
        field.append(RaceBot(QString::fromLatin1(names[index]), wattsPerKg, massKg(), random.generate()));
    }
    return field;
}
