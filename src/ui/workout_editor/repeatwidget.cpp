#include "repeatwidget.h"
#include "ui_repeatwidget.h"
#include <QLabel>
#include <QDebug>

RepeatWidget::~RepeatWidget() {
    delete ui;
    delete data;
}

//void RepeatWidget::moveEvent (QMoveEvent * event) {

//    qDebug() << "MOVE EVENT repeat Widget";
//}



///////////////////////////////////////////////////////////////////////////////////////////////////////////
RepeatWidget::RepeatWidget(RepeatData *data, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RepeatWidget)
{
    ui->setupUi(this);


    QIcon iconRepeat(":/image/icon/repeat");
    for (int i=0; i<ui->comboBox_repeat->count(); i++)
        ui->comboBox_repeat->setItemIcon(i, iconRepeat);
    setMouseTracking(true);


    setWindowFlags(Qt::WindowStaysOnTopHint);
    setWindowFlags(Qt::FramelessWindowHint );

    ui->widget_left->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->widget_right->setAttribute(Qt::WA_NoMousePropagation);




    this->data = data;

    ui->comboBox_repeat->setCurrentText(QString::number(data->getNumberRepeat()));

    // Connect after setCurrentText() so seeding the initial value does not emit.
    connect(ui->comboBox_repeat, &QComboBox::currentTextChanged,
            this, &RepeatWidget::onRepeatCountChanged);

    ui->comboBox_repeat->installEventFilter(this);
    ui->widget_right->installEventFilter(this);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool RepeatWidget::eventFilter(QObject *watched, QEvent *event) {

    Q_UNUSED(watched);

//    ///Check if mouse is on the scroll bar, remove hover effect on the rows
//    QComboBox *ptrCombo = qobject_cast<QComboBox*>(watched);

//    if (ptrCombo != NULL && event->type() == QEvent::MouseButtonPress) {
//        qDebug() << "Button Press got here EventFilter!...";
//    }

    if (event->type() == QEvent::MouseButtonPress) {
        qDebug() << "Button Press got here EventFilter!...";
        emit clickedRightPartWidget();
    }


    return false;
}



//---------------------------------------------------------
void RepeatWidget::setRightWidth(int width) {
    ui->widget_right->setFixedWidth(width);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
bool RepeatWidget::myLessThan(RepeatWidget* left, RepeatWidget* right) {

    return (left->getRepeatData()->getFirstRow() < right->getRepeatData()->getFirstRow());
}




//////////////////////////////////////////////////////////////////////////////
void RepeatWidget::on_pushButton_delete_clicked()
{
    emit deleteSignal(data->getId());
}

void RepeatWidget::onRepeatCountChanged(const QString &arg1)
{
    this->data->setNumberRepeat(arg1.toInt());
    emit updateSignal(this->data->getId());
}


