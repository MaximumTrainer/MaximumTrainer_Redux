#include "workoututil.h"
#include <QApplication>

#include "repeatdata.h"

WorkoutUtil::WorkoutUtil()
{
}


////////////////////////////////////////////////////////////////////////////////////
QList<Workout> WorkoutUtil::getListWorkoutBase() {


    QList<Workout> lstWorkout;

    lstWorkout.append(FTP());
    lstWorkout.append(FTP_8min());


    return lstWorkout;

}

////////////////////////////////////////////////////////////////////////////////////
Workout WorkoutUtil::getWorkoutMap(int userFTP) {


    return MAP(userFTP);
}









//http://www.canadian-cycling.com/cca/documents/protocolMAPe.pdf
//http://cyclingtrainingnuts.blogspot.ca/2013/03/maximal-aerobic-power-test-so-short-yet.html
//http://semiprocycling.com/map
//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
Workout WorkoutUtil::MAP(int userFTP) {


    qDebug() << "GOT HERE MAP TEST!" << userFTP;

    //Le dernier palier complété donne la PAM.
    //Si l'on complète 1'15 au palier de 300W, le résultat est 270 (dernier palier complété) + 12,5W (1'15 de fait sur 3:00 à 300W) = PAM de 282,5W

    // Augmentation de 30Watts par palier
    // On ajoute des palier tant que l'utilisateur n'echoue pas
    // Echec = Si l'utilisateur est en bas de la zone pendant plus de 15'' dans le 3min, test terminé (aller au cooldown de 5min)
    // Affiche des messages après 5secondes total en bas de la cible (à 5sec: 10sec restant en dehors de la zone avant la fin du test!)

    QString name = QApplication::translate("WorkoutUtil", "MAP Test", 0);
    QString description = QApplication::translate("WorkoutUtil", "Test that let you find your MAP (Maximal Aerobic Power). Main Protocol: 30W increase each 3' until you can keep up. End of Test happens after 20'' total or 10'' consecutive below the target zone, you will go to the cooldown directly (last interval). Test Intervals will be added automatically after you complete one. The duration of the test depends on your performance! Your FTP is calculated from the result (75% of MAP) and saved to your profile.", 0);
    QList<Interval> lstInterval;
    QList<RepeatData> lstRepeat;



    double userFTP_d = userFTP;
    int startValueWatts = 60;

    if (userFTP <= 120)      // [0-120]
        startValueWatts = 60;
    else if (userFTP <= 150) // ]120-150]
        startValueWatts = 90;
    else if (userFTP <= 240) // ]150-240]
        startValueWatts = 120;
    else if (userFTP <= 300) // ]240-300]
        startValueWatts = 150;
    else                    //  ]300-inf
        startValueWatts = 180;




    Interval intervalW1(QTime::fromString( "00:05:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Warm up", 0),
                        Interval::PROGRESSIVE,  0.45, 0.60, 15, -1,
                        Interval::NONE, 0, 0, 0,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);


    Interval intervalW2(QTime::fromString( "00:00:15", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "95 rpm", 0),
                        Interval::NONE,  0, 0, 0, -1,
                        Interval::FLAT, 95, 0, 5,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);
    Interval intervalW3(QTime::fromString( "00:00:15", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "105 rpm", 0),
                        Interval::NONE,  0, 0, 0, -1,
                        Interval::FLAT, 105, 0, 5,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);
    Interval intervalW4(QTime::fromString( "00:00:15", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "115 rpm", 0),
                        Interval::NONE,  0, 0, 0, -1,
                        Interval::FLAT, 115, 0, 5,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);



    Interval intervalW5(QTime::fromString( "00:01:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "1' Easy", 0),
                        Interval::FLAT,  0.5, 0, 15, -1,
                        Interval::NONE, 0, 0, 0,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);


    Interval intervalW6(QTime::fromString( "00:00:20", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Progressive acceleration", 0),
                        Interval::PROGRESSIVE,  0.7, 1.17, 15, -1,
                        Interval::NONE, 0, 0, 0,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);
    Interval intervalW6_1(QTime::fromString( "00:00:40", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Easy", 0),
                          Interval::FLAT,  0.5, 0, 15, -1,
                          Interval::NONE, 0, 0, 0,
                          Interval::NONE, 0, 0, 0,
                          false, 0, 0, 0);


    Interval intervalW7(QTime::fromString( "00:02:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "2' Easy", 0),
                        Interval::FLAT,  0.5, 0, 15, -1,
                        Interval::NONE, 0, 0, 0,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);


    Interval intervalCd(QTime::fromString( "00:10:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Cooldown", 0),
                        Interval::FLAT, 0.5, 0, 30, -1,
                        Interval::NONE, 0, 0, 0,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);



    // using conversion 85%PAM = FTP
    // Pédaler en z.1-2 (30 à 50% PAM) pendant 5 à 15'
    lstInterval.append(intervalW1);
    //4 X série suivante (ne pas dépasser 75% PAM) : 15'' à 95 rpm / 15'' à 105 rpm / 15'' à 95 rpm / 15'' à 115 rpm
    lstInterval.append(intervalW2);
    lstInterval.append(intervalW3);
    lstInterval.append(intervalW2);
    lstInterval.append(intervalW4);

    //    - 1' ez (moins de 40% PAM)
    lstInterval.append(intervalW5);
    //    - 3 X (20'' accélérations progressives de 60 à 100% PAM / 40'' ez)
    lstInterval.append(intervalW6);
    lstInterval.append(intervalW6_1);
    //    - 2' ez
    lstInterval.append(intervalW7);

    // main test protocol ------------
    int currentWatts = startValueWatts;

    QString firstInterval = QApplication::translate("WorkoutUtil", "Starting MAP Test at ", 0);
    QString msg = firstInterval + QString::number(currentWatts) + " watts";
    Interval intervalt1(QTime::fromString( "00:03:00", "hh:mm:ss"), msg,   //3min
                        Interval::FLAT, currentWatts/userFTP_d, 0, 15, -1,
                        Interval::FLAT, 90, 0, 10,
                        Interval::NONE, 0, 0, 0,
                        true, 0, 0, 0);

    currentWatts += 30;

    msg = QString::number(currentWatts) + " watts";
    Interval intervalt2(QTime::fromString( "00:03:00", "hh:mm:ss"), msg,
                        Interval::FLAT, currentWatts/userFTP_d, 0, 15, -1,
                        Interval::FLAT, 90, 0, 10,
                        Interval::NONE, 0, 0, 0,
                        true, 0, 0, 0);


    lstInterval.append(intervalt1);
    lstInterval.append(intervalt2);

    //-------------------------------

    lstInterval.append(intervalCd);


    //id, firstrow, lastrow, numberRepeat
    RepeatData repeat0(0, 1, 4, 4);
    lstRepeat.append(repeat0);

    RepeatData repeat1(0, 6, 7, 3);
    lstRepeat.append(repeat1);


    Workout workout("", Workout::MAP_TEST, lstInterval, lstRepeat,
                    name, "MaximumTrainer", description, "-", Workout::T_TEST);

    return workout;
}



//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
Workout WorkoutUtil::FTP() {



    QList<Interval> lstInterval;
    Interval interval1(QTime::fromString( "00:10:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Easy warm up", 0),
                       Interval::PROGRESSIVE,  0.50, 0.65, 15, -1,
                       Interval::NONE, 0, 0, 0,
                       Interval::NONE, 0, 0, 0,
                       false, 0, 0, 0);
    Interval interval2(QTime::fromString( "00:01:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Wind up", 0),
                       Interval::FLAT, 0.9, 0, 15, -1,
                       Interval::FLAT, 90, 0, 10,
                       Interval::NONE, 0, 0, 0,
                       false, 0, 0, 0);
    Interval interval3(QTime::fromString( "00:01:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Rest", 0),
                       Interval::FLAT, 0.5, 0, 30, -1,
                       Interval::NONE, 0, 0, 0,
                       Interval::NONE, 0, 0, 0,
                       false, 0, 0, 0);
    Interval interval4(QTime::fromString( "00:05:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Easy", 0),
                       Interval::FLAT, 0.5, 0, 30, -1,
                       Interval::NONE, 0, 0, 0,
                       Interval::NONE, 0, 0, 0,
                       false, 0, 0, 0);
    Interval interval5(QTime::fromString( "00:05:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Hard effort", 0),
                       Interval::FLAT, 1.00, 0, 20, -1,
                       Interval::FLAT, 90, 0, 10,
                       Interval::NONE, 0, 0, 0,
                       false, 0, 0, 0);
    Interval interval6(QTime::fromString( "00:05:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "5min Easy", 0),
                       Interval::FLAT, 0.5, 0, 30, -1,
                       Interval::NONE, 0, 0, 0,
                       Interval::NONE, 0, 0, 0,
                       false, 0, 0, 0);
    Interval interval7(QTime::fromString( "00:20:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "20' Test Interval - GO!", 0),
                       Interval::FLAT, 1.05, 0, 20, -1,
                       Interval::FLAT, 90, 0, 10,
                       Interval::NONE, 0, 0, 0,
                       true, 0, 0, 0);
    Interval interval8(QTime::fromString( "00:10:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Cooldown", 0),
                       Interval::FLAT, 0.5, 0, 30, -1,
                       Interval::NONE, 0, 0, 0,
                       Interval::NONE, 0, 0, 0,
                       false, 0, 0, 0);

    lstInterval.append(interval1);
    lstInterval.append(interval2);
    lstInterval.append(interval3);
    lstInterval.append(interval2);
    lstInterval.append(interval3);
    lstInterval.append(interval2);
    lstInterval.append(interval4);
    lstInterval.append(interval5);
    lstInterval.append(interval6);
    lstInterval.append(interval7);
    lstInterval.append(interval8);

    Workout workout("", Workout::FTP_TEST, lstInterval,
                    QApplication::translate("WorkoutUtil", "FTP Test", 0) , "MaximumTrainer",
                    QApplication::translate("WorkoutUtil", "Test that lets you find your FTP and LTHR: after the warm up, ride the 20 minute test interval as hard as you can sustain. FTP is 95% of your average test power; the result is shown as soon as the test interval ends.", 0),
                    "-", Workout::T_TEST);
    return workout;
}




//--------------------------------------------------------------------------------------------------------------------------------------------------------------------
Workout WorkoutUtil::FTP_8min() {


    QString name = QApplication::translate("WorkoutUtil", "FTP_8min Test", 0);
    QString description = QApplication::translate("WorkoutUtil", "Test that let you find your FTP", 0);
    QList<Interval> lstIntervalSource;
    QList<RepeatData> lstRepeat;


    Interval intervalW1(QTime::fromString( "00:10:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Warm up", 0),
                        Interval::PROGRESSIVE,  0.50, 0.65, 20, -1,
                        Interval::NONE, 0, 0, 0,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);
    Interval intervalW2(QTime::fromString( "00:00:20", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Warm up Interval", 0),
                        Interval::FLAT,  1.10, 0, 10, -1,
                        Interval::NONE, 0, 0, 0,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);
    Interval intervalW3(QTime::fromString( "00:00:40", "hh:mm:ss"), "",
                        Interval::FLAT,  0.65, 0, 10, -1,
                        Interval::NONE, 0, 0, 0,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);
    Interval intervalW4(QTime::fromString( "00:05:00", "hh:mm:ss"),  QApplication::translate("WorkoutUtil", "Warm up Steady", 0),
                        Interval::FLAT,  0.75, 0, 20, -1,
                        Interval::NONE, 0, 0, 0,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);
    Interval intervalTest(QTime::fromString( "00:08:00", "hh:mm:ss"),  QApplication::translate("WorkoutUtil", "8' Test Interval - GO!", 0),
                          Interval::FLAT,  1.15, 0, 20, -1,
                          Interval::NONE, 0, 0, 0,
                          Interval::NONE, 0, 0, 0,
                          true, 0, 0, 0);
    Interval intervalRest(QTime::fromString( "00:10:00", "hh:mm:ss"),  QApplication::translate("WorkoutUtil", "Rest", 0),
                          Interval::FLAT,  0.55, 0, 20, -1,
                          Interval::NONE, 0, 0, 0,
                          Interval::NONE, 0, 0, 0,
                          false, 0, 0, 0);
    Interval intervalCd(QTime::fromString( "00:10:00", "hh:mm:ss"), QApplication::translate("WorkoutUtil", "Cooldown", 0),
                        Interval::FLAT, 0.5, 0, 30, -1,
                        Interval::NONE, 0, 0, 0,
                        Interval::NONE, 0, 0, 0,
                        false, 0, 0, 0);


    lstIntervalSource.append(intervalW1);
    lstIntervalSource.append(intervalW2);
    lstIntervalSource.append(intervalW3);
    lstIntervalSource.append(intervalW4);
    lstIntervalSource.append(intervalTest);
    lstIntervalSource.append(intervalRest);
    lstIntervalSource.append(intervalTest);
    lstIntervalSource.append(intervalCd);


    //id, firstrow, lastrow, numberRepeat
    RepeatData repeat0(0, 1, 2, 5);
    lstRepeat.append(repeat0);

    Workout workout("", Workout::FTP8min_TEST, lstIntervalSource, lstRepeat,
                    name, "MaximumTrainer", description, "-", Workout::T_TEST);


    return workout;
}




