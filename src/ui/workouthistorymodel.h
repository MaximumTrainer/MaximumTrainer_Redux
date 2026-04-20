#ifndef WORKOUTHISTORYMODEL_H
#define WORKOUTHISTORYMODEL_H

#include <QAbstractTableModel>
#include <QList>

#include "workouthistorysummary.h"

class WorkoutHistoryModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColDate = 0,
        ColWorkout,
        ColDuration,
        ColAvgPower,
        ColNP,
        ColHR,
        ColCadence,
        ColTSS,
        ColDistance,
        ColCalories,
        ColCount
    };

    explicit WorkoutHistoryModel(QObject *parent = nullptr);

    void setHistory(const QList<WorkoutHistorySummary> &history);
    const QList<WorkoutHistorySummary> &history() const { return m_history; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    QList<WorkoutHistorySummary> m_history;
};

#endif // WORKOUTHISTORYMODEL_H
