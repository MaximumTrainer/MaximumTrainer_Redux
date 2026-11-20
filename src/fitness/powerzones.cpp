#include "powerzones.h"

#include <QCoreApplication>
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
