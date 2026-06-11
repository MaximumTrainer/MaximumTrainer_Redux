#include "timewidget.h"
#include "ui_timewidget.h"

#include "util.h"




TimeWidget::~TimeWidget()
{
    delete ui;
}




TimeWidget::TimeWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TimeWidget)
{
    ui->setupUi(this);

    hasTargetPower = false;
    hasTargetCad = false;
    hasTargetHr= false;

    ui->label_imagePower->setStyleSheet("image: url(:/image/icon/power2)");
    ui->label_imagePower->setToolTip(tr("Power - %FTP"));
    ui->label_targetPower->setToolTip(tr("Power - %FTP"));

    //    ui->label_imagePower->setPixmap();

    ui->label_imageCad->setStyleSheet("image: url(:/image/icon/crank2)");
    ui->label_imageCad->setToolTip(tr("Cadence - rpm"));
    ui->label_targetCad->setToolTip(tr("Cadence - rpm"));

    ui->label_imageHr->setStyleSheet("image: url(:/image/icon/heart2)");
    ui->label_imageHr->setToolTip(tr("Heart Rate - %LTHR"));
    ui->label_targetHr->setToolTip(tr("Heart Rate - %LTHR"));

    //Do not show labels, not needed, show tooltip instead
    ui->label_timeInterval_txt->setVisible(false);
    ui->label_timeWorkoutRemaining_txt->setVisible(false);
    ui->label_timeWorkoutElaps_txt->setVisible(false);

    ui->label_intervalTime->setToolTip(tr("Interval Remaining"));
    ui->label_timeElapsed->setToolTip(tr("Elapsed time"));
    ui->label_remainingTime->setToolTip(tr("Workout Remaining"));


}



//////////////////////////////////////////////////////////////////////////////////////////////////
void TimeWidget::setTargetPower(double percentageFTP, int range) {

    if (percentageFTP > 0) {
        ui->label_targetPower->setText(QString::number(percentageFTP*100, 'f', 1) + "%");
        ui->label_rangePower->setText(" ±" + QString::number(range));
        ui->label_imagePower->setVisible(true);
        ui->label_targetPower->setVisible(true);
        ui->label_rangePower->setVisible(true);
        hasTargetPower = true;
    }
    else {
        ui->label_imagePower->setVisible(false);
        ui->label_targetPower->setVisible(false);
        ui->label_rangePower->setVisible(false);
        hasTargetPower = false;
    }

    if (showTargetEnabled && (hasTargetPower || hasTargetCad || hasTargetHr)) {
        ui->groupBox->setVisible(true);
    }
    else {
        ui->groupBox->setVisible(false);
    }


}


//////////////////////////////////////////////////////////////////////////////////////////////////
void TimeWidget::setTargetCadence(int cadence, int range) {

    if (cadence > 0) {
        ui->label_targetCad->setText(QString::number(cadence));
        ui->label_rangeCad->setText(" ±" + QString::number(range));
        ui->label_imageCad->setVisible(true);
        ui->label_targetCad->setVisible(true);
        ui->label_rangeCad->setVisible(true);
        hasTargetCad = true;
    }
    else {
        ui->label_imageCad->setVisible(false);
        ui->label_targetCad->setVisible(false);
        ui->label_rangeCad->setVisible(false);
        hasTargetCad = false;
    }

    if (showTargetEnabled && (hasTargetPower || hasTargetCad || hasTargetHr)) {
        ui->groupBox->setVisible(true);
    }
    else {
        ui->groupBox->setVisible(false);
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////
void TimeWidget::setTargetHeartRate(double percentageLTHR, int range) {

    if (percentageLTHR > 0) {
        ui->label_targetHr->setText(QString::number(percentageLTHR*100, 'f', 1) + "%");
        ui->label_rangeHr->setText(" ±" + QString::number(range));
        ui->label_imageHr->setVisible(true);
        ui->label_targetHr->setVisible(true);
        ui->label_rangeHr->setVisible(true);
        hasTargetHr = true;

    }
    else {
        ui->label_imageHr->setVisible(false);
        ui->label_targetHr->setVisible(false);
        ui->label_rangeHr->setVisible(false);
        hasTargetHr = false;
    }

    if (showTargetEnabled && (hasTargetPower || hasTargetCad || hasTargetHr)) {
        ui->groupBox->setVisible(true);
    }
    else {
        ui->groupBox->setVisible(false);
    }

}

//-----------------------------------------------------
void TimeWidget::showIntervalRemaining(bool show) {

    ui->label_intervalTime->setVisible(show);
    //    ui->label_timeInterval_txt->setVisible(show);
}

void TimeWidget::showWorkoutRemaining(bool show) {

    ui->label_remainingTime->setVisible(show);
    //    ui->label_timeWorkoutRemaining_txt->setVisible(show);
}

void TimeWidget::showWorkoutElapsed(bool show) {

    ui->label_timeElapsed->setVisible(show);
    //    ui->label_timeWorkoutElaps_txt->setVisible(show);

}

void TimeWidget::showCurrentTarget(bool show) {

    showTargetEnabled = show;
    ui->groupBox->setVisible(show && (hasTargetPower || hasTargetCad || hasTargetHr));
}



//-----------------------------------------------------
void TimeWidget::setFreeRideMode() {

    //    ui->label_timeInterval_txt->setText(tr("Elapsed Interval"));
    ui->label_intervalTime->setToolTip(tr("Elapsed Interval"));

    ui->label_remainingTime->setVisible(false);
    ui->label_timeWorkoutRemaining_txt->setVisible(false);

    ui->groupBox->setVisible(false);



}

void TimeWidget::setMAPMode() {

    ui->label_remainingTime->setVisible(false);
    ui->label_timeWorkoutRemaining_txt->setVisible(false);
}


//-----------------------------------------------------
void TimeWidget::setWorkoutTime(QTime time) {

    ui->label_timeElapsed->setText(Util::showQTimeAsString(time));
}

//-----------------------------------------------------
void TimeWidget::setWorkoutRemainingTime(QTime time) {

    ui->label_remainingTime->setText(Util::showQTimeAsString(time));
}

//-----------------------------------------------------
void TimeWidget::setIntervalTime(QTime time) {

    ui->label_intervalTime->setText(Util::showQTimeAsString(time));
}

