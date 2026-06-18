#ifndef MANAGERACHIEVEMENT_H
#define MANAGERACHIEVEMENT_H

#include <QObject>
#include <QTimer>

#include "workout.h"
#include "achievement.h"
#include "account.h"


class ManagerAchievement : public QObject
{
    Q_OBJECT

public:
    explicit ManagerAchievement(QObject *parent = 0);




public slots:

    /// Check for Level achievement
    void updateMinuteRode(int minutes);

    /// Check for 1h Workout, 2h Workout, 3h Workout, Target Maniac
    void workoutCompleted(Workout workout);

    void checkMAPAchievement(int lastStepCompleted);


signals:
    void achievementCompleted(Achievement achievement);




private :

    Account *account;

    // Position in QList is equals to DB ID - 1
    // e.g: lstAchievement.at(0) = first id in database (Endurance Starter, ID:1)
    QList<Achievement> lstAchievement;
    int numberOfAchievement;  // number of achievements in the DB at the moment, increase when adding more


    //    (1, 'Endurance Starter', 'Apprenti en Endurance', 'Complete a workout of 1h or more.', "Compléter un entraînement de 1h et plus.", 'https://maximumtrainer.com/assets/image/achievements/1.png'),
    //    (2, 'Endurance Intermediate', 'Intermédiaire en Endurance', 'Complete a workout of 2h or more.', "Compléter un entraînement de 2h et plus.", 'https://maximumtrainer.com/assets/image/achievements/1.png'),
    //    (3, 'Endurance Master', 'Maître en Endurance', 'Complete a workout of 3h or more.', "Compléter un entraînement de 3h et plus.", 'https://maximumtrainer.com/assets/image/achievements/1.png'),
    //    (4, 'Target Maniac', 'Maniaque de cibles', 'Complete a workout that has a: power target, power balance target, cadence target and heart rate target.', "Compléter un entraînement qui possède une cible de puissance, d'équilibre de puissance, de cadence et de fréquence cardiaque.", 'https://maximumtrainer.com/assets/image/achievements/1.png'),
    //    (5, 'Learning to test', 'Apprenti testeur', 'Complete your first FTP Test', "Compléter votre premier Test SFP", 'https://maximumtrainer.com/assets/image/achievements/1.png');


    //    (6, 'Learning to test', 'Apprenti testeur', 'Complete your first FTP Test', "Compléter votre premier Test SFP", 'https://maximumtrainer.com/assets/image/achievements/1.png');
    //    (7, 'Learning to test', 'Apprenti testeur', 'Complete your first FTP Test', "Compléter votre premier Test SFP", 'https://maximumtrainer.com/assets/image/achievements/1.png');


};
Q_DECLARE_METATYPE(ManagerAchievement*)






#endif // MANAGERACHIEVEMENT_H
