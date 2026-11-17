#ifndef MYTABLEMODEL_H
#define MYTABLEMODEL_H

#include <QAbstractTableModel>
#include <QPersistentModelIndex>

#include "workout.h"
#include "account.h"


class WorkoutTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    WorkoutTableModel(QObject *parent = nullptr);
    void addListWorkout(QList<Workout> lstWorkout);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    Workout getWorkoutAtRow(const QModelIndex &index);
    void deleteWorkoutWithName(QString name);
    void deleteAllUserMadeWorkouts();
    void deleleteMapWorkout();
    void addWorkout(Workout workout);

    bool removeRows(int row, int count, const QModelIndex & parent) override;

signals :
    void dataChanged();

public slots :
    void changeRowHovered(int row);
    void updateWorkoutsMetrics();


private:
    QList<Workout> lstWorkout;
    int rowHovered;

    Account *account; //to keep reference of QSet workout done



};

#endif // MYTABLEMODEL_H
