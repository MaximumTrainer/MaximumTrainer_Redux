#include "intervaldelegate.h"
#include <QDebug>
#include <QTimeEdit>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QApplication>
#include <QDoubleSpinBox>
#include "powereditor.h"
#include "cadenceeditor.h"
#include "hreditor.h"
#include "repeatincreaseeditor.h"
#include "tableviewinterval.h"




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//void IntervalDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
//{

//    QStyleOptionViewItemV4 opt = option;
//    initStyleOption(&opt, index);
//    opt.showDecorationSelected = true;



//    else {
//        QStyledItemDelegate::paint(painter, option, index);
//    }
//}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IntervalDelegate::setParentWidget(QWidget *parent) {

    this->parent = parent;

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QWidget *IntervalDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const

{


    /// Duration;
    if (index.column() == 1 ) {
        QTimeEdit *editor = new QTimeEdit(parent);
        editor->setDisplayFormat("hh:mm:ss");
        return editor;
    }
    /// Power
    else if (index.column() == 2 ) {
        PowerEditor *editor = new PowerEditor(parent);
        connect (editor, SIGNAL(endEdit()),    this, SLOT(closeWidgetEditor()) );
        connect (editor, SIGNAL(cancelEdit()), this, SLOT(cancelWidgetEditor()) );
        return editor;
    }
    /// Cadence
    else if (index.column() == 3 ) {
        CadenceEditor *editor = new CadenceEditor(parent);
        connect (editor, SIGNAL(endEdit()),    this, SLOT(closeWidgetEditor()) );
        connect (editor, SIGNAL(cancelEdit()), this, SLOT(cancelWidgetEditor()) );
        return editor;
    }
    /// Heart Rate
    else if (index.column() == 4 ) {
        HrEditor *editor = new HrEditor(parent);
        connect (editor, SIGNAL(endEdit()),    this, SLOT(closeWidgetEditor()) );
        connect (editor, SIGNAL(cancelEdit()), this, SLOT(cancelWidgetEditor()) );
        return editor;
    }
    /// Display Msg
    else if (index.column() == 5 ) {
        QLineEdit *editor = new QLineEdit(parent);
        return editor;
    }
    /// Repeat Perc
    else if (index.column() == 6 ) {
        RepeatIncreaseEditor *editor = new RepeatIncreaseEditor(parent);
        connect (editor, SIGNAL(endEdit()),    this, SLOT(closeWidgetEditor()) );
        connect (editor, SIGNAL(cancelEdit()), this, SLOT(cancelWidgetEditor()) );
        return editor;

    }
    else {
        return QStyledItemDelegate::createEditor(parent, option, index);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////
void IntervalDelegate::closeWidgetEditor() {

    qDebug() << "CLOSE EDITOR NOW!";
    QWidget *editor = qobject_cast<QWidget*>(sender());

    emit commitData(editor);
    emit closeEditor(editor);

}


/////////////////////////////////////////////////////////////////////////////////////////////////
void IntervalDelegate::cancelWidgetEditor() {

    // Close without committing — discards the edit (Cancel button).
    QWidget *editor = qobject_cast<QWidget*>(sender());
    emit closeEditor(editor);

}


/////////////////////////////////////////////////////////////////////////////////////////////////
void IntervalDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{

    /// Duration;
    if (index.column() == 1 ) {
        QTime time1 = index.model()->data(index, Qt::EditRole).toTime();
        QTimeEdit *timeEdit = static_cast<QTimeEdit*>(editor);
        timeEdit->setTime(time1);
    }
    /// Power
    else if (index.column() == 2 ) {
        Interval interval = index.model()->data(index, Qt::EditRole).value<Interval>();
        PowerEditor *powerEditor = qobject_cast<PowerEditor*>(editor);
        powerEditor->setInterval(interval);
    }
    /// Cadence
    else if (index.column() == 3 ) {
        Interval interval = index.model()->data(index, Qt::EditRole).value<Interval>();
        CadenceEditor *cadenceEditor = qobject_cast<CadenceEditor*>(editor);
        cadenceEditor->setInterval(interval);

    }
    /// Heartrate
    else if (index.column() == 4 ) {
        Interval interval = index.model()->data(index, Qt::EditRole).value<Interval>();
        HrEditor *hrEditor = qobject_cast<HrEditor*>(editor);
        hrEditor->setInterval(interval);
    }
    else if (index.column() == 5 ) {
        QString displayMsg = index.model()->data(index, Qt::EditRole).toString();
        QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);
        lineEdit->setText(displayMsg);
    }
    /// Repeat Increase
    else if (index.column() == 6 ) {
        Interval interval = index.model()->data(index, Qt::EditRole).value<Interval>();
        RepeatIncreaseEditor *repeatEditor = qobject_cast<RepeatIncreaseEditor*>(editor);
        repeatEditor->setInterval(interval);

    }
    else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////
void IntervalDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                    const QModelIndex &index) const
{

    /// Duration
    if (index.column() == 1) {
        QTimeEdit *timeEdit = static_cast<QTimeEdit*>(editor);
        timeEdit->interpretText();
        QTime timeNow = timeEdit->time();
        model->setData(index, timeNow, Qt::EditRole);
    }
    /// Power
    else if (index.column() == 2 ) {
        PowerEditor *powerEditor = static_cast<PowerEditor*>(editor);
        QVariant stored;
        stored.setValue(powerEditor->getInterval());
        model->setData(index, stored, Qt::EditRole);
    }
    /// Cadence
    else if (index.column() == 3 ) {
        CadenceEditor *cadenceEditor = static_cast<CadenceEditor*>(editor);
        QVariant stored;
        stored.setValue(cadenceEditor->getInterval());
        model->setData(index, stored, Qt::EditRole);
    }
    /// Cadence
    else if (index.column() == 4 ) {
        HrEditor *hrEditor = static_cast<HrEditor*>(editor);
        QVariant stored;
        stored.setValue(hrEditor->getInterval());
        model->setData(index, stored, Qt::EditRole);
    }
    /// Display Msg
    else if (index.column() == 5) {
        QLineEdit *lineEdit = static_cast<QLineEdit*>(editor);
        QString msgNow = lineEdit->text();
        model->setData(index, msgNow, Qt::EditRole);
    }
    /// Repeat perc
    else if (index.column() == 6 ) {
        RepeatIncreaseEditor *repeatEditor = static_cast<RepeatIncreaseEditor*>(editor);
        QVariant stored;
        stored.setValue(repeatEditor->getInterval());
        model->setData(index, stored, Qt::EditRole);
    }
    else {
        QStyledItemDelegate::setModelData(editor, model, index);
    }

}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IntervalDelegate::updateEditorGeometry(QWidget *editor,  const QStyleOptionViewItem &option, const QModelIndex &index ) const {

    editor->setGeometry(option.rect);

    /// Power
    if (index.column() == 2) {
        editor->setWindowFlags(Qt::Popup);
        editor->setGeometry(option.rect.x(), option.rect.y(), 701, 90);
        editor->move(editor->parentWidget()->mapToGlobal(option.rect.topLeft()));
    }
    /// Cadence
    else if  (index.column() == 3) {
        editor->setWindowFlags(Qt::Popup);
        editor->setGeometry(option.rect.x(), option.rect.y(), 350, 90);
        editor->move(editor->parentWidget()->mapToGlobal(option.rect.topLeft()));
    }
    /// HR
    else if  (index.column() == 4) {
        editor->setWindowFlags(Qt::Popup);
        editor->setGeometry(option.rect.x(), option.rect.y(), 350, 90);
        editor->move(editor->parentWidget()->mapToGlobal(option.rect.topLeft()));
    }
    /// Repeat Change
    else if  (index.column() == 6) {
        editor->setWindowFlags(Qt::Popup);
        editor->setGeometry(option.rect.x(), option.rect.y(), 490, 150);
        editor->move(editor->parentWidget()->mapToGlobal(option.rect.topLeft()));
    }



}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QSize IntervalDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{

    //    return QSize(500,200);

    return QStyledItemDelegate::sizeHint(option, index);

    //    if (index.data().canConvert<StarRating>()) {
    //        StarRating starRating = qvariant_cast<StarRating>(index.data());
    //        return starRating.sizeHint();
    //    } else {
    //        return QStyledItemDelegate::sizeHint(option, index);
    //    }
}
