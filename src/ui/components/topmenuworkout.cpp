#include "topmenuworkout.h"
#include "ui_topmenuworkout.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include <QKeyEvent>
#include <QPainter>
#include <QStyle>
#include "util.h"

namespace {
QIcon tintedStandardIcon(QStyle *style, QStyle::StandardPixmap sp,
                         const QSize& size, const QColor& color) {
    // Qt's title-bar standard pixmaps have differing native sizes (e.g. min and
    // close are ~11x13 while max is 16x16) and pixmap(size) does not upscale.
    // Scale the source up to the requested size so the toolbar icons render at
    // a uniform visual size.
    QPixmap source = style->standardIcon(sp).pixmap(size);
    if (source.size() != size)
        source = source.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap out(size);
    out.fill(Qt::transparent);
    QPainter p(&out);
    // Centre the (aspect-preserved) glyph within the target canvas.
    const int x = (size.width()  - source.width())  / 2;
    const int y = (size.height() - source.height()) / 2;
    p.drawPixmap(x, y, source);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), color);
    p.end();
    return QIcon(out);
}

// Recolour an alpha-shaped resource glyph to @color. Use only for icons that
// carry their shape in the alpha channel (not opaque-background PNGs).
QIcon tintedPixmapIcon(const QString& resourcePath, const QColor& color) {
    QPixmap source(resourcePath);
    if (source.isNull())
        return QIcon();
    QPixmap out(source.size());
    out.fill(Qt::transparent);
    QPainter p(&out);
    p.drawPixmap(0, 0, source);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(out.rect(), color);
    p.end();
    return QIcon(out);
}
}


TopMenuWorkout::~TopMenuWorkout()
{
    delete ui;

}



