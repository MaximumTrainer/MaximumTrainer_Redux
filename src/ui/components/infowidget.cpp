#include "infowidget.h"
#include "ui_infowidget.h"

#include <QDebug>
#include "myconstants.h"



InfoWidget::~InfoWidget() {
    delete ui;
}



InfoWidget::InfoWidget(QWidget *parent) : QWidget(parent), ui(new Ui::InfoWidget) {
    ui->setupUi(this);


    useMile = false;

    target = -1;
    ui->label_currentValue->setText(" 0");
    ui->label_range->setText("");

    ui->label_range->setVisible(false);
    ui->label_targetValue->setVisible(false);
    ui->label_imageTarget->setVisible(false);
    ui->label_trainerSpeedTxt->setVisible(false);


    isStopped = true;
    currentColor = 0;

    ui->frame->setStyleSheet(constants::stylesheetOK);
}


//void InfoWidget::setFreeRideMode() {

//    ui->label_AvgIntervalTxt->setVisible(false);
//    ui->label_avgInterval->setVisible(false);
//    ui->label_maxInterval->setVisible(false);

//    ui->label_WorkoutTxt->setVisible(false);
//    ui->label_range->setVisible(false);


//    ///Move Icon and value 1 column to the right
//    QLabel *labelIcon = ui->label_image;
//    QLabel *labelValue = ui->label_currentValue;
//    ui->gridLayout->addWidget(labelIcon,0,3,1,1, Qt::AlignLeft);
//    ui->gridLayout->addWidget(labelValue,0,4,1,1, Qt::AlignLeft);

//}


//////////////////////////////////////////////////////////////
void InfoWidget::setUserData(double FTP, double LTHR) {
    this->FTP = FTP;
    this->LTHR = LTHR;
}


///Only for speed widget
//////////////////////////////////////////////////////////////////////////////////////////////////
void InfoWidget::setValue(double v) {



    if (v == 0) {
        ui->label_currentValue->setText(" 0");
    }
    else if(v < 0) {
        ui->label_currentValue->setText(" -");
    }
    else {
        if (useMile)
            v = v* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;

        ui->label_currentValue->setText(" " + QString::number(v, 'f', 1));
    }
}

///Only for speed widget - trainer Speed in M/S
/// //////////////////////////////////////////////////////////////////////////////////////////////////
void InfoWidget::setTrainerSpeed(double v) {

    if (v == 0) {
        ui->label_trainerSpeed->setText("0");
    }
    else if(v < 0) {
        ui->label_trainerSpeed->setText("-");
    }
    else {
        if (useMile)
            v = v* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;

        ui->label_trainerSpeed->setText("" + QString::number(v, 'f', 1));


//        ui->label_trainerSpeedTxt->setText(" " + QString::number(v, 'f', 1));
    }

}



//////////////////////////////////////////////////////////////////////////////////////////////////
void InfoWidget::setValue( int value ) {


    if(value < 0) {
        ui->label_currentValue->setText(" -");
        return;
    }

    ui->label_currentValue->setText(" " + QString::number(value));


    int diff = value - target;
    QString diff_str;

    if (diff > 0) {
        diff_str = "+" + QString::number(diff);
    }
    else {
        diff_str = QString::number(diff);
    }

    if (diff != 0)
        ui->label_range->setText(diff_str);
    else {
        ui->label_range->setText("");
    }


    if (isStopped) {
        if (currentColor != 0) {
            ui->frame->setStyleSheet(constants::stylesheetOK);
            currentColor = 0;
        }
    }
    else {
        if (target < 0) {
            ui->frame->setStyleSheet(constants::stylesheetOK);
            currentColor = 0;
        }
        else if ( (diff < (-targetRange)) && (currentColor != -1) ) {
            ui->frame->setStyleSheet(constants::stylesheetTooLow);
            currentColor = -1;
        }
        else if( (diff > targetRange) && (currentColor != 1) ) {
            ui->frame->setStyleSheet(constants::stylesheetTooHigh);
            currentColor = 1;
        }
        else if ( ((diff < targetRange) && (diff > (-targetRange))) && (currentColor != 0)) {
            ui->frame->setStyleSheet(constants::stylesheetOK);
            currentColor = 0;
        }
    }

}


//////////////////////////////////////////////////////////////////////////////////////////////////
void InfoWidget::targetChanged(double percentageValue, int range) {

    //    qDebug() << "infoBox: targetChanged" << target << "r:" << range;


    if (percentageValue < 0) { //no target
        ui->label_range->setVisible(false);
        ui->label_targetValue->setVisible(false);
        ui->label_imageTarget->setVisible(false);
    }
    else {
        ui->label_range->setVisible(true);
        ui->label_targetValue->setVisible(true);
        ui->label_imageTarget->setVisible(true);
    }


    int realTarget = percentageValue;
    if (type == InfoWidget::POWER) {
        realTarget = qRound(percentageValue * FTP);
    }
    else if (type == InfoWidget::HEART_RATE) {
        realTarget = qRound(percentageValue * LTHR);
    }

    //    ui->label_targetValue->setText(QString::number(target) + " (±" + QString::number(range)  + ")");
    ui->label_targetValue->setText(QString::number(realTarget));


    this->target = realTarget;
    this->targetRange = range;
}

