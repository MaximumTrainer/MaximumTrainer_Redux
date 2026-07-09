#include "infowidget.h"
#include "ui_infowidget.h"

#include <QDebug>
#include "myconstants.h"
#include "util.h"



InfoWidget::~InfoWidget() {
    delete ui;
}



InfoWidget::InfoWidget(QWidget *parent) : QWidget(parent), ui(new Ui::InfoWidget) {
    ui->setupUi(this);

    // Typography scale (font rules only — the target-state frame colours and
    // the light/dark palette keep coming from the theme): hero live value,
    // medium secondary stats, small muted captions.
    setStyleSheet(QStringLiteral(R"(
        QLabel#label_currentValue { font: 600 25pt 'Inter'; }
        QLabel#label_targetValue, QLabel#label_range { font: 15pt 'Inter'; }
        QLabel#label_avgInterval, QLabel#label_maxInterval,
        QLabel#label_avgWorkout,  QLabel#label_maxWorkout { font: 600 13pt 'Inter'; }
        QLabel#label_WorkoutTxt, QLabel#label_AvgIntervalTxt,
        QLabel#label_AverageTxt, QLabel#label_MaximumTxt,
        QLabel#label_trainerSpeedTxt { font: 600 8pt 'Inter'; color: #8d8d8d; }
    )"));

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

    // Rowing (Beta): the speed channel carries the pace-equivalent km/h from
    // the rower; show it as time per 500 m instead of a speed.
    if (rowingMode && type == InfoWidget::SPEED) {
        if (v <= 0.5) {
            ui->label_currentValue->setText(" -");
            return;
        }
        const int paceSec = qRound(1800.0 / v);
        ui->label_currentValue->setText(QStringLiteral(" %1:%2")
            .arg(paceSec / 60)
            .arg(paceSec % 60, 2, 10, QLatin1Char('0')));
        return;
    }

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
    const qreal dpr = devicePixelRatioF();
    QPixmap pixmapPower   = Util::loadIconForDpr(":/image/icon/power2", size, dpr);
    QPixmap pixmapCadence = Util::loadIconForDpr(":/image/icon/crank2", size, dpr);
    QPixmap pixmapSpeed   = Util::loadIconForDpr(":/image/icon/speed",  size, dpr);
    QPixmap pixmapHr      = Util::loadIconForDpr(":/image/icon/heart2", size, dpr);
    QPixmap pixmapTarget  = Util::loadIconForDpr(":/image/icon/target", size - 10, dpr);


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
void InfoWidget::setRowingMode(bool rowing) {
    rowingMode = rowing;
    if (!rowing)
        return;
    // Rowing (Beta): the cadence channel carries stroke rate and the speed
    // channel carries the pace equivalent — relabel the tooltips to match.
    if (type == InfoWidget::CADENCE) {
        ui->label_currentValue->setToolTip(tr("Stroke Rate - spm"));
        ui->label_targetValue->setToolTip(tr("Target Stroke Rate - spm"));
        ui->label_range->setToolTip(tr("Difference from Target Stroke Rate - spm"));
        ui->label_image->setToolTip(tr("Stroke Rate - spm"));
    }
    else if (type == InfoWidget::SPEED) {
        ui->label_currentValue->setToolTip(tr("Pace - per 500 m"));
        ui->label_image->setToolTip(tr("Pace - per 500 m"));
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

