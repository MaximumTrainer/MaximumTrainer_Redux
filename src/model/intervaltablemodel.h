#ifndef INTERVALTABLEMODEL_H
#define INTERVALTABLEMODEL_H

#include <QAbstractTableModel>
#include <QPersistentModelIndex>
#include <QMimeData>
#include <QStringList>
#include <QModelIndexList>

#include "interval.h"
#include "account.h"

class IntervalTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit IntervalTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;


    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    Qt::DropActions supportedDropActions() const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData* mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;

    bool insertRows(int position, int rows, const QModelIndex &index) override;
    bool removeRows(int row, int count, const QModelIndex & parent) override;
    bool copyRows(int row);

    void removeIntervalRepeatData(int firstRow, int lastRow);



    void updateRowSourceInterval();
    Interval getIntervalAtRow(const QModelIndex &index);
    void setListInterval(QList<Interval> lstInterval);
    QList<Interval> getLstInterval();
    void resetLstInterval();

signals :
    void dataChanged();

private:
    Account *account;

    QList<Interval> lstInterval;

};

#endif // INTERVALTABLEMODEL_H
