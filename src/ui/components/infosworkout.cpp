#include "infosworkout.h"
#include "ui_infosworkout.h"

infosWorkout::infosWorkout(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::infosWorkout)
{
    ui->setupUi(this);

    // Same typography scale as InfoWidget: values readable at a glance,
    // captions small and muted.
    setStyleSheet(QStringLiteral(R"(
        QLabel#label_np, QLabel#label_if, QLabel#label_tss,
        QLabel#label_kcal, QLabel#label_distance { font: 600 13pt 'Inter'; }
        QLabel#label_4, QLabel#label_44, QLabel#label_2,
        QLabel#label_distanceText { font: 600 8pt 'Inter'; color: #8d8d8d; }
    )"));

    inMile = false; //km by default
    isStopped = true;

}

infosWorkout::~infosWorkout()
{
    delete ui;
}

//ui->label_image->setStyleSheet("image: url(:/image/icon/burn)");
//ui->label_image->setToolTip(tr("Calories - kilocalorie"));
//ui->label_image->setMaximumWidth(32);

//else if(this->type == InfoWidget::CALORIES) {
//    ui->label_currentValue->setText(QString::number((int)v)); //do not show decimals
//}



//------------------------------------------------------
void infosWorkout::setStopped(bool b) {

    isStopped = b;
}


void infosWorkout::setDistanceInMile(bool b) {

    inMile = b;
    ui->label_distanceText->setToolTip(tr("Miles"));
}


//------------------------------------------------------
void infosWorkout::setDistanceVisible(bool b) {

    ui->label_distance->setVisible(b);
    ui->label_distanceText->setVisible(b);

}

//--------------------------------------------------
void infosWorkout::NP_Changed(double val) {


    ui->label_np->setText(QString::number((int)val)); //do not show decimals

}


//--------------------------------------------------
void infosWorkout::IF_Changed(double val) {

    ui->label_if->setText(locale.toString(val, 'f', 2));

}

//--------------------------------------------------
void infosWorkout::distanceChanged(double meters) {

    if (inMile) {
        ui->label_distance->setText(locale.toString(meters*0.000621371, 'f', 3));
    }
    else {
         ui->label_distance->setText(locale.toString(meters/1000, 'f', 3));
    }

}



//--------------------------------------------------
void infosWorkout::TSS_Changed(double val) {


    ui->label_tss->setText(QString::number((int)val)); //do not show decimals
}


//--------------------------------------------------
void infosWorkout::calories_Changed(double val) {

    ui->label_kcal->setText(QString::number((int)val)); //do not show decimals
}