TopMenuWorkout::TopMenuWorkout(QWidget *parent) : QWidget(parent), ui(new Ui::TopMenuWorkout)
{
    ui->setupUi(this);

    dontShowRemainingTime = false;
    hasTargetPower = false;
    hasTargetCad = false;
    hasTargetHr= false;


    int size = 25;
    QPixmap pixmapPower = QPixmap(":/image/icon/power2");
    QPixmap pixmapCadence = QPixmap(":/image/icon/crank2");
    QPixmap pixmapHr = QPixmap(":/image/icon/heart2");
    QPixmap pixmapRadio = QPixmap(":/image/icon/radio");

    pixmapPower = pixmapPower.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmapCadence = pixmapCadence.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmapHr = pixmapHr.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmapRadio = pixmapRadio.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);


    ui->label_imagePower->setPixmap(pixmapPower);
    ui->label_imageCad->setPixmap(pixmapCadence);
    ui->label_imageHr->setPixmap(pixmapHr);

    ui->label_imageCad->setToolTip(tr("Power - %FTP"));
    ui->label_imagePower->setVisible(false);
    ui->label_targetPower->setToolTip(tr("Power - %FTP"));
    ui->label_targetPower->setText("");
    ui->label_rangePower->setText("");

    ui->label_imageCad->setToolTip(tr("Cadence - rpm"));
    ui->label_imageCad->setVisible(false);
    ui->label_targetCad->setToolTip(tr("Cadence - rpm"));
    ui->label_targetCad->setText("");
    ui->label_rangeCad->setText("");

    ui->label_imageHr->setToolTip(tr("Heart Rate - %LTHR"));
    ui->label_imageHr->setVisible(false);
    ui->label_targetHr->setToolTip(tr("Heart Rate - %LTHR"));
    ui->label_targetHr->setText("");
    ui->label_rangeHr->setText("");

    ui->label_intervalTime->setToolTip(tr("Interval Remaining"));
    ui->label_timeElapsed->setToolTip(tr("Elapsed time"));
    ui->label_remainingTime->setToolTip(tr("Workout Remaining"));


    ui->label_radioIcon->setPixmap(pixmapRadio);

    const QSize iconSize(20, 20);
    const QColor iconColor(230, 230, 230);
    ui->pushButton_prevRadio->setIcon(tintedStandardIcon(style(), QStyle::SP_MediaSkipBackward, iconSize, iconColor));
    ui->pushButton_prevRadio->setIconSize(iconSize);
    ui->pushButton_playPauseRadio->setIcon(tintedStandardIcon(style(), QStyle::SP_MediaPlay, iconSize, iconColor));
    ui->pushButton_playPauseRadio->setIconSize(iconSize);
    ui->pushButton_nextRadio->setIcon(tintedStandardIcon(style(), QStyle::SP_MediaSkipForward, iconSize, iconColor));
    ui->pushButton_nextRadio->setIconSize(iconSize);

    // Window/config controls: render light icons on the always-dark toolbar,
    // instead of the legacy opaque-black PNGs that the dark theme broke.
    // Config uses the alpha-shaped fa-gear glyph; the window controls use Qt's
    // standard title-bar pixmaps. All tinted light to match the media buttons.
    const QSize ctrlIconSize(20, 20);
    ui->pushButton_config->setIcon(tintedPixmapIcon(":/image/icon/crank2", iconColor));
    ui->pushButton_config->setIconSize(ctrlIconSize);
    ui->pushButton_expand->setIcon(tintedStandardIcon(style(), QStyle::SP_TitleBarMaxButton, ctrlIconSize, iconColor));
    ui->pushButton_expand->setIconSize(ctrlIconSize);
    ui->pushButton_exit->setIcon(tintedStandardIcon(style(), QStyle::SP_TitleBarCloseButton, ctrlIconSize, iconColor));
    ui->pushButton_exit->setIconSize(ctrlIconSize);

    // Start/Pause uses the same tinted standard media glyphs as the radio
    // controls rather than the legacy coloured PNG, for a native, consistent
    // look on the dark toolbar.
    const QSize startIconSize(16, 16);
    ui->pushButton_start->setIcon(tintedStandardIcon(style(), QStyle::SP_MediaPlay, startIconSize, iconColor));
    ui->pushButton_start->setIconSize(startIconSize);


    ui->pushButton_calibrateFEC->setFocusPolicy(Qt::NoFocus);
    ui->pushButton_calibratePM->setFocusPolicy(Qt::NoFocus);
    ui->pushButton_config->setFocusPolicy(Qt::NoFocus);
    ui->pushButton_expand->setFocusPolicy(Qt::NoFocus);
    ui->pushButton_start->setFocusPolicy(Qt::NoFocus);
    ui->pushButton_lap->setFocusPolicy(Qt::NoFocus);
    ui->pushButton_exit->setFocusPolicy(Qt::NoFocus);



#ifdef Q_OS_WIN32
    ui->pushButton_expand->installEventFilter(this);
    ui->pushButton_expand->setStyleSheet("QPushButton#pushButton_expand{image: url(:/image/icon/exp1);border-radius: 1px;}");
    isMacMenu = false;
#endif
#ifdef Q_OS_MAC
    ui->pushButton_expand->setVisible(false);
    ui->pushButton_exit->setVisible(false);
    isMacMenu = true;

    this->setMaximumHeight(35); //PushButton bigger on OSX, need more space
    ui->pushButton_start->setMinimumWidth(130);
    ui->pushButton_lap->setMinimumWidth(130);

    //ui->pushButton_start->setIcon(QIcon());
#endif



    ui->pushButton_calibratePM->setHidden(true);
    ui->pushButton_calibrateFEC->setHidden(true);




    timeUpdateTime = new QTimer(this);
    timeUpdateTime->start(60000);
    connect (timeUpdateTime, SIGNAL(timeout()), this, SLOT(setCurrentTime()) );
    setCurrentTime();

}


////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::radioStartedPlaying() {
    ui->pushButton_playPauseRadio->setIcon(
        tintedStandardIcon(style(), QStyle::SP_MediaPause, QSize(20, 20), QColor(230, 230, 230)));
}

