#include "achievementlivetracker.h"

#include "achievementcatalog.h"

using namespace AchievementCatalog;

AchievementLiveTracker::AchievementLiveTracker()
{
    for (const Definition &definition : all()) {
        if (!isLive(definition))
            continue;
        Window window;
        window.key     = definition.key;
        window.kind    = static_cast<int>(definition.kind);
        window.target  = definition.target;
        window.seconds = qMax(1, definition.seconds);
        m_windows.append(window);
    }
}

void AchievementLiveTracker::reset()
{
    for (Window &window : m_windows) {
        window.samples.clear();
        window.sum = 0.0;
        window.reported = false;
    }
}

QStringList AchievementLiveTracker::addSecond(double powerWatts, double cadenceRpm, double ftpWatts)
{
    QStringList completed;
    for (Window &window : m_windows) {
        const auto kind = static_cast<Kind>(window.kind);
        const double sample = qMax(0.0, kind == Kind::PeakCadence ? cadenceRpm : powerWatts);
        window.samples.push_back(sample);
        window.sum += sample;
        if (static_cast<int>(window.samples.size()) > window.seconds) {
            window.sum -= window.samples.front();
            window.samples.pop_front();
        }
        if (window.reported || static_cast<int>(window.samples.size()) < window.seconds)
            continue;

        double threshold = window.target;
        if (kind == Kind::PeakFtpPercent) {
            if (ftpWatts <= 0)
                continue;
            threshold = window.target * ftpWatts;
        }
        // Small tolerance so a rolling sum of whole watts that equals the
        // target is not lost to floating-point drift.
        if (window.sum / window.seconds >= threshold - 1e-6) {
            window.reported = true;
            completed.append(window.key);
        }
    }
    return completed;
}
