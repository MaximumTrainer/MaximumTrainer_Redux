#ifndef HISTORYFILTERPROXYMODEL_H
#define HISTORYFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>
#include <QDate>

/// Filters the workout history by workout-name substring and an inclusive
/// start/end date range. An invalid (null) bound disables that side of the range.
class HistoryFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit HistoryFilterProxyModel(QObject *parent = nullptr);

    void setNameFilter(const QString &text);
    void setDateRange(const QDate &from, const QDate &to);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_nameFilter;
    QDate   m_from;
    QDate   m_to;
};

#endif // HISTORYFILTERPROXYMODEL_H
