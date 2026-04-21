#include "workouthistorymodel.h"

#include <QFont>

WorkoutHistoryModel::WorkoutHistoryModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void WorkoutHistoryModel::setHistory(const QList<WorkoutHistorySummary> &history)
{
    beginResetModel();
    m_history = history;
    endResetModel();
}

int WorkoutHistoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_history.size();
}

int WorkoutHistoryModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColCount;
}

QVariant WorkoutHistoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_history.size())
        return {};

    const WorkoutHistorySummary &s = m_history.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColDate:
            return s.startTime.toString(QStringLiteral("yyyy-MM-dd  hh:mm"));
        case ColWorkout:
            return s.workoutName;
        case ColDuration: {
            int h = s.durationSec / 3600;
            int m = (s.durationSec % 3600) / 60;
            int sec = s.durationSec % 60;
            return h > 0 ? QStringLiteral("%1h %2m").arg(h).arg(m, 2, 10, QChar('0'))
                         : QStringLiteral("%1:%2").arg(m).arg(sec, 2, 10, QChar('0'));
        }
        case ColAvgPower:
            return s.avgPowerW > 0 ? QStringLiteral("%1 W").arg(s.avgPowerW) : QStringLiteral("—");
        case ColNP:
            return s.normalizedPower > 0 ? QStringLiteral("%1 W").arg(s.normalizedPower) : QStringLiteral("—");
        case ColHR:
            return s.avgHrBpm > 0 ? QStringLiteral("%1 bpm").arg(s.avgHrBpm) : QStringLiteral("—");
        case ColCadence:
            return s.avgCadence > 0 ? QStringLiteral("%1 rpm").arg(s.avgCadence) : QStringLiteral("—");
        case ColTSS:
            return s.tss > 0.0 ? QStringLiteral("%1").arg(s.tss, 0, 'f', 1) : QStringLiteral("—");
        case ColDistance:
            return s.totalDistanceKm > 0.0 ? QStringLiteral("%1 km").arg(s.totalDistanceKm, 0, 'f', 1) : QStringLiteral("—");
        case ColCalories:
            return s.calories > 0 ? QStringLiteral("%1 kcal").arg(s.calories) : QStringLiteral("—");
        default:
            break;
        }
    } else if (role == Qt::UserRole) {
        // Numeric sort values for QSortFilterProxyModel — avoids lexicographic ordering
        // on formatted strings like "120 W", "9.5 km", "1h 05m".
        switch (index.column()) {
        case ColDate:     return s.startTime.toSecsSinceEpoch();
        case ColWorkout:  return s.workoutName;
        case ColDuration: return s.durationSec;
        case ColAvgPower: return s.avgPowerW;
        case ColNP:       return s.normalizedPower;
        case ColHR:       return s.avgHrBpm;
        case ColCadence:  return s.avgCadence;
        case ColTSS:      return s.tss;
        case ColDistance: return s.totalDistanceKm;
        case ColCalories: return s.calories;
        default:          break;
        }
    } else if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColDate:
        case ColWorkout:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        default:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    return {};
}

QVariant WorkoutHistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ColDate:     return QStringLiteral("Date");
    case ColWorkout:  return QStringLiteral("Workout");
    case ColDuration: return QStringLiteral("Duration");
    case ColAvgPower: return QStringLiteral("Avg W");
    case ColNP:       return QStringLiteral("NP");
    case ColHR:       return QStringLiteral("Avg HR");
    case ColCadence:  return QStringLiteral("Cadence");
    case ColTSS:      return QStringLiteral("TSS");
    case ColDistance: return QStringLiteral("Distance");
    case ColCalories: return QStringLiteral("Calories");
    default:          return {};
    }
}

Qt::ItemFlags WorkoutHistoryModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}
