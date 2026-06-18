#include "userstudiowidget.h"
#include "ui_userstudiowidget.h"

#include "myconstants.h"
#include "util.h"



UserStudioWidget::~UserStudioWidget()
{
    delete ui;
}


UserStudioWidget::UserStudioWidget(int userID, QString displayName, double FTP, double LTHR, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::UserStudioWidget)
{
    ui->setupUi(this);

    int size = 20; //20x20
    const qreal dpr = devicePixelRatioF();
    QPixmap pixmapPower   = Util::loadIconForDpr(":/image/icon/power2", size, dpr);
    QPixmap pixmapCadence = Util::loadIconForDpr(":/image/icon/crank2", size, dpr);
    QPixmap pixmapHr      = Util::loadIconForDpr(":/image/icon/heart2", size, dpr);
    QPixmap pixmapTarget  = Util::loadIconForDpr(":/image/icon/target", size - 5, dpr);


    ui->label_image_power->setPixmap(pixmapPower);
    ui->label_image_cadence->setPixmap(pixmapCadence);
    ui->label_image_hr->setPixmap(pixmapHr);
    ui->label_target->setPixmap(pixmapTarget);
    ui->label_target_2->setPixmap(pixmapTarget);
    ui->label_target_3->setPixmap(pixmapTarget);


    ui->label_image_power->setToolTip(tr("Power - watts"));
    ui->label_currentValuePower->setToolTip(tr("Power - watts"));
    ui->label_targetPowerValue->setToolTip(tr("Target Power - watts"));
    ui->label_target->setToolTip(tr("Target Power - watts"));
    ui->label_diffFromTargetPower->setToolTip(tr("Difference from Target Power - watts"));

    ui->label_image_cadence->setToolTip(tr("Cadence - rpm"));
    ui->label_currentValueCadence->setToolTip(tr("Cadence - rpm"));
    ui->label_targetCadenceValue->setToolTip(tr("Target Cadence - rpm"));
    ui->label_target_2->setToolTip(tr("Target Cadence - rpm"));
    ui->label_diffFromTargetCadence->setToolTip(tr("Difference from Target Cadence - rpm"));

    ui->label_image_hr->setToolTip(tr("Heart Rate - bpm"));
    ui->label_currentValueHr->setToolTip(tr("Heart Rate - bpm"));
    ui->label_targetHrValue->setToolTip(tr("Target Heart Rate - bpm"));
    ui->label_target_3->setToolTip(tr("Target Heart Rate - bpm"));
    ui->label_diffFromTargetHr->setToolTip(tr("Difference from Target Heart Rate - bpm"));


    this->userID = userID;
    this->displayName =  displayName;
    this->FTP = FTP;
    this->LTHR = LTHR;


    ui->label_userID->setText(QString::number(userID));
    QString displayNameToShow = displayName;
    if (FTP > 0) {
        displayNameToShow = displayNameToShow + " - " + QString::number(FTP) + " W";
    }
    ui->label_displayName->setText(displayNameToShow);


    isStopped = true;
    currentColorPower = 0;
    currentColorCad = 0;
    currentColorHr = 0;

    ui->frame_power->setStyleSheet(constants::stylesheetOK);
    ui->frame_cadence->setStyleSheet(constants::stylesheetOK);
    ui->frame_hr->setStyleSheet(constants::stylesheetOK);


}


