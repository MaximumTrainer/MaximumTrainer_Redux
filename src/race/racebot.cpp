#include "racebot.h"

#include <algorithm>
#include <iterator>
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
    m_sprintFromM = 200.0 + m_random.generateDouble() * 100.0;
    m_sprintFactor = 2.0 + m_random.generateDouble() * 0.6;
}

double RaceBot::nextPower(double grade, double dtSec, double metresToFinish)
{
    // The finish sprint: from 200-300 m out (each bot its own), all in. A short
    // all-out effort is about 2 to 2.6 times what the bot holds all race, so a
    // 3 W/kg bot sprints near 7 W/kg; capped at a realistic 15 W/kg.
    if (metresToFinish <= m_sprintFromM && metresToFinish > 0.0)
        return std::min(m_sprintFactor * m_wattsPerKg, kMaxSprintWattsPerKg) * m_massKg;

    const double kick = (m_random.generateDouble() * 2.0 - 1.0) * kWanderKickPerSec * std::sqrt(dtSec);
    m_wander += kick - m_wander * kWanderPullPerSec * dtSec;
    m_wander = std::clamp(m_wander, -kWanderLimit, kWanderLimit);

    const double terrain = grade <= kDescentGrade ? kDescentEase : 1.0;
    return m_wattsPerKg * m_massKg * terrain * (1.0 + m_wander);
}

QVector<RaceBot> RaceBot::field(const FieldSettings &settings, quint32 seed)
{
    QRandomGenerator random(seed);
    const auto massKg = [&random] { return 62.0 + random.generateDouble() * 20.0; };
    const int count = std::clamp(settings.count, 0, kMaxBots);
    const double low = std::min(settings.minWattsPerKg, settings.maxWattsPerKg);
    const double high = std::max(settings.minWattsPerKg, settings.maxWattsPerKg);

    // Real first names, shuffled, so the standings read like a bunch of people.
    static const char *const kNames[] = {
        "Ana", "Ben", "Carla", "Dev", "Elio", "Fatou", "Gus", "Hana", "Ivo", "Jade", "Kenji", "Lena",
        "Milo", "Nora", "Omar", "Pia", "Quinn", "Rui", "Sara", "Tom", "Uma", "Vic", "Wen", "Xavi",
        "Yara", "Zoe", "Amir", "Bea", "Chloe", "Dario", "Emma", "Finn", "Greta", "Hugo", "Ines",
        "Jonas", "Kira", "Luca", "Maya", "Nico", "Olga", "Pablo", "Rosa", "Sven", "Tara", "Ugo",
        "Vera", "Will", "Yuki", "Zara"};
    QVector<QString> names;
    for (const char *name : kNames)
        names.append(QString::fromLatin1(name));
    for (qsizetype index = names.size() - 1; index > 0; --index)
        std::swap(names[index], names[random.bounded(static_cast<int>(index + 1))]);

    QVector<RaceBot> bots;
    const int pace = count >= 3 ? static_cast<int>(std::lround(count * 0.3)) : 0;
    const double paceWattsPerKg = std::clamp(kPaceWattsPerKg, low, high);
    const int others = count - pace;
    for (int index = 0; index < count; ++index) {
        const double wattsPerKg = index < pace ? paceWattsPerKg
            : others == 1 ? (low + high) / 2.0
                          : low + (high - low) * (index - pace) / (others - 1);
        // The pace group shares one build (72 kg, give or take 1), so at the same
        // W/kg they push the same watts and stay together; at different weights
        // the heavier ones would ride away on the flat.
        const double mass = index < pace ? 71.0 + random.generateDouble() * 2.0 : massKg();
        bots.append(RaceBot(names.at(index % names.size()), wattsPerKg, mass, random.generate()));
    }
    return bots;
}
