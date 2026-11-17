#ifndef TABLEVIEWINTERVAL_H
#define TABLEVIEWINTERVAL_H

#include <QTableView>
#include <QMouseEvent>
#include <QDragEnterEvent>

class TableViewInterval : public QTableView
{
    Q_OBJECT

public:
    explicit TableViewInterval(QWidget *parent = nullptr);


signals:

public slots:


private :
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;


};

#endif // TABLEVIEWINTERVAL_H

