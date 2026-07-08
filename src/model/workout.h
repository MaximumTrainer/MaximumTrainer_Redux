#ifndef WORKOUT_H
#define WORKOUT_H


#include <QString>
#include <QList>

#include "interval.h"
#include "repeatdata.h"



class Workout
{
    //http://qt-project.org/doc/qt-4.8/containers.html#assignable-data-types
public:

    //----------------------------
    enum Type
    {
        T_ENDURANCE,
        T_INTERVAL,
        T_TEMPO,
        T_TEST,
        T_OTHERS,
        T_THRESHOLD,
    };

    QString getTypeToString() const;


    //----------------------------
    enum WORKOUT_NAME
    {
        USER_MADE,

        FTP_TEST,
        FTP8min_TEST,
        MAP_TEST,

        INCLUDED_WORKOUT,
        OPEN_RIDE,
    };



    Workout();
    ~Workout() {}
    Workout( const Workout& other );
    Workout(QString filePath, WORKOUT_NAME workout_name_enum, QList<Interval> lstIntervalSource, QList<RepeatData> lstRepeat,
            QString name, QString createdBy, QString description, QString plan, Type type);

    Workout(QString filePath, WORKOUT_NAME workout_name_enum, QList<Interval> lstInterval,
            QString name, QString createdBy, QString description, QString plan, Type type);


//    bool operator==(const Workout &other) const;
    void computeWorkout();
    void computeWorkoutTotalTime();

    /// update NP, IF, TSS
    void initializeArrayFTP();
    void calculateWorkoutMetrics();




    const QList<Interval> &getLstInterval() const;
    QList<Interval> getLstIntervalSource() const;
    QList<RepeatData> getLstRepeat() const;

    QString getFilePath() const;
    WORKOUT_NAME getWorkoutNameEnum() const;
    QString getName() const;
    QString getCreatedBy() const;
    QString getDescription() const;
    Type getType() const;
    QString getPlan() const;

    double getMaxPowerPourcent() const;
    QString getMaxPowerPourcentQString() const;
    int getNbInterval() const;
    Interval getInterval(int nb) const;
    QTime getDurationQTime() const;

    double getAveragePower() const;
    double getNormalizedPower() const;
    double getIntensityFactor() const;
    double getTrainingStressScore() const;



    void setFilePath(QString filePath) {
        this->filePath = filePath;
    }
    void setWorkout_name_enum(Workout::WORKOUT_NAME workout_name_enum) {
        this->workout_name_enum = workout_name_enum;
    }
    void setName(QString name) {
        this->name = name;
    }
    void setCreator(QString createdBy) {
        this->createdBy = createdBy;
    }
    void setDescription(QString description) {
        this->description = description;
    }
    void setPlan(QString plan) {
        this->plan = plan;
    }
    void setType(Type type) {
        this->type = type;
    }


private :

    QList<Interval> lstInterval;
    QList<Interval> lstIntervalSource;
    QList<RepeatData> lstRepeatData;


    QString filePath;
    WORKOUT_NAME workout_name_enum;
    QString name;
    QString createdBy;
    QString description;
    Type type;
    QString plan;
    double maxPowerPourcent;
    QTime durationQTime;

    /// metrics related to user FTP
    double averagePower;
    double normalizedPower;
    double intensityFactor;
    double trainingStressScore;


    /// Used to calculate metrics
    QVector<double> vecFtp1sec;
    QVector<double> vecFtp30sec;

};
Q_DECLARE_METATYPE(Workout)

#endif // WORKOUT_H
