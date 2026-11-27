#include "bottombar.h"

#include <QBoxLayout>
#include "ui_bottombar.h"

#include <QSizeGrip>
#include <QTimer>
#include <QMenuBar>

#include "util.h"
#include "account.h"


BottomBar::~BottomBar()
{
    delete ui;
}


BottomBar::BottomBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BottomBar)
{
    ui->setupUi(this);


    ui->label_circle->setVisible(false);

}


void BottomBar::setGeneralMessage(const QString& text, int timeToDisplay) {

    ui->label_generalMsg->setText( text );
    ui->label_generalMsg->show();
    ui->label_generalMsg->fadeInAndFadeOutAfterPause(400, 1000, timeToDisplay);
}
void BottomBar::setGeneralMessage(const QString& text) {

    ui->label_generalMsg->setText( text );
    ui->label_generalMsg->show();
    ui->label_generalMsg->fadeIn(300);


}
void BottomBar::removeGeneralMessage() {

    ui->label_generalMsg->fadeOut(500);
}



void BottomBar::setStatusWidget(QWidget *widget)
{
    if (auto *row = qobject_cast<QBoxLayout *>(layout()))
        row->insertWidget(0, widget, 0, Qt::AlignLeft | Qt::AlignVCenter);
}
