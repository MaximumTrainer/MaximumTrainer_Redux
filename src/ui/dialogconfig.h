#ifndef DIALOGCONFIG_H
#define DIALOGCONFIG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLabel>

#include "workoutdialog.h"
#include "account.h"
#include "soundplayer.h"
#include "radiotablemodel.h"



namespace Ui {
class DialogConfig;
}

class DialogConfig : public QDialog
{
    Q_OBJECT

public:
    DialogConfig(QList<Radio> lstRadio, QWidget *parent, WorkoutDialog *ptrParent);
    ~DialogConfig();
    void setStudioMode();

    void accept();
    void reject();


    void saveSettings();






signals:
    void signal_connectToRadioUrl(QString url);
    void signal_volumeRadioChanged(int volume);
    void signal_stopPlayingRadio();



    void radioStatus(QString status);



public slots:
    void radioStartedPlaying();
    void radioStoppedPlaying();

    void playPauseRadio();




private slots:
    void currentListViewSelectionChanged(int section);

    void on_checkBox_targetPower_clicked(bool checked);
    void on_checkBox_targetCadence_clicked(bool checked);
    void on_checkBox_targetHr_clicked(bool checked);

    void on_comboBox_startTrigger_activated(int index);

    void on_checkBox_seperator_clicked(bool checked);
    void on_checkBox_showGrid_clicked(bool checked);


    void on_checkBox_enableHR_clicked(bool checked);
    void on_checkBox_enablePower_clicked(bool checked);
    void on_checkBox_enableCadence_clicked(bool checked);
    void on_checkBox_enableSpeed_clicked(bool checked);
    void on_checkBox_enableCalories_clicked(bool checked);
    void on_checkBox_enablePowerBalance_clicked(bool checked);

    void on_comboBox_displayHR_currentIndexChanged(int index);
    void on_comboBox_virtualSpeed_currentIndexChanged(int index);

    void on_comboBox_displayCadence_currentIndexChanged(int index);
    void on_comboBox_displayPower_currentIndexChanged(int index);
    void on_comboBox_displayPowerBalance_currentIndexChanged(int index);
    void on_checkBox_PowerCurve_clicked(bool checked);
    void on_checkBox_CadenceCurve_clicked(bool checked);
    void on_checkBox_HeartRateCurve_clicked(bool checked);
    void on_checkBox_SpeedCurve_clicked(bool checked);

    void on_comboBox_displayVideo_activated(int index);

    void sliderValueMoved(int value);
    void sliderSoundPressed();
    void sliderSoundReleased();


    // Internet Radio
    void RadioDoubleClicked(QModelIndex);
    void playNextOrPreviousRadio(bool next);
    void on_pushButton_prevRadio_clicked();
    void on_pushButton_nextRadio_clicked();


    ///timers
    void on_checkBox_showTimerOnTop_clicked(bool checked);
    void on_checkBox_showIntervalRemainingTime_clicked(bool checked);
    void on_checkBox_showWorkoutRemainingTime_clicked(bool checked);
    void on_checkBox_showElapsedTime_clicked(bool checked);
    void on_comboBox_fontSizeTimer_currentIndexChanged(const QString &arg1);





    void on_pushButton_rightTimer_clicked();

    void on_pushButton_leftTimer_clicked();

    void on_pushButton_leftHr_clicked();

    void on_pushButton_rightHr_clicked();

    void on_pushButton_leftPower_clicked();

    void on_pushButton_rightPower_clicked();

    void on_pushButton_rightPowerBal_clicked();

    void on_pushButton_leftPowerBal_clicked();

    void on_pushButton_rightCadence_clicked();

    void on_pushButton_leftCadence_clicked();

    void on_pushButton_rightSpeed_clicked();

    void on_pushButton_leftSpeed_clicked();

    void on_pushButton_rightInfoWorkout_clicked();

    void on_pushButton_leftInfoWorkout_clicked();

    void on_checkBox_enableOxygen_clicked(bool checked);

    void on_pushButton_leftOxygen_clicked();

    void on_pushButton_rightOxygen_clicked();






    void on_checkBox_showTrainerSpeed_clicked(bool checked);

    void on_spinBox_value_intervalmessage_duration_sec_valueChanged(int arg1);

    void on_spinBox_value_intervalmessage_before_valueChanged(int arg1);

private :
    void initUi();

    void playSoundTestEffect();
    void moveElement(QString widgetIdentifier, bool moveRight); //moveRight false = moveLeft

    // Group C: preference tab setup helpers
    QWidget *setupLoggingTab();
    QWidget *setupLanguageTab();
    QWidget *setupStudioTab();
    QWidget *setupTrainerTab();

    void initLoggingTab();
    void initLanguageTab();
    void initStudioTab();
    void initTrainerTab();

    void saveLoggingTab();
    void saveLanguageTab();
    void saveStudioTab();
    void saveTrainerTab();

private:
    RadioTableModel *tableModel;
    QString currentRadioName;
    QModelIndex currentRadioIndex;


    Ui::DialogConfig *ui;
    WorkoutDialog *parentDialog;
    //    Settings *settings;
    Account *account;
    SoundPlayer *soundPlayer;

    bool isHoldingSliderSound;
    bool playOnNextSliderRelease;

    bool isPlayingRadio;

    // Logging tab controls
    QComboBox    *comboLogLevel     = nullptr;
    QCheckBox    *checkLogToFile    = nullptr;
    QLabel       *labelLogFilePath  = nullptr;

    // Language tab controls
    QComboBox    *comboLanguage     = nullptr;
    QLabel       *labelRestartNote  = nullptr;

    // Studio tab controls
    QCheckBox    *checkStudioMode   = nullptr;
    QSpinBox     *spinNbRiders      = nullptr;

    // Trainer tab controls
    QCheckBox    *checkControlResistance = nullptr;
    QComboBox    *comboTrainerModel      = nullptr;
    QSpinBox     *spinErgSmoothing       = nullptr;
    QCheckBox    *checkIntervalsIcuAutoUpload = nullptr;

    // Trainer tab — Sensor dropout (issue #148)
    QCheckBox    *checkDropoutEnabled    = nullptr;
    QSpinBox     *spinDropoutTimeout     = nullptr;

    // Trainer tab — Battery warning threshold (issue #156)
    QSpinBox     *spinBatteryThreshold   = nullptr;

    // Interval Summary overlay controls
    QCheckBox    *checkIntervalSummary         = nullptr;
    QSpinBox     *spinIntervalSummaryDuration  = nullptr;
};

#endif // DIALOGCONFIG_H