void TopMenuWorkout::radioStoppedPlaying() {
    ui->pushButton_playPauseRadio->setIcon(
        tintedStandardIcon(style(), QStyle::SP_MediaPlay, QSize(20, 20), QColor(230, 230, 230)));
}


//------------------------------------------------------------
void TopMenuWorkout::setCurrentTime() {

    QTime time = QTime::currentTime();
    QString timeTxt = Util::showCurrentTimeAsString(time);
    ui->label_currentTime->setText(timeTxt);
}

//-----------------------------------------------------------------------------------------------------------------------------------
bool TopMenuWorkout::eventFilter(QObject *watched, QEvent *event) {


    Q_UNUSED(watched);

    //    qDebug() << "watched object" << watched << "event:" << event << "eventType" << event->type();

    if(event->type() == QEvent::HoverEnter)
    {
        ui->pushButton_expand->setStyleSheet("QPushButton#pushButton_expand{image: url(:/image/icon/exp2);border-radius: 1px;}");
    }
    else if(event->type() == QEvent::HoverLeave)
    {
        ui->pushButton_expand->setStyleSheet("QPushButton#pushButton_expand{image: url(:/image/icon/exp1);border-radius: 1px;}");
    }
    return false;

}




////-----------------------------------------------------------------------------------------------------------------------------------
//void TopMenuWorkout::setFreeRideMode() {


//    ui->label_intervalTime->setVisible(false);
//    ui->progressBar_interval->setVisible(false);

//}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::setMinExpandExitVisible(bool visible) {


    if (isMacMenu)
        return;

    ui->pushButton_expand->setVisible(visible);
    ui->pushButton_exit->setVisible(visible);
}


//-----------------------------------------
void TopMenuWorkout::on_pushButton_expand_clicked()
{
    emit expand();
}
//-----------------------------------------
void TopMenuWorkout::on_pushButton_config_clicked()
{
    emit config();
}

void TopMenuWorkout::on_pushButton_exit_clicked()
{
    emit exit();
}





