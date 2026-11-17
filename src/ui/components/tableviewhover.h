#ifndef TABLEVIEWHOVER_H
#define TABLEVIEWHOVER_H

#include <QTableView>
#include <QEvent>
#include <QMouseEvent>
#include <QRubberBand>
#include <QItemSelection>

class TableViewHover : public QTableView
{
    Q_OBJECT

public:
    explicit TableViewHover(QWidget *parent = nullptr);


private :
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent *event) override;
    void focusOutEvent(QFocusEvent* event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;


    void selectionChanged (const QItemSelection & selected, const QItemSelection & deselected ) override;


    int curr_row_hovered;
    int curr_row_selected;
    QRubberBand *bandHover;

};

#endif // TABLEVIEWHOVER_H
