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
}

double RaceBot::nextPower(double grade, double dtSec)
{
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

    QVector<RaceBot> bots;
    const int pace = count >= 3 ? static_cast<int>(std::lround(count * 0.3)) : 0;
    const double paceWattsPerKg = std::clamp(kPaceWattsPerKg, low, high);
    for (int index = 1; index <= pace; ++index)
        bots.append(RaceBot(QStringLiteral("Pace %1").arg(index), paceWattsPerKg, massKg(), random.generate()));

    static const char *const names[] = {"Ana", "Ben", "Carla", "Dev", "Elio", "Fatou", "Gus", "Hana", "Ivo",
                                        "Jade", "Kenji", "Lena", "Milo", "Nora", "Omar", "Pia", "Quinn",
                                        "Rui", "Sara", "Tom", "Uma", "Vic", "Wen", "Xavi", "Yara", "Zoe"};
    constexpr int kNames = static_cast<int>(std::size(names));
    const int others = count - pace;
    for (int index = 0; index < others; ++index) {
        const double wattsPerKg = others == 1 ? (low + high) / 2.0 : low + (high - low) * index / (others - 1);
        QString name = QString::fromLatin1(names[index % kNames]);
        if (index >= kNames)
            name += QStringLiteral(" %1").arg(index / kNames + 1);
        bots.append(RaceBot(name, wattsPerKg, massKg(), random.generate()));
    }
    return bots;
}
