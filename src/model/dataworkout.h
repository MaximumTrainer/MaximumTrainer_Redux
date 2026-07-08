#ifndef DATAWORKOUT_H
#define DATAWORKOUT_H

#include <QDateTime>
#include <QList>
#include <QVector>
#include <QDebug>

#include "intervaldata.h"
#include "settings.h"
#include "workout.h"
#include "fitactivitycreator.h"

class DataWorkout : public QObject
{
    Q_OBJECT


public:
    DataWorkout(Workout workout, int userftp, QObject *parent = 0);
    ~DataWorkout(); // closeFitFile


    void updateData(bool studioMode, int secReceived,
                    double hr, double cadence, double speed, double power,
                    double rightPedal, double avgLeftTorqueEff, double avgRightTorqueEff, double avgLeftPedalSmooth, double avgRightPedalSmooth, double avgCombinedPedalSmooth,
                    double saturatedHemoglobinPercent, double totalHemoglobinConc);
    void checkUpdateMaxHr(int);
    void checkUpdateMaxCad(int);
    void checkUpdateMaxSpeed(double);
    void checkUpdateMaxPower(int);
    void updateDistance(double kmToAdd);


    void changeInterval(double timeStarted, double timeNow, int timePaused_msec, bool workoutOver, bool testInterval);
    void writeEndFile(double timeNow);
    void initFitFile(bool createDir, QString username, QString nameWorkout, QDateTime timeStarted);
    void closeFitFile();
    void closeAndDeleteFitFile();

    QString getFitFilename();



    void setStartTimeWorkout(QDateTime dt);



    // Retrive Test results
    int getFTP();
    int getLTHR();
    //-----------------

    // Interval average accessors — valid until the next changeInterval() call
    double getAvgIntervalPower() const { return avgIntervalPower; }
    double getAvgIntervalHr()    const { return avgIntervalHr; }
    double getAvgIntervalCad()   const { return avgIntervalCad; }
    int    getNbPointsIntervalPower() const { return nbPointsIntervalPower; }





    /////////////////////////////////////////////////////////////////////////
signals:
    void maxCadenceIntervalChanged(double cadence);
    void maxCadenceWorkoutChanged(double cadence);
    void avgCadenceIntervalChanged(double cadence);
    void avgCadenceWorkoutChanged(double cadence);

    void maxPowerIntervalChanged(double cadence);
    void maxPowerWorkoutChanged(double cadence);
    void avgPowerIntervalChanged(double power);
    void avgPowerWorkoutChanged(double power);

    void maxHrIntervalChanged(double cadence);
    void maxHrWorkoutChanged(double cadence);
    void avgHrIntervalChanged(double hr);
    void avgHrWorkoutChanged(double hr);

    void maxSpeedIntervalChanged(double cadence);
    void maxSpeedWorkoutChanged(double cadence);
    void avgSpeedIntervalChanged(double speed);
    void avgSpeedWorkoutChanged(double speed);


    void normalizedPowerChanged(double NP);
    void intensityFactorChanged(double IF);
    void tssChanged(double TSS);

    void caloriesWorkoutChanged(double calories);
    void totalDistanceChanged(double distanceMeters);







private :
    void hrDataReceived(double averageHr1sec);
    void cadDataReceived(double averageCadence1sec);
    void speedDataReceived(double averageSpeed1sec);
    void powerDataReceived(int secReceived, double averageCadence1sec);

    void updateWorkoutMetrics(int secReceived, double power);


    //-- helpers getPowerResultTest
    int getFTP_normal();
    int getFTP_8min();



private:
    FitActivityCreator fitCreator;
    Workout workout;


    int ftp;
    QDateTime startTimeWorkout;


    /// Real-time Workout Stats calculation
    int nbPoints30SecPower;
    double totalSumExp4;
    double last30secMean;
    double nb30secMean;

    double normalizedPower;
    double intensityFactor;
    double trainingStressScore;



    // To keep lap average data
    int nbLap;
    QList<double> meanHr;
    QList<double> meanCad;
    QList<double> meanSpeed;
    QList<double> meanPower;
    // To keep lap average for test interval only
    QList<double> meanHrTest;
    QList<double> meanCadTest;
    QList<double> meanSpeedTest;
    QList<double> meanPowerTest;


    /// Hr -------------------------
    double avgIntervalHr;
    double maxIntervalHr;
    int nbPointsIntervalHr;

    double avgWorkoutHr;
    double maxWorkoutHr;
    int nbPointsWorkoutHr;


    /// Cadence -------------------------
    double avgIntervalCad;
    double maxIntervalCad;
    int nbPointsIntervalCad;

    double avgWorkoutCad;
    int maxWorkoutCad;
    int nbPointsWorkoutCad;


    /// Speed -------------------------
    double avgIntervalSpeed;
    double maxIntervalSpeed;
    int nbPointsIntervalSpeed;

    double avgWorkoutSpeed;
    int maxWorkoutSpeed;
    int nbPointsWorkoutSpeed;



    /// Power -------------------------
    double avgIntervalPower;
    double maxIntervalPower;
    int nbPointsIntervalPower;

    double avgWorkoutPower;
    int maxWorkoutPower;
    int nbPointsWorkoutPower;


    /// Calories
    double calories;
    double lapCalories;

    ///distance
    double totalMeters;


    ///Last Used Value (to fix dropouts)
    double lastHr = -1;
    double lastCadence = -1;
    double lastSpeed = -1;
    double lastPower = -1;
    double lastRightPedal = -1;
    double  lastLeftTorqueEff = -1;
    double lastRightTorqueEff = -1;
    double lastLeftPedalSmooth = -1;
    double lastRightPedalSmooth = -1;
    double lastCombinedPedalSmooth = -1;
    double lastSaturatedHemoglobinPercent = -1;
    double lastTotalHemoglobinConc = -1;

    int numberTimeUsedLastHr = 0;
    int numberTimeUsedLastCadence = 0;
    int numberTimeUsedLastSpeed = 0;
    int numberTimeUsedLastPower = 0;
    int numberTimeUsedLastRightPedal = 0;
    int numberTimeUsedLastLeftTorqueEff = 0;
    int numberTimeUsedLastSaturatedHemoglobinPercent = 0;








};




#endif // DATAWORKOUT_H
