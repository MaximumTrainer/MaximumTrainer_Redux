#ifndef TOPMENUWORKOUT_H
#define TOPMENUWORKOUT_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QToolButton>

namespace Ui {
class TopMenuWorkout;
}

class TopMenuWorkout : public QWidget
{
    Q_OBJECT

public:
    explicit TopMenuWorkout(QWidget *parent = nullptr);
    ~TopMenuWorkout() override;

    void setMinExpandExitVisible(bool visible);

    void setButtonStartPaused(bool showPause);
    void setButtonStartReady(bool ready);
    void setWorkoutNameLabel(QString label);




    //Timers
    void setTimersVisible(bool);
    void setFreeRideMode();
    void setMAPMode();

    void setIntervalTime(QTime time);
    void setWorkoutRemainingTime(QTime time);
    void setWorkoutTime(QTime time);
    /// The start button's label before the first start ("Start Workout").
    void setStartLabel(const QString &label);
    /// A course: only the ride time shows (no interval or remaining time).
    void setCourseMode();

    void showIntervalRemaining(bool);
    void showWorkoutRemaining(bool);
    void showWorkoutElapsed(bool);
    void showCurrentTarget(bool);

    //Target
    void setTargetPower(double percentageFTP, int range);
    void setTargetCadence(int cadence, int range);
    void setTargetHeartRate(double percentageLTHR, int range);

    // Virtual-shifting gear indicator (#293): ▼ gear N/24 ▲ in the middle of the
    // top bar. Shown only while gears drive resistance; the dialog hides it
    // during ERG intervals (gears do nothing there).
    void setGearVisible(bool visible);
    void updateGear(int gear, int count);
    /// Briefly highlight a toolbar control (green pulse) so the rider sees an
    /// input registered, whatever the source (keys or on-screen).
    void flashShift(int direction);   // ▲ if > 0 else ▼ gear button
    void flashRadioPrev();
    void flashRadioNext();
    void flashRadioPlayPause();





signals :
    void expand();
    void config();
    void exit();

    void startOrPause();

    //radio
    void prevRadio();
    void playPauseRadio();
    void nextRadio();

    // Virtual-shifting: emitted when the on-screen ▲/▼ gear arrows are clicked.
    void gearUp();
    void gearDown();



public slots:
    void updateExpandIcon();
    void changeConfigIcon(bool insideConfig);

    void updateRadioStatus(QString status);
    void updateRadioVolume(int percent);

    //radio
    void radioStartedPlaying();
    void radioStoppedPlaying();


private slots:
    void on_pushButton_expand_clicked();
    void on_pushButton_config_clicked();

    void on_pushButton_exit_clicked();


    void on_pushButton_start_clicked();

    void setCurrentTime();


    void on_pushButton_prevRadio_clicked();

    void on_pushButton_playPauseRadio_clicked();

    void on_pushButton_nextRadio_clicked();

private :
    bool eventFilter(QObject *watched, QEvent *event) override;


private:
    Ui::TopMenuWorkout *ui;

    /// Pulse a widget green for ~180 ms, then restore its resting style.
    void flashWidget(QWidget *w);

    // Virtual-shifting gear indicator widgets (built in the constructor).
    QWidget     *m_gearWidget = nullptr;
    QToolButton *m_gearDownBtn = nullptr;
    QToolButton *m_gearUpBtn   = nullptr;
    QLabel      *m_gearLabel   = nullptr;
    int          m_lastRadioVolumePct = -1;   // flash the volume icon only on change

    bool hasTargetPower;
    bool hasTargetCad;
    bool hasTargetHr;
    bool showTargetEnabled = true;

    bool isMacMenu;


    QTimer *timeUpdateTime;

    bool dontShowRemainingTime;

};

#endif // TOPMENUWORKOUT_H





