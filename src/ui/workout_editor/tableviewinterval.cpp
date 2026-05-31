#include "tableviewinterval.h"
#include <QDebug>
#include "intervalviewstyle.h"
#include <QApplication>
#include <QHeaderView>

TableViewInterval::TableViewInterval(QWidget *parent) :
    QTableView(parent)
{
    this->setMouseTracking(true);

    setStyle(new IntervalViewStyle(style()));

    // Enable drag-to-reorder rows via the grab handle in column 0.
    setDragEnabled(true);
    setAcceptDrops(true);
    setDropIndicatorShown(true);
    setDragDropMode(QAbstractItemView::InternalMove);
    setDragDropOverwriteMode(false);
    setDefaultDropAction(Qt::MoveAction);
    // ExtendedSelection (not SingleSelection) so Shift-click / Ctrl-click
    // can select a multi-row range — required by the Repeat button, which
    // wraps the selected interval range in a repeat block.
    setSelectionMode(QAbstractItemView::ExtendedSelection);
}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void TableViewInterval::mouseMoveEvent(QMouseEvent* event) {


    QPoint pos = event->pos();
    QModelIndex index = indexAt(pos);

    //    qDebug() << "mouseMoveEvent" << index.row();


    /// Drag cursor when hoverwing first column
    if (index.isValid() && index.column() == 0) {
//        QApplication::setOverrideCursor(Qt::SizeAllCursor);
        setCursor(Qt::SizeAllCursor);
    }
    else {
//        QApplication::restoreOverrideCursor();
        setCursor(Qt::ArrowCursor);
    }
    QTableView::mouseMoveEvent(event);
}




///////////////////////////////////////////////////////////////////////////////////////////////////////
//void TableViewInterval::setCursor(QModelIndex index) {

//    Q_UNUSED(index);
//        qDebug() << "TableViewInterval setCursor" << index.row();

//    if (index.isValid() && index.column() == 0) {
//        QApplication::setOverrideCursor(Qt::SizeAllCursor);
//    }
//    else {
//        QApplication::setOverrideCursor(Qt::ArrowCursor);
//    }


//}


///////////////////////////////////////////////////////////////////////////////////////////////////////
bool TableViewInterval::eventFilter(QObject *watched, QEvent *event) {


    Q_UNUSED(watched);


    //    qDebug() << "watched object" << watched << "event:" << event << "eventType" << event->type();

    //    if(qobject_cast<QHeaderView*>(watched) != nullptr && event->type() == QEvent::HoverMove)
    //    {
    //        qDebug() << "HoverMove" << watched;
    //        QApplication::setOverrideCursor(Qt::ArrowCursor);
    //    }

    /// Change cursor to normal when hovering on Headers;
    if(event->type() == QEvent::HoverMove)
    {
//        QApplication::restoreOverrideCursor();
        setCursor(Qt::ArrowCursor);
    }

    return false;


}




//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void TableViewInterval::leaveEvent(QEvent *event) {


    //    qDebug() << "leaveEvent";
    //    QApplication::setOverrideCursor(Qt::ArrowCursor);

//    QApplication::restoreOverrideCursor();
    setCursor(Qt::ArrowCursor);
    QTableView::leaveEvent(event);
}