////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::updateRadioStatus(QString status) {

    ui->label_radioStatus->setText(status);
    ui->label_radioStatus->fadeIn(400);


}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::updateExpandIcon() {

    ui->pushButton_expand->setIcon(
        tintedStandardIcon(style(), QStyle::SP_TitleBarMaxButton, QSize(20, 20), QColor(230, 230, 230)));
    ui->pushButton_expand->setIconSize(QSize(20, 20));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::changeConfigIcon(bool insideConfig) {

    // Tinted gear glyph (see constructor). Brighten when the config dialog is
    // open to signal the active state, instead of swapping PNGs.
    const QColor color = insideConfig ? QColor(255, 255, 255) : QColor(230, 230, 230);
    ui->pushButton_config->setIcon(tintedPixmapIcon(":/image/icon/crank2", color));
    ui->pushButton_config->setIconSize(QSize(20, 20));
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void TopMenuWorkout::setButtonStartReady(bool ready) {
    ui->pushButton_start->setVisible(ready);
}

void TopMenuWorkout::setWorkoutNameLabel(QString label) {
    ui->label_nameWorkout->setText(label);
}

//void TopMenuWorkout::setWorkoutTime(QString time) {
//    ui->labelTime->setText(time);
//}

//void TopMenuWorkout::setIntervalTime(QString time) {
//    ui->label_intervalTime->setText(time);
//}

//void TopMenuWorkout::setProgressBarMaximum(int value) {
//    ui->progressBar_interval->setMaximum(value);
//}

//void TopMenuWorkout::setProgressBarCurrent(int value) {
//    ui->progressBar_interval->setValue(value);
//}

//void TopMenuWorkout::setProgressBarMinus1() {
//    ui->progressBar_interval->setValue(ui->progressBar_interval->value()-1);
//}


//-------------------------------------------------------------------------
void TopMenuWorkout::setButtonStartPaused(bool showPause) {



    const QSize startIconSize(16, 16);
    const QColor startIconColor(230, 230, 230);
    if (showPause) {
        ui->pushButton_start->setText(tr("Pause"));
        ui->pushButton_start->setIcon(tintedStandardIcon(style(), QStyle::SP_MediaPause, startIconSize, startIconColor));
    }
    else {
        ui->pushButton_start->setText(tr("Resume"));
        ui->pushButton_start->setIcon(tintedStandardIcon(style(), QStyle::SP_MediaPlay, startIconSize, startIconColor));
    }
    ui->pushButton_start->setIconSize(startIconSize);


}

//-------------------------------------------------------------------------
void TopMenuWorkout::on_pushButton_start_clicked()
{
    emit startOrPause();




}


//-------------------------------------------------------------------------
void TopMenuWorkout::setButtonLapVisible(bool visible) {
    ui->pushButton_lap->setHidden(!visible);
}

//-------------------------------------------------------------------------
void TopMenuWorkout::setButtonCalibratePMVisible(bool visible) {
    ui->pushButton_calibratePM->setHidden(!visible);
}
void TopMenuWorkout::setButtonCalibrationFECVisible(bool visible) {
    ui->pushButton_calibrateFEC->setHidden(!visible);
}





//-------------------------------------------------------------------------
void TopMenuWorkout::on_pushButton_lap_clicked()
{
    emit lap();
}
//-------------------------------------------------------------------------
void TopMenuWorkout::on_pushButton_calibrateFEC_clicked()
{
    emit startCalibrateFEC();
}

void TopMenuWorkout::on_pushButton_calibratePM_clicked()
{
    emit startCalibrationPM();
}


/////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::setTimersVisible(bool visible) {


    ui->frame_timer->setVisible(visible);


}

/////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::setFreeRideMode() {

    ui->label_intervalTime->setToolTip(tr("Elapsed Interval"));
    ui->label_remainingTime->setVisible(false);
    ui->label_dash2->setVisible(false);


    dontShowRemainingTime = true;

}

/////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::setMAPMode() {


    ui->label_remainingTime->setVisible(false);
    ui->label_dash2->setVisible(false);

    dontShowRemainingTime = true;
}


/////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::setIntervalTime(QTime time) {

    ui->label_intervalTime->setText(Util::showQTimeAsString(time));
}

void TopMenuWorkout::setWorkoutRemainingTime(QTime time) {

    ui->label_remainingTime->setText(Util::showQTimeAsString(time));
}

void TopMenuWorkout::setWorkoutTime(QTime time) {

    ui->label_timeElapsed->setText(Util::showQTimeAsString(time));
}



/////////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::showIntervalRemaining(bool show) {

    ui->label_intervalTime->setVisible(show);
    ui->label_dash3->setVisible(show);
}
/////////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::showWorkoutRemaining(bool show) {

    qDebug() << "TOP MENU, show Workout Remaining" << show;
    // Not allowed on certain Workouts
    if (dontShowRemainingTime) {
        return;
    }

    qDebug() << "TOP MENU2, show Workout Remaining" << show;

    ui->label_remainingTime->setVisible(show);
    ui->label_dash2->setVisible(show);
}

/////////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::showWorkoutElapsed(bool show) {

    ui->label_timeElapsed->setVisible(show);
}

//////////////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::setTargetPower(double percentageFTP, int range) {


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

}


//////////////////////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::setTargetCadence(int cadence, int range) {


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

}


////////////////////////////////////////////////////////////////////////////////////////////////////
void TopMenuWorkout::setTargetHeartRate(double percentageLTHR, int range) {


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

}

////////////////////////////////////////////////////////////
void TopMenuWorkout::on_pushButton_prevRadio_clicked()
{
    emit prevRadio();
}

void TopMenuWorkout::on_pushButton_playPauseRadio_clicked()
{
    emit playPauseRadio();
}

void TopMenuWorkout::on_pushButton_nextRadio_clicked()
{
    emit nextRadio();
}
