#ifndef XMLUTIL_H
#define XMLUTIL_H

#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <workout.h>
#include <interval.h>
#include "repeatwidget.h"
#include "account.h"
#include "settings.h"
#include "course.h"
#include "userstudio.h"



class XmlUtil: public QObject
{
    Q_OBJECT

public:
    XmlUtil(QObject *parent = 0);


    QList<Workout> parseWorkoutLstPath(QStringList lstPath, Workout::WORKOUT_NAME workoutType);
    QList<Course> parseCourseLstPath(QStringList lstPath, Course::COURSE_TYPE courseType);


    static bool createWorkoutXml(Workout workout, QString destinationPath);
    static bool createCourseXml(Course course, QString destinationPath);


    QString parseFileNameFromPath(QString filePath);


    // Workouts from ressource
    QList<Workout> getLstWorkoutRachel();
    QList<Workout> getLstWorkoutSufferfest();
    QList<Workout> getLstWorkoutBt16WeeksPlan();


    QList<Workout> getLstUserWorkout();


    //Courses
    QList<Course> getLstUserCourse();
    //Courses from ressources
    QList<Course> getLstCourseIncluded();


    //parse .save file and load data in Settings and Account
    static void parseLocalSaveFile(Account *account);
    static void parseWorkoutDone(Account *account, QXmlStreamReader&);
    static void parseCourseDone(Account *account, QXmlStreamReader&);

    //Save data from Settings and Account to .save file
    static bool saveLocalSaveFile(Account *account);

    //User Studio
    static bool saveUserStudioFile(QVector<UserStudio>, QString filepath);
    QVector<UserStudio> parseUserStudioFile(QString filepath);
    UserStudio parseUserStudio(QXmlStreamReader &xml);


    // List workout Done
    //    static bool saveLstWorkoutDone(QString email_clean, QSet<QString> hashWorkoutDone);

    Workout parseSingleWorkoutXml(QString filePath);
    Interval parseInterval(QXmlStreamReader&);
    RepeatData parseRepeat(QXmlStreamReader&);


    //    Course parseSingleCourseXml(QString filePath);
    Trackpoint parseTrackpoint(QXmlStreamReader&);



signals:
    void workoutListIsReady();




};

#endif // XMLUTIL_H
