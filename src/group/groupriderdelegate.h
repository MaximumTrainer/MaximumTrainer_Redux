#ifndef GROUPRIDERDELEGATE_H
#define GROUPRIDERDELEGATE_H

#include <QStyledItemDelegate>

class GroupRidersModel;

/// Paints one rider row: status stripe, name and detail on the left, power and
/// heart rate on the right. Compact enough that 20 riders stay readable.
class GroupRiderDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    static constexpr int kRowHeight = 36;

    explicit GroupRiderDelegate(const GroupRidersModel *model, QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    const GroupRidersModel *m_model;
};

#endif // GROUPRIDERDELEGATE_H
