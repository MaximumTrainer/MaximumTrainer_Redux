#include "cadenceeditor.h"
#include "ui_cadenceeditor.h"
#include "metriceditorvisibility.h"
#include <QDebug>

CadenceEditor::~CadenceEditor()
{
    delete ui;
}

CadenceEditor::CadenceEditor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CadenceEditor)
{

    ui->setupUi(this);




#ifdef Q_OS_WIN32
    ui->comboBox_cadence->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
#endif
#ifdef Q_OS_MAC
    ui->comboBox_cadence->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
#endif



    setAutoFillBackground(true);
    setAttribute(Qt::WA_NoMousePropagation);
    setWindowFlags(Qt::WindowStaysOnTopHint);



    ui->gridLayout->setRowMinimumHeight(0,45);
    ui->gridLayout->setRowMinimumHeight(1,45);

    MetricEditorVisibility::applyNativeOkCancelButtons(ui->pushButton_ok, ui->pushButton_default);
}



//////////////////////////////////////////////////////////////////////////////////////////
void CadenceEditor::setInterval(const Interval &interval) {
    this->myInterval = interval;

    qDebug() << "CadenceStart is now" << myInterval.getCadence_start();

    ui->comboBox_cadence->setCurrentIndex(myInterval.getCadenceStepType());
    on_comboBox_cadence_currentIndexChanged(ui->comboBox_cadence->currentIndex());

    ui->spinBox_cadenceStart->setValue(myInterval.getCadence_start());
    ui->spinBox_cadenceEnd->setValue(myInterval.getCadence_end());
    ui->spinBox_rangeCadence->setValue(myInterval.getCadence_range());

}

/////////////////////////////////////////////////////////////////////////////////
void CadenceEditor::on_comboBox_cadence_currentIndexChanged(int index)
{
    Interval::StepType typeStep = static_cast<Interval::StepType>(index);
    myInterval.setCadenceStepType(typeStep);

    MetricEditorVisibility::applyStepTypeVisibility(
        index,
        ui->spinBox_cadenceStart,
        ui->label_toCadenceTxt,
        ui->spinBox_cadenceEnd,
        ui->label_cadenceRpmTxt,
        ui->label_acceptedCadence,
        ui->spinBox_rangeCadence,
        ui->label_accptedRpmTxt);
}


void CadenceEditor::on_spinBox_cadenceStart_valueChanged(int arg1)
{
    myInterval.setTargetCadence_start(arg1);
}

void CadenceEditor::on_spinBox_cadenceEnd_valueChanged(int arg1)
{
    myInterval.setTargetCadence_end(arg1);
}

void CadenceEditor::on_spinBox_rangeCadence_valueChanged(int arg1)
{
    qDebug() << "on_spinBox_rangeCadence_valueChanged" << arg1;
    myInterval.setTargetCadence_range(arg1);
}

void CadenceEditor::on_pushButton_ok_clicked()
{
    emit endEdit();
}

void CadenceEditor::on_pushButton_default_clicked()
{
    emit cancelEdit();
}