///////////////////////////////////////////////////////////////////////////////////////////////

void InfoWidget::maxIntervalChanged(double avg) {

    if (type == InfoWidget::SPEED) {

        if (useMile) {
            avg = avg* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;
        }

        QString text =  locale.toString(avg, 'f', 1);
        ui->label_maxInterval->setText(text);
    }
    else {
        ui->label_maxInterval->setText(QString::number(qRound(avg)));
    }

}
//----------------------------------------------------
void InfoWidget::maxWorkoutChanged(double avg) {

    if (type == InfoWidget::SPEED) {

        if (useMile)
            avg = avg* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;

        QString text =  locale.toString(avg, 'f', 1);
        ui->label_maxWorkout->setText(text);
    }
    else {
        ui->label_maxWorkout->setText(QString::number(qRound(avg)));
    }
}
//----------------------------------------------------
void InfoWidget::avgIntervalChanged(double avg) {

    if (useMile)
        avg = avg* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;

    QString text =  locale.toString(avg, 'f', 1);
    ui->label_avgInterval->setText(text);

}
//----------------------------------------------------
void InfoWidget::avgWorkoutChanged(double avg) {

    if (useMile)
        avg = avg* constants::GLOBAL_CONST_CONVERT_KMH_TO_MILES;

    QString text =  locale.toString(avg, 'f', 1);
    ui->label_avgWorkout->setText(text);
}





///////////////////////////////////////////////////////////////////////////////////////////////
void InfoWidget::setTypeInfoBox(TypeInfoBox type) {


    this->type = type;

    int size = 35;
    QPixmap pixmapPower = QPixmap(":/image/icon/power2");
    QPixmap pixmapCadence = QPixmap(":/image/icon/crank2");
    QPixmap pixmapSpeed = QPixmap(":/image/icon/speed");
    QPixmap pixmapHr = QPixmap(":/image/icon/heart2");
    QPixmap pixmapTarget = QPixmap(":/image/icon/target");

    pixmapPower = pixmapPower.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmapCadence = pixmapCadence.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmapSpeed = pixmapSpeed.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmapHr = pixmapHr.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmapTarget = pixmapTarget.scaled(size-10, size-10, Qt::KeepAspectRatio, Qt::SmoothTransformation);


    ui->label_imageTarget->setPixmap(pixmapTarget);
    ui->label_imageTarget->setToolTip(tr("Target"));
    //    ui->label_imageTarget->setP margin left

    if (type == InfoWidget::HEART_RATE) {
        ui->label_currentValue->setToolTip(tr("Heart Rate - bpm"));
        ui->label_targetValue->setToolTip(tr("Target Heart Rate - bpm"));
        ui->label_range->setToolTip(tr("Difference from Target Heart Rate - bpm"));

        ui->label_image->setPixmap(pixmapHr);
        ui->label_image->setToolTip(tr("Heart Rate - bpm"));
    }
    else if (type == InfoWidget::SPEED) {
        ui->label_image->setStyleSheet("image: url(:/image/icon/speed)");
        ui->label_image->setToolTip(tr("Speed - km/h"));
        ui->label_currentValue->setToolTip(tr("Speed - km/h"));

        ui->label_image->setPixmap(pixmapSpeed);
        ui->label_image->setToolTip(tr("Speed - km/h"));

        ui->label_trainerSpeedTxt->setVisible(true);
    }
    else if (type == InfoWidget::CADENCE) {
        ui->label_currentValue->setToolTip(tr("Cadence - rpm"));
        ui->label_targetValue->setToolTip(tr("Target Cadence - rpm"));
        ui->label_range->setToolTip(tr("Difference from Target Cadence - rpm"));

        ui->label_image->setPixmap(pixmapCadence);
        ui->label_image->setToolTip(tr("Cadence - rpm"));
    }
    else if (type == InfoWidget::POWER) {
        ui->label_currentValue->setToolTip(tr("Power - watts"));
        ui->label_targetValue->setToolTip(tr("Target Power - watts"));
        ui->label_range->setToolTip(tr("Difference from Target Power - watts"));

        ui->label_image->setPixmap(pixmapPower);
        ui->label_image->setToolTip(tr("Power - watts"));
    }

}


///////////////////////////////////////////////////////////////////////////////////////////////
void InfoWidget::setStopped(bool b) {
    isStopped = b;
}

void InfoWidget::setUseMiles(bool b) {
    useMile = b;
    ui->label_image->setStyleSheet("image: url(:/image/icon/speed)");
    ui->label_image->setToolTip(tr("Speed - mph"));
    ui->label_currentValue->setToolTip(tr("Speed - mph"));
}

void InfoWidget::setTrainerSpeedVisible(bool b) {
    ui->label_trainerSpeed->setVisible(b);
    ui->label_trainerSpeedTxt->setVisible(b);
}

