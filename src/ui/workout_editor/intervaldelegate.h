#ifndef INTERVALDELEGATE_H
#define INTERVALDELEGATE_H

#include <QStyledItemDelegate>

class IntervalDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    IntervalDelegate(QWidget *parent = 0) : QStyledItemDelegate(parent) {}


    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const;
    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const;

    void updateEditorGeometry(QWidget *editor,  const QStyleOptionViewItem &option, const QModelIndex &/* index */) const;

    void setParentWidget(QWidget *parent);


signals:


public slots:
    void closeWidgetEditor();
    void cancelWidgetEditor();

private :

    QWidget *parent;


};

#endif // INTERVALDELEGATE_H

