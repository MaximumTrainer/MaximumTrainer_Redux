#include "balancewidget.h"
#include "ui_balancewidget.h"

#include "util.h"

BalanceWidget::~BalanceWidget()
{
    delete ui;
}


BalanceWidget::BalanceWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BalanceWidget)
{
    ui->setupUi(this);


    zoneTarget = 50;
    zoneRange = 3;
    setStopped(true);
    showMode = 1;
    ui->horizontalSlider->setMinimum(0);
    ui->horizontalSlider->setMaximum(100);

    // Flat look inside the workout metric card: muted caption, big L/R numbers.
    ui->groupBox_3->setTitle(tr("POWER BALANCE"));
    ui->horizontalSlider->hide();   // the card's live trend shows the balance
    setStyleSheet(QStringLiteral(
        "QGroupBox#groupBox_3 { border: none; margin-top: 16px; padding-top: 2px; }"
        "QGroupBox#groupBox_3::title { subcontrol-origin: margin; subcontrol-position: top left;"
        " left: 0px; padding: 0px; background: transparent; color: #9a9aa6; font-size: 8pt; }"
        "QLabel#label_powerLeft, QLabel#label_powerRight { font: 700 22pt; }"
        "QLabel#label_unit_left, QLabel#label_unit_right { color: #9a9aa6; border-radius: 3px; padding: 0 3px; }"
        "QFrame#frame { border: none; background: transparent; }"));


}


void BalanceWidget::setShowMode(int display) {
    this->showMode = display;
}


void BalanceWidget::setStopped(bool b) {
    this->isStopped = b;
}


void BalanceWidget::setValue(int value) {

    ui->horizontalSlider->setValue(value);

    ui->label_powerRight->setText(QString::number(value));
    ui->label_powerLeft->setText(QString::number(100-value));


    //    /// Check Value is outside of zone, change color of QLabel
    int diff = value - zoneTarget;
    int diffAbs = qAbs(diff);


    if (isStopped || zoneTarget == -1 || diffAbs <= zoneRange)
    {
        ui->label_unit_right->setStyleSheet("QLabel#label_unit_right { background-color :"+ Util::getColor(Util::DONE).name() + "; }");
        ui->label_unit_left->setStyleSheet("QLabel#label_unit_left { background-color :"+ Util::getColor(Util::DONE).name() + "; }");
    }

    else
    {
        /// Left is too strong is too strong (right too soft) inverse..
        if (diff > zoneRange) {
            ui->label_unit_right->setStyleSheet("QLabel#label_unit_right { background-color :"+ Util::getColor(Util::TOO_HIGH).name() + "; }");
            ui->label_unit_left->setStyleSheet("QLabel#label_unit_left { background-color :"+ Util::getColor(Util::TOO_LOW).name() + "; }");

        }
        /// Left is too soft   (diff < zoneRange*-1)
        else {
            ui->label_unit_right->setStyleSheet("QLabel#label_unit_right { background-color :"+ Util::getColor(Util::TOO_LOW).name() + "; }");
            ui->label_unit_left->setStyleSheet("QLabel#label_unit_left { background-color :"+ Util::getColor(Util::TOO_HIGH).name() + "; }");
        }
    }
}


void BalanceWidget::removeZone() {

    zoneRange = -1;
    zoneTarget = -1;

    ui->horizontalSlider->setStyleSheet("QSlider::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0.0 rgb(35, 35, 35), stop:1.0 rgb(35, 35, 35)); }");

}

void BalanceWidget::setZone(int target, int range) {


    if (target < range)
        target = range;
    if (target + range > 100)
        target = 100 - range;

    zoneRange = range;
    zoneTarget = target;

    int low = target - range + 0.01;
    int high = target + range - 0.01;

    double zoneLow = low / 100.0;
    double zoneHigh = high / 100.0;


    ui->horizontalSlider->setStyleSheet("QSlider::groove:horizontal { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                                        ///  0 to zMin
                                        "stop:0.0 " + Util::getColor(Util::DONE).name() + ", stop:"+ QString::number(zoneLow - 0.01) + " " +  Util::getColor(Util::DONE).name() +
                                        /// zoneTarget
                                        ", stop:" + QString::number(zoneLow) + " " +  Util::getColor(Util::SQUARE_POWER).name() + ", stop:" + QString::number(zoneHigh) + " " + Util::getColor(Util::SQUARE_POWER).name() +
                                        /// zMax to 1.0
                                        ", stop:" + QString::number(zoneHigh + 0.01) + " " + Util::getColor(Util::DONE).name() + ", stop:1.0 " +   Util::getColor(Util::DONE).name()  + "); }");

}

