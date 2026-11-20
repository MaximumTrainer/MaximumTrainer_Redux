#include "powerzones.h"

#include <QCoreApplication>
#include <QLinearGradient>
#include <algorithm>
#include <array>

namespace PowerZones {

QVector<int> boundsPct(const QList<int> &athleteZonesPct)
{
    QVector<int> bounds;
    for (const int pct : athleteZonesPct) {
        if (pct > 0 && pct < 300)
            bounds.append(pct);
    }
    if (bounds.isEmpty())
        bounds = {55, 75, 90, 105, 120, 150};
    return bounds;
}

int zoneForPct(double pct, const QVector<int> &bounds)
{
    for (int zone = 0; zone < bounds.size(); ++zone) {
        if (pct <= bounds.at(zone))
            return zone;
    }
    return int(bounds.size());
}

QColor color(int zone)
{
    static const std::array<QColor, 10> colors {
        QColor("#9e9e9e"), QColor("#2f8fd8"), QColor("#3fb27f"), QColor("#f2c230"), QColor("#f28c28"),
        QColor("#e5534b"), QColor("#a052d6"), QColor("#d6529e"), QColor("#7a5cff"), QColor("#5c5c66")};
    return colors.at(qBound(0, zone, int(colors.size()) - 1));
}

QBrush brush(double startPct, double endPct, const QVector<int> &bounds, int alpha)
{
    const auto zoneColor = [alpha](int zone) {
        QColor zoneColor = color(zone);
        zoneColor.setAlpha(alpha);
        return zoneColor;
    };
    if (zoneForPct(startPct, bounds) == zoneForPct(endPct, bounds))
        return {zoneColor(zoneForPct(startPct, bounds))};

    QVector<double> cuts {0.0, 1.0};
    const double lowPct = std::min(startPct, endPct);
    const double highPct = std::max(startPct, endPct);
    for (const int bound : bounds) {
        if (bound > lowPct && bound < highPct)
            cuts.append((bound - startPct) / (endPct - startPct));
    }
    std::sort(cuts.begin(), cuts.end());

    const auto zoneAt = [&](double position) {
        return zoneForPct(startPct + (endPct - startPct) * position, bounds);
    };
    constexpr double fadeTowardNext = 0.6;
    constexpr double hardEdge = 0.0005;
    QLinearGradient gradient(0, 0, 1, 0);
    gradient.setCoordinateMode(QGradient::ObjectMode);
    for (int segment = 0; segment + 1 < cuts.size(); ++segment) {
        const double from = cuts.at(segment);
        const double to = cuts.at(segment + 1);
        const QColor current = zoneColor(zoneAt((from + to) / 2));
        QColor end = current;
        if (segment + 2 < cuts.size()) {
            const QColor next = zoneColor(zoneAt((to + cuts.at(segment + 2)) / 2));
            end = QColor::fromRgbF(
                float(current.redF() + (next.redF() - current.redF()) * fadeTowardNext),
                float(current.greenF() + (next.greenF() - current.greenF()) * fadeTowardNext),
                float(current.blueF() + (next.blueF() - current.blueF()) * fadeTowardNext),
                current.alphaF());
        }
        gradient.setColorAt(segment == 0 ? from : std::min(from + hardEdge, to), current);
        gradient.setColorAt(to, end);
    }
    return {gradient};
}

QString label(int zone, int zoneCount, const QStringList &athleteNames)
{
    QString number = QStringLiteral("Z%1").arg(zone + 1);
    if (athleteNames.size() == zoneCount && !athleteNames.at(zone).trimmed().isEmpty())
        return number + QLatin1Char(' ') + athleteNames.at(zone).trimmed();
    if (zoneCount == 7) {
        static const std::array<const char *, 7> coggan {
            QT_TRANSLATE_NOOP("PowerZones", "Recovery"), QT_TRANSLATE_NOOP("PowerZones", "Endurance"),
            QT_TRANSLATE_NOOP("PowerZones", "Tempo"),    QT_TRANSLATE_NOOP("PowerZones", "Threshold"),
            QT_TRANSLATE_NOOP("PowerZones", "VO2 Max"),  QT_TRANSLATE_NOOP("PowerZones", "Anaerobic"),
            QT_TRANSLATE_NOOP("PowerZones", "Neuromuscular")};
        return number + QLatin1Char(' ') + QCoreApplication::translate("PowerZones", coggan.at(zone));
    }
    return number;
}

} // namespace PowerZones
