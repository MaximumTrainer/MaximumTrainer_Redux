#include "historyfilterproxymodel.h"

#include <QDateTime>

#include "workouthistorymodel.h"

HistoryFilterProxyModel::HistoryFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void HistoryFilterProxyModel::setNameFilter(const QString &text)
{
    m_nameFilter = text.trimmed();
    invalidateFilter();
}

void HistoryFilterProxyModel::setDateRange(const QDate &from, const QDate &to)
{
    m_from = from;
    m_to   = to;
    invalidateFilter();
}

bool HistoryFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QAbstractItemModel *src = sourceModel();
    if (!src)
        return true;

    if (!m_nameFilter.isEmpty()) {
        const QModelIndex nameIdx = src->index(sourceRow, WorkoutHistoryModel::ColWorkout, sourceParent);
        const QString name = src->data(nameIdx, Qt::DisplayRole).toString();
        if (!name.contains(m_nameFilter, Qt::CaseInsensitive))
            return false;
    }

    if (m_from.isValid() || m_to.isValid()) {
        const QModelIndex dateIdx = src->index(sourceRow, WorkoutHistoryModel::ColDate, sourceParent);
        const qint64 secs = src->data(dateIdx, Qt::UserRole).toLongLong();
        const QDate date = QDateTime::fromSecsSinceEpoch(secs).date();
        if (m_from.isValid() && date < m_from)
            return false;
        if (m_to.isValid() && date > m_to)
            return false;
    }

    return true;
}