//------------------------------------------------
void UserStudioWidget::setStopped(bool b) {
    this->isStopped = b;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setPowerSectionHidden() {

    ui->frame_power->setVisible(false);
}

void UserStudioWidget::setCadSectionHidden() {

    ui->frame_cadence->setVisible(false);
}

void UserStudioWidget::setHrSectionHidden() {

    ui->frame_hr->setVisible(false);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setPowerValue(int value) {

    if(value < 0) {
        ui->label_currentValuePower->setText("-");
        return;
    }


    ui->label_currentValuePower->setText(QString::number(value));

    if (FTP < 0) {
        return;
    }

    int diff = value - targetPower;
    QString diff_str;

    if (diff > 0) {
        diff_str = "+" + QString::number(diff);
    }
    else {
        diff_str = QString::number(diff);
    }

    if (diff != 0)
        ui->label_diffFromTargetPower->setText(diff_str);
    else {
        ui->label_diffFromTargetPower->setText("");
    }


    if (isStopped) {
        if (currentColorPower != 0) {
            ui->frame_power->setStyleSheet(constants::stylesheetOK);
            currentColorPower = 0;
        }
    }
    else {
        if (targetPower < 0) {
            ui->frame_power->setStyleSheet(constants::stylesheetOK);
            currentColorPower = 0;
        }
        else if ( (diff < (-targetPowerRange)) && (currentColorPower != -1) ) {
            ui->frame_power->setStyleSheet(constants::stylesheetTooLow);
            currentColorPower = -1;
        }
        else if( (diff > targetPowerRange) && (currentColorPower != 1) ) {
            ui->frame_power->setStyleSheet(constants::stylesheetTooHigh);
            currentColorPower = 1;
        }
        else if ( ((diff < targetPowerRange) && (diff > (-targetPowerRange))) && (currentColorPower != 0)) {
            ui->frame_power->setStyleSheet(constants::stylesheetOK);
            currentColorPower = 0;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setCadenceValue(int value) {

    if(value < 0) {
        ui->label_currentValueCadence->setText("-");
        return;
    }

    ui->label_currentValueCadence->setText(QString::number(value));

    int diff = value - targetCad;
    QString diff_str;

    if (diff > 0) {
        diff_str = "+" + QString::number(diff);
    }
    else {
        diff_str = QString::number(diff);
    }

    if (diff != 0)
        ui->label_diffFromTargetCadence->setText(diff_str);
    else {
        ui->label_diffFromTargetCadence->setText("");
    }

    if (isStopped) {
        if (currentColorCad != 0) {
            ui->frame_cadence->setStyleSheet(constants::stylesheetOK);
            currentColorCad = 0;
        }
    }
    else {
        if (targetCad < 0) {
            ui->frame_cadence->setStyleSheet(constants::stylesheetOK);
            currentColorCad = 0;
        }
        else if ( (diff < (-targetCadRange)) && (currentColorCad != -1) ) {
            ui->frame_cadence->setStyleSheet(constants::stylesheetTooLow);
            currentColorCad = -1;
        }
        else if( (diff > targetCadRange) && (currentColorCad != 1) ) {
            ui->frame_cadence->setStyleSheet(constants::stylesheetTooHigh);
            currentColorCad = 1;
        }
        else if ( ((diff < targetCadRange) && (diff > (-targetCadRange))) && (currentColorCad != 0)) {
            ui->frame_cadence->setStyleSheet(constants::stylesheetOK);
            currentColorCad = 0;
        }
    }
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setHrValue(int value) {

    if(value < 0) {
        ui->label_currentValueHr->setText("-");
        return;
    }

    ui->label_currentValueHr->setText(QString::number(value));

    if (LTHR < 0) {
        return;
    }

    int diff = value - targetHr;
    QString diff_str;

    if (diff > 0) {
        diff_str = "+" + QString::number(diff);
    }
    else {
        diff_str = QString::number(diff);
    }

    if (diff != 0)
        ui->label_diffFromTargetHr->setText(diff_str);
    else {
        ui->label_diffFromTargetHr->setText("");
    }

    if (isStopped) {
        if (currentColorHr != 0) {
            ui->frame_hr->setStyleSheet(constants::stylesheetOK);
            currentColorHr = 0;
        }
    }
    else {
        if (targetHr < 0) {
            ui->frame_hr->setStyleSheet(constants::stylesheetOK);
            currentColorHr = 0;
        }
        else if ( (diff < (-targetHrRange)) && (currentColorHr != -1) ) {
            ui->frame_hr->setStyleSheet(constants::stylesheetTooLow);
            currentColorHr = -1;
        }
        else if( (diff > targetHrRange) && (currentColorHr != 1) ) {
            ui->frame_hr->setStyleSheet(constants::stylesheetTooHigh);
            currentColorHr = 1;
        }
        else if ( ((diff < targetHrRange) && (diff > (-targetHrRange))) && (currentColorHr != 0)) {
            ui->frame_hr->setStyleSheet(constants::stylesheetOK);
            currentColorHr = 0;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setTargetPower(double percentageFTP, int range) {

//    qDebug() << "setTargetPower" << percentageFTP << "myFTP is:" << FTP;


    if (percentageFTP < 0 || FTP < 1) { //no target
        ui->label_targetPowerValue->setVisible(false);
        ui->label_diffFromTargetPower->setVisible(false);
        ui->label_target->setVisible(false);
        ui->label_spacer_power->setVisible(true);

        targetPower = -1;
    }
    else {
        ui->label_targetPowerValue->setVisible(true);
        ui->label_diffFromTargetPower->setVisible(true);
        ui->label_target->setVisible(true);
        ui->label_spacer_power->setVisible(false);

        targetPower = qRound(percentageFTP * FTP);
        ui->label_targetPowerValue->setText(QString::number(targetPower));
    }
    targetPowerRange = range;


}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setTargetCadence(int value, int range) {


    if (value < 0) { //no target
        ui->label_targetCadenceValue->setVisible(false);
        ui->label_diffFromTargetCadence->setVisible(false);
        ui->label_target_2->setVisible(false);
        ui->label_spacer_cad->setVisible(true);
    }
    else {
        ui->label_targetCadenceValue->setVisible(true);
        ui->label_diffFromTargetCadence->setVisible(true);
        ui->label_target_2->setVisible(true);
        ui->label_spacer_cad->setVisible(false);
    }

    targetCad = value;
    targetCadRange = range;

    ui->label_targetCadenceValue->setText(QString::number(targetCad));
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setTargetHr(double percentageLTHR, int range) {


    if (percentageLTHR < 0 || LTHR < 1) { //no target
        ui->label_targetHrValue->setVisible(false);
        ui->label_diffFromTargetHr->setVisible(false);
        ui->label_target_3->setVisible(false);
        ui->label_spacer_hr->setVisible(true);

        targetHr = -1;
    }
    else {
        ui->label_targetHrValue->setVisible(true);
        ui->label_diffFromTargetHr->setVisible(true);
        ui->label_target_3->setVisible(true);
        ui->label_spacer_hr->setVisible(false);

        targetHr = qRound(percentageLTHR * LTHR);
        ui->label_targetHrValue->setText(QString::number(targetHr));
    }

    targetHrRange = range;
}


//POWER
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setMaxIntervalPower(double val) {

    ui->label_maxIntervalPower->setText(QString::number(qRound(val)));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setMaxWorkoutPower(double val) {

    ui->label_maxWorkoutPower->setText(QString::number(qRound(val)));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setAvgIntervalPower(double val) {

    //    QString text =  locale.toString(val, 'f', 1);
    //    ui->label_avgIntervalPower->setText(text);

    ui->label_avgIntervalPower->setText(QString::number(qRound(val)));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setAvgWorkoutPower(double val) {

    //    QString text =  locale.toString(val, 'f', 1);
    //    ui->label_avgWorkoutPower->setText(text);

    ui->label_avgWorkoutPower->setText(QString::number(qRound(val)));
}

//CAD
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setMaxIntervalCad(double val) {

    ui->label_maxIntervalCadence->setText(QString::number(qRound(val)));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setMaxWorkoutCad(double val) {

    ui->label_maxWorkoutCadence->setText(QString::number(qRound(val)));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setAvgIntervalCad(double val) {

    //    QString text =  locale.toString(val, 'f', 1);
    //    ui->label_avgIntervalCadence->setText(text);

    ui->label_avgIntervalCadence->setText(QString::number(qRound(val)));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setAvgWorkoutCad(double val) {

    //    QString text =  locale.toString(val, 'f', 1);
    //    ui->label_avgWorkoutCadence->setText(text);

    ui->label_avgWorkoutCadence->setText(QString::number(qRound(val)));
}

//HR
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setMaxIntervalHr(double val) {

    ui->label_maxIntervalHr->setText(QString::number(qRound(val)));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setMaxWorkoutHr(double val) {

    ui->label_maxWorkoutHr->setText(QString::number(qRound(val)));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setAvgIntervalHr(double val) {

    //    QString text =  locale.toString(val, 'f', 1);
    //    ui->label_avgIntervalHr->setText(text);

    ui->label_avgIntervalHr->setText(QString::number(qRound(val)));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setAvgWorkoutHr(double val) {

    //    QString text =  locale.toString(val, 'f', 1);
    //    ui->label_avgWorkoutHr->setText(text);

    ui->label_avgWorkoutHr->setText(QString::number(qRound(val)));
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setNormalizedPower(double val) {

    ui->label_np->setText(QString::number((int)val)); //do not show decimals
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setIntensityFactor(double val) {

    QString text =  locale.toString(val, 'f', 2);
    ui->label_if->setText(text);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setTrainingStressScore(double val) {

    ui->label_tss->setText(QString::number((int)val)); //do not show decimals
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setCalories(double val) {

    ui->label_kcal->setText(QString::number((int)val)); //do not show decimals
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void UserStudioWidget::setResultTest(QString text) {

    ui->label_resultTest->setText(text);
    ui->label_resultTest->fadeIn(1000);
}
