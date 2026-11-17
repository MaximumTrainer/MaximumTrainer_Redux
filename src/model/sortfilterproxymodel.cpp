#include "sortfilterproxymodel.h"

#include <QDebug>


SortFilterProxyModel::SortFilterProxyModel(QObject *parent) : QSortFilterProxyModel(parent)
{

    showWorkoutIncluded = true;
}


void SortFilterProxyModel::setFilterKeyColumns(const QList<qint32> &filterColumns)
{
    m_columnPatterns.clear();

    for (qint32 column : std::as_const(filterColumns))
        m_columnPatterns.insert(column, QString());
}


void SortFilterProxyModel::addFilterColumn(qint32 column) {

    m_columnPatterns.insert(column, QString());
}


void SortFilterProxyModel::addFilterFixedString(qint32 column, const QString &pattern)
{
    if(!m_columnPatterns.contains(column)) {
        return;
    }

    m_columnPatterns[column] = pattern;

}

//void SortFilterProxyModel::addFilterInt(int value)
//{
//    typeWorkout = value;


void SortFilterProxyModel::addFilterWorkoutType(Workout::WORKOUT_NAME workoutType) {

    if (workoutType == Workout::INCLUDED_WORKOUT) {
        showWorkoutIncluded = true;
    }
    else {
        showWorkoutIncluded = false;
    }
    this->workoutType = workoutType; // not used
}


bool SortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{

    if(m_columnPatterns.isEmpty())
        return true;


    ///Check Id of workout
    QModelIndex index = sourceModel()->index(sourceRow, 2, sourceParent);
    QVariant variant = sourceModel()->data(index, Qt::UserRole);
    Workout workout;
    workout = variant.value<Workout>();

    if (!showWorkoutIncluded && workout.getWorkoutNameEnum() != Workout::USER_MADE)
        return false;


    bool ret = false;

    for(QMap<qint32, QString>::const_iterator iter = m_columnPatterns.constBegin();
        iter != m_columnPatterns.constEnd();
        ++iter)
    {
        QModelIndex index = sourceModel()->index(sourceRow, iter.key(), sourceParent);
        ret = index.data().toString().startsWith(iter.value(), Qt::CaseInsensitive);
        //        compare ( const QString & other, Qt::CaseSensitivity cs ) const
        //        contains ( const QString & str, Qt::CaseSensitivity cs = Qt::CaseSensitive ) const
        //        startsWith ( const QString & s, Qt::CaseSensitivity cs = Qt::CaseSensitive ) const


        if(!ret)
            return ret;
    }

    return ret;
}

