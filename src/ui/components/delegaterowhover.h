#ifndef DELEGATEROWHOVER_H
#define DELEGATEROWHOVER_H

#include <QStyledItemDelegate>
#include <QTableView>


class delegateRowHover : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit delegateRowHover( bool hoverer, QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;





private:
    bool hovered;



};

#endif // DELEGATEROWHOVER_H
