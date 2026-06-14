#ifndef CLOCK_H
#define CLOCK_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QElapsedTimer>

class Clock : public QObject
{
    Q_OBJECT


public:
    explicit Clock(QString objectName, QObject *parent = 0);



signals:
    void oneCyclePassed(double totalTimeElapsed_msec);
    void oneSecPassed(double totalTimeElapsed_sec);


    void updateTimePaused(double totalTimePaused_msec);
    void virtualSpeed(int userID, double speedMS, double timeAtSpeedSec);


    void finished();



public slots:
    void startClock();
    void pauseClock();
    void resumeClock();
    void finishClock();

    // Virtual speed
    void receiveUserInfo(int userID, double cda, double weightKG, int nbUser);
    void receivePowerData(int userID, int powerWatts);
    void receiveSlopeData(int userID, double slope);

    void startClockSpeed();


private slots:
    void timerClockTimeout();
    void timerSpeedTimeout();



private :

private :


    QTimer *timerClock;
    const int timerClockMs = 25;
    QElapsedTimer d_clock;
    QElapsedTimer d_clock_speed;

    double totalTimeElapsed_msec;
    int lastSecond;

    double resumeActivatedAt_time_msec;
    double pauseActivatedAt_time_msec;
    double totalTimePaused_msec;


    //used to calculate virtual speed, never stops
    QTimer *timerSpeed;
    const int timerSpeedMs = 333;
    double totalTimeElapsedLast;

    bool firstSpeedCheck;


    int nbUser;
    // TOCHECK: Consider making an array[nbUser] of Object with theses attributes:
    QVector<double> cda;
    QVector<double> weightKG;
    QVector<double> currentSpeedMS;
    QVector<int> currentPowerWatts;
    QVector<double> currentSlope;







};

#endif // CLOCK_H
