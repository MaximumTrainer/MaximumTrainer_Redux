#ifndef SORTFILTERPROXYMODEL_H
#define SORTFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include "workout.h"


class SortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit SortFilterProxyModel(QObject *parent = nullptr);
    void setFilterKeyColumns(const QList<qint32> &filterColumns);
    void addFilterColumn(qint32 column);
    void addFilterFixedString(qint32 column, const QString &pattern);

    void addFilterWorkoutType(Workout::WORKOUT_NAME workoutType);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QMap<qint32, QString> m_columnPatterns;


    bool showWorkoutIncluded;
    // not used yet, only using bool for filter (Included workout/All workouts)
    Workout::WORKOUT_NAME workoutType;

};


#endif // SORTFILTERPROXYMODEL_H
