#include "repeatincreaseeditor.h"
#include "ui_repeatincreaseeditor.h"

#include <QDebug>
#include <QPushButton>
#include <QStyle>


RepeatIncreaseEditor::~RepeatIncreaseEditor()
{
    delete ui;
}




RepeatIncreaseEditor::RepeatIncreaseEditor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::RepeatIncreaseEditor)
{



    ui->setupUi(this);


    setAutoFillBackground(true);
    setAttribute(Qt::WA_NoMousePropagation);
    setWindowFlags(Qt::WindowStaysOnTopHint);





    ui->gridLayout->setRowMinimumHeight(0,26);
    ui->gridLayout->setRowMinimumHeight(1,56);
    ui->gridLayout->setRowMinimumHeight(2,34);

    // Native Ok/Cancel button box: Ok confirms, Cancel discards the edit.
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &RepeatIncreaseEditor::endEdit);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &RepeatIncreaseEditor::cancelEdit);

    // Icon-only buttons (drop the text) so they take less room.
    if (QPushButton *ok = ui->buttonBox->button(QDialogButtonBox::Ok)) {
        ok->setText(QString());
        ok->setIcon(style()->standardIcon(QStyle::SP_DialogOkButton));
    }
    if (QPushButton *cancel = ui->buttonBox->button(QDialogButtonBox::Cancel)) {
        cancel->setText(QString());
        cancel->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    }
}


//////////////////////////////////////////////////////////////////////////////////////////
void RepeatIncreaseEditor::setInterval(const Interval &interval) {

    qDebug() << "RepeatIncreaseEditor start setInterval!";
    this->myInterval = interval;

    ui->doubleSpinBox_increaseFTP->setValue(myInterval.getRepeatIncreaseFTP());
    ui->spinBox_increaseCadence->setValue(myInterval.getRepeatIncreaseCadence());
    ui->doubleSpinBox_increaseLTHR->setValue(myInterval.getRepeatIncreaseLTHR());

    qDebug() << "setInterval RepeatIncreaseEditor done!";

}


//////////////////////////////////////////////////////////////////////////////////////////
void RepeatIncreaseEditor::on_doubleSpinBox_increaseFTP_valueChanged(double arg1)
{
    myInterval.setRepeatIncreaseFTP(arg1);

}

void RepeatIncreaseEditor::on_spinBox_increaseCadence_valueChanged(int arg1)
{
    myInterval.setRepeatIncreaseCadence(arg1);
}

void RepeatIncreaseEditor::on_doubleSpinBox_increaseLTHR_valueChanged(double arg1)
{
    myInterval.setRepeatIncreaseLTHR(arg1);
}
