#ifndef XMLUTIL_H
#define XMLUTIL_H

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <workout.h>
#include <interval.h>
#include "repeatwidget.h"
#include "account.h"
#include "settings.h"
#include "userstudio.h"
#include "trackpoint.h"



class XmlUtil: public QObject
{
    Q_OBJECT

public:
    XmlUtil(QObject *parent = 0);


    QList<Workout> parseWorkoutLstPath(QStringList lstPath, Workout::WORKOUT_NAME workoutType);


    static bool createWorkoutXml(Workout workout, QString destinationPath);


    QString parseFileNameFromPath(QString filePath);


    // Workouts from ressource
    QList<Workout> getLstWorkoutFtpKickstart();
    QList<Workout> getLstWorkoutPolarized3x();
    QList<Workout> getLstWorkoutVo2ShockBlock();


    QList<Workout> getLstUserWorkout();


    //parse .save file and load data in Settings and Account
    static void parseLocalSaveFile(Account *account);
    static void parseWorkoutDone(Account *account, QXmlStreamReader&);

    //Save data from Settings and Account to .save file
    static bool saveLocalSaveFile(Account *account);



    // List workout Done
    //    static bool saveLstWorkoutDone(QString email_clean, QSet<QString> hashWorkoutDone);

    Workout parseSingleWorkoutXml(QString filePath);
    Interval parseInterval(QXmlStreamReader&);
    RepeatData parseRepeat(QXmlStreamReader&);


    Trackpoint parseTrackpoint(QXmlStreamReader&);



signals:
    void workoutListIsReady();




};

#endif // XMLUTIL_H
