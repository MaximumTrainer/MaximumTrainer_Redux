#include "hreditor.h"
#include "ui_hreditor.h"
#include "metriceditorvisibility.h"
#include <QDebug>

HrEditor::~HrEditor()
{
    delete ui;
}


HrEditor::HrEditor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::HrEditor)
{
    ui->setupUi(this);

#ifdef Q_OS_WIN32
    ui->comboBox_hr->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
#endif
#ifdef Q_OS_MAC
    ui->comboBox_hr->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
#endif

    this->account = qApp->property("Account").value<Account*>();
    editingInBpm = false;



    setAutoFillBackground(true);
    setAttribute(Qt::WA_NoMousePropagation);
    setWindowFlags(Qt::WindowStaysOnTopHint);




    ui->gridLayout->setRowMinimumHeight(0,45);
    ui->gridLayout->setRowMinimumHeight(1,45);
}



//////////////////////////////////////////////////////////////////////////////////////////
void HrEditor::setInterval(const Interval &interval) {
    this->myInterval = interval;

    ui->comboBox_hr->setCurrentIndex(myInterval.getHRStepType());
        on_comboBox_hr_currentIndexChanged(ui->comboBox_hr->currentIndex());

    ui->spinBox_lthrStart->setValue(myInterval.getHR_start()*100);
    ui->spinBox_lthrEnd->setValue(myInterval.getHR_end()*100);
    ui->spinBox_rangeLthr->setValue(myInterval.getHR_range());


    qDebug() << "SEt interval Hr range is" <<  myInterval.getHR_range();

    //    if (myInterval.getHR_range() < 5 || myInterval.getHR_range() > 100 ) {
    //        ui->spinBox_rangeLthr->setValue(20);
    //        on_spinBox_rangeLthr_valueChanged(20);
    //    }
    //    else {
    //        ui->spinBox_rangeLthr->setValue(myInterval.getCadence_range());
    //        on_spinBox_rangeLthr_valueChanged(myInterval.getCadence_range());
    //    }



}




//////////////////////////////////////////////////////////////////////////////////////////
void HrEditor::on_comboBox_hr_currentIndexChanged(int index)
{
    int hrStepType = index;
    Interval::StepType typeStep = static_cast<Interval::StepType>(hrStepType);
    myInterval.setHrStepType(typeStep);

    MetricEditorVisibility::applyStepTypeVisibility(
        index,
        ui->spinBox_lthrStart,
        ui->label_toLthrTxt,
        ui->spinBox_lthrEnd,
        ui->comboBox_LTHRorBpm,
        ui->label_acceptedLthr,
        ui->spinBox_rangeLthr,
        ui->label_accptedBpmTxt);
}


//////////////////////////////////////////////////////////////////////////////////////////////
void HrEditor::on_spinBox_lthrStart_valueChanged(double arg1)
{

    // Convert to bpm value
    int lthr = account->LTHR;
    if (editingInBpm) {
        myInterval.setTargetHR_start(arg1/lthr);
    }
    else {
         myInterval.setTargetHR_start(arg1/100.0);
    }

}
//////////////////////////////////////////////////////////////////////////////////////////////
void HrEditor::on_spinBox_lthrEnd_valueChanged(double arg1)
{

    int lthr = account->LTHR;
    if (editingInBpm) {
        myInterval.setTargetHR_end(arg1/lthr);
    }
    else {
         myInterval.setTargetHR_end(arg1/100.0);
    }
}





void HrEditor::on_spinBox_rangeLthr_valueChanged(int arg1)
{
    qDebug() << "on_spinBox_rangeLthr_valueChanged" << arg1;
    myInterval.setTargetHR_range(arg1);
}

void HrEditor::on_pushButton_ok_clicked()
{
    emit endEdit();
}

void HrEditor::on_pushButton_default_clicked()
{
    this->myInterval.setHrStepType(Interval::StepType::NONE);
    this->myInterval.setTargetHR_start(0.5);
    this->myInterval.setTargetHR_end(0.5);
    this->myInterval.setTargetHR_range(15);
    emit endEdit();
}


//////////////////////////////////////////////////////////////////////////////////////////////
void HrEditor::on_comboBox_LTHRorBpm_currentIndexChanged(int index)
{

    if (index == 0) { ///% LTHR
        editingInBpm = false;
        ui->spinBox_lthrStart->setDecimals(2);
        ui->spinBox_lthrEnd->setDecimals(2);
        ui->spinBox_lthrStart->setMaximum(250);
        ui->spinBox_lthrEnd->setMaximum(250);
        on_spinBox_lthrStart_valueChanged(ui->spinBox_lthrStart->value());
        on_spinBox_lthrEnd_valueChanged(ui->spinBox_lthrEnd->value());

    }
    else { /// bpm
        editingInBpm = true;
        ui->spinBox_lthrStart->setDecimals(0);
        ui->spinBox_lthrEnd->setDecimals(0);
        ui->spinBox_lthrStart->setMaximum(300);
        ui->spinBox_lthrEnd->setMaximum(300);
        on_spinBox_lthrStart_valueChanged(ui->spinBox_lthrStart->value());
        on_spinBox_lthrEnd_valueChanged(ui->spinBox_lthrEnd->value());
    }

}
