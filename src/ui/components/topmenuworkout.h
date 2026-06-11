#ifndef TOPMENUWORKOUT_H
#define TOPMENUWORKOUT_H

#include <QWidget>
#include <QPushButton>

#include "calibration_types.h"

namespace Ui {
class TopMenuWorkout;
}

class TopMenuWorkout : public QWidget
{
    Q_OBJECT

public:
    explicit TopMenuWorkout(QWidget *parent = 0);
    ~TopMenuWorkout();

    void setMinExpandExitVisible(bool visible);

    void setButtonStartPaused(bool showPause);
    void setButtonStartReady(bool ready);
    void setWorkoutNameLabel(QString label);

    void setButtonLapVisible(bool visible);
    void setButtonCalibratePMVisible(bool visible);
    void setButtonCalibrationFECVisible(bool visible);



    //Timers
    void setTimersVisible(bool);
    void setFreeRideMode();
    void setMAPMode();

    void setIntervalTime(QTime time);
    void setWorkoutRemainingTime(QTime time);
    void setWorkoutTime(QTime time);

    void showIntervalRemaining(bool);
    void showWorkoutRemaining(bool);
    void showWorkoutElapsed(bool);
    void showCurrentTarget(bool);

    //Target
    void setTargetPower(double percentageFTP, int range);
    void setTargetCadence(int cadence, int range);
    void setTargetHeartRate(double percentageLTHR, int range);





signals :
    void expand();
    void config();
    void exit();

    void startOrPause();
    void sendCalibrate(CalibrationType eCalibrationType);
    void lap();
    void startCalibrateFEC();
    void startCalibrationPM();

    //radio
    void prevRadio();
    void playPauseRadio();
    void nextRadio();



public slots:
    void updateExpandIcon();
    void changeConfigIcon(bool insideConfig);

    void updateRadioStatus(QString status);

    //radio
    void radioStartedPlaying();
    void radioStoppedPlaying();


private slots:
    void on_pushButton_expand_clicked();
    void on_pushButton_config_clicked();

    void on_pushButton_exit_clicked();


    void on_pushButton_start_clicked();

    void setCurrentTime();

    void on_pushButton_lap_clicked();



    void on_pushButton_calibrateFEC_clicked();



    void on_pushButton_calibratePM_clicked();

    void on_pushButton_prevRadio_clicked();

    void on_pushButton_playPauseRadio_clicked();

    void on_pushButton_nextRadio_clicked();

private :
    bool eventFilter(QObject *watched, QEvent *event);


private:
    Ui::TopMenuWorkout *ui;

    bool hasTargetPower;
    bool hasTargetCad;
    bool hasTargetHr;
    bool showTargetEnabled = true;

    bool isMacMenu;


    QTimer *timeUpdateTime;

    bool dontShowRemainingTime;

};

#endif // TOPMENUWORKOUT_H





