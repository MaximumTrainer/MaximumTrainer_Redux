#ifndef MYTABLEVIEW_H
#define MYTABLEVIEW_H

#include <QTableView>
#include <QMouseEvent>
#include <QWheelEvent>
#include "delegaterowhover.h"


class MyTableView : public QTableView
{
    Q_OBJECT

public:
    explicit MyTableView(QWidget *parent = nullptr);


private :
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent *event) override;


    delegateRowHover *m_background_delegate_hover;
    delegateRowHover *m_background_delegate_notHover;
    int curr_row_hovered;


};

#endif // MYTABLEVIEW_H
