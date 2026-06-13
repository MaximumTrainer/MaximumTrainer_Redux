#include "minimalistwidget.h"
#include "ui_minimalistwidget.h"

#include <QDebug>
#include "util.h"


MinimalistWidget::~MinimalistWidget()
{
    delete ui;
}




MinimalistWidget::MinimalistWidget(QWidget *parent) : QWidget(parent), ui(new Ui::MinimalistWidget)
{
    ui->setupUi(this);

    // Hero value, consistent with the InfoWidget type scale.
    setStyleSheet(QStringLiteral("QLabel#label_value { font: 600 24pt 'Inter'; }"));

    target = -1;
    setStopped(true);

}



///////////////////////////////////////////////////////////////////////////////////////////////
void MinimalistWidget::setStopped(bool b) {
    isStopped = b;
}


///////////////////////////////////////////////////////////////////////////////////////////////
void MinimalistWidget::setTarget(double percentageTarget, int range) {




    if (percentageTarget < 0) {
        ui->horizontalSlider->setVisible(false);
    }
    else {
        ui->horizontalSlider->setVisible(true);
    }


    int realTarget = percentageTarget;
    if (type == MinimalistWidget::POWER) {
        realTarget = qRound(percentageTarget * FTP);
    }
    else if (type == MinimalistWidget::HEART_RATE) {
        realTarget = qRound(percentageTarget * LTHR);
    }


    this->target = realTarget;
    this->range = range;



    low = target - range - extraAfterRange;
    high = target + range + extraAfterRange;


    /// Low = 0
    /// High = 1
    /// Trouver inconnu zMin et zMax (borne de la zone)
    double zMin = low + extraAfterRange;
    double zMax = high - extraAfterRange;

    double zMin_01 = (zMin - low) / (double)(high - low);
    double zTarget_01 = (target - low) / (double) (high - low);
    double zMax_01 = (zMax - low) / (double)(high - low);


    //    qDebug() << "Low :" << low << "high :" << high;
    //    qDebug() << "zMin_" << zMin_01 << "ZMax_" << zMax_01;



    // Out-of-zone groove ends use a dark neutral (the shared Util::DONE light
    // colour glares on the dark workout background); the groove and the
    // current-value handle get a rounded, slimmer profile.
    const QString grooveEnds = QStringLiteral("#3a3a3a");
    ui->horizontalSlider->setStyleSheet("QSlider::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                                        ///  0 to zMin
                                        "stop:0.0 " + grooveEnds + ", stop:"+ QString::number(zMin_01 - 0.01) + " " +  grooveEnds +
                                        /// zoneBeforeTarget
                                        ", stop:" + QString::number(zMin_01) + " " +  colorSquare.name() + ", stop:" + QString::number(zTarget_01 - 0.01) + " " + colorSquare.name() +
                                        /// Target line
                                        ", stop:" + QString::number(zTarget_01) + " " +  Util::getColor(Util::LINE_ON_TARGET_GRAPH).name() + ", stop:" + QString::number(zTarget_01) + " " + Util::getColor(Util::LINE_ON_TARGET_GRAPH).name() +
                                        /// zoneAfterTarget
                                        ", stop:" + QString::number(zTarget_01 + 0.01) + " " +  colorSquare.name() + ", stop:" + QString::number(zMax_01) + " " + colorSquare.name() +
                                        /// zMax to 1.0
                                        ", stop:" + QString::number(zMax_01 + 0.01) + " " + grooveEnds + ", stop:1.0 " + grooveEnds + ");"
                                        " height: 10px; border-radius: 5px; border: 1px solid #2c2c2c; }"
                                        " QSlider::handle:horizontal { background: #ffd54f; width: 6px;"
                                        " border-radius: 3px; margin: -3px 0; }");

}




