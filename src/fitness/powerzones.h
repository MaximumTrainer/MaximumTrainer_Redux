#ifndef POWERZONES_H
#define POWERZONES_H

#include <QColor>
#include <QList>
#include <QStringList>
#include <QVector>

/// Power zones as % of FTP, shared by the workout graphs, the power card and
/// the post-workout summary so a zone has the same bounds and colour everywhere.
namespace PowerZones {

/// Zone upper bounds in % of FTP: the athlete's Intervals.icu zones without
/// their open-ended top value (999 %), or Coggan's 55/75/90/105/120/150.
/// N bounds make N + 1 zones.
QVector<int> boundsPct(const QList<int> &athleteZonesPct);

/// Zone index (0 = Z1) for \a pct: the first bound it does not exceed.
int zoneForPct(double pct, const QVector<int> &bounds);

QColor color(int zone);

/// "Z3 Tempo": the athlete's zone name when there is one for every zone,
/// Coggan's names for the default 7 zones, else just "Z3".
QString label(int zone, int zoneCount, const QStringList &athleteNames);

} // namespace PowerZones

#endif // POWERZONES_H