///////////////////////////////////////////////////////////////////////////////////////////////
void MinimalistWidget::setValue(int value) {

    if (target < 0) {
        ui->horizontalSlider->setVisible(false);
        applyFrameColor(Util::DONE);
        ui->label_value->setText(QString::number(value));
        return;
    }

    if (ui->horizontalSlider->minimum() != 0 && ui->horizontalSlider->maximum() != 1000) {
        ui->horizontalSlider->setMinimum(0);
        ui->horizontalSlider->setMaximum(1000);
    }


    if(value < 0) {
        ui->label_value->setText("-");
        ui->horizontalSlider->setValue(0);
        return;
    }


    /// Convert value to 0:1 format
    double value_01 = (value - low) / (double)(high - low);
    /// Convert value to 0 - 1000 format
    double value_0mille = value_01 * 1000;
    int value_0mille_i = (int) value_0mille;


    //    qDebug() << "******OK VALUE 0_1 is" << value_01;
    //    qDebug() << "VALUE 0MILLE is" << value_0mille << "Int:" << value_0mille_i;


    ui->label_value->setText(QString::number(value));
    ui->horizontalSlider->setValue(value_0mille_i);


    /// Check Value is outside of zone, change color of QLabel
    int diff = value - target;
    int diffAbs = qAbs(diff);


    //    qDebug() << "is stopped?" << isStopped;
    //    qDebug() << "zone Target?" << target;
    //    qDebug() << "diff is" << diff << "zoneRange is " << range;

    if (isStopped || diffAbs <= range)
    {
        applyFrameColor(Util::DONE);
    }
    else
    {
        /// Too Strong
        if (diff > range) {
            applyFrameColor(Util::TOO_HIGH);
        }
        /// Too Soft
        else {
            applyFrameColor(Util::TOO_LOW);
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// setValue() runs on every sensor packet; setStyleSheet() forces a full style
// recompute. Skip it when the frame colour has not actually changed.
void MinimalistWidget::applyFrameColor(Util::Color colorId) {

    if (currentColor == colorId)
        return;
    currentColor = colorId;

    ui->frame->setStyleSheet("QFrame#frame { background-color :"
                             + Util::getColor(colorId).name() + "; }");
}



//////////////////////////////////////////////////////////////
void MinimalistWidget::setUserData(double FTP, double LTHR) {
    this->FTP = FTP;
    this->LTHR = LTHR;
}




///////////////////////////////////////////////////////////////////////////////////////////////
void MinimalistWidget::setTypeWidget(TypeMinimalist type) {


    this->type = type;

    if (type == MinimalistWidget::HEART_RATE) {
        ui->label_image->setStyleSheet("image: url(:/image/icon/heart2)");
        ui->label_image->setToolTip(tr("Heart Rate - bpm"));
        colorSquare = Util::getColor(Util::SQUARE_HEARTRATE);
        //        ui->frame->setStyleSheet("QSlider::handle:horizontal { background:"+ Util::getColor(Util::LINE_HEARTRATE).name() + "; }");
        extraAfterRange = 10;
    }
    else if (type == MinimalistWidget::SPEED) {
        ui->label_image->setStyleSheet("image: url(:/image/icon/speed)");
        ui->label_image->setToolTip(tr("Speed - km/h"));
        //        colorSquare = Util::getColor(Util::SQUARE_POWER);
        //                 ui->horizontalLayout->setStyleSheet("QSlider::handle:horizontal { background:"+ Util::getColor(Util::LINE_CADENCE).name() + "; }");
        //        extraAfterRange = 5;
    }
    else if (type == MinimalistWidget::CADENCE) {
        ui->label_image->setStyleSheet("image: url(:/image/icon/crank2)");
        ui->label_image->setToolTip(tr("Cadence - rpm"));
        colorSquare = Util::getColor(Util::SQUARE_CADENCE);
        //        ui->horizontalSlider->setStyleSheet("QSlider::handle:horizontal { background:"+ Util::getColor(Util::TOO_HIGH).name() + "; }");
        extraAfterRange = 5;
    }
    else if (type == MinimalistWidget::POWER) {
        ui->label_image->setStyleSheet("image: url(:/image/icon/power2)");
        ui->label_image->setToolTip(tr("Power - watts"));
        colorSquare = Util::getColor(Util::SQUARE_POWER);
        //        ui->frame->setStyleSheet("QSlider::handle:horizontal { background:"+ Util::getColor(Util::LINE_POWER).name() + "; }");
        extraAfterRange = 15;
    }
}

