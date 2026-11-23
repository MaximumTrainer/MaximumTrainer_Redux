/*
 * Drives a real WorkoutDialog in bot-race mode: the race starts with the ride,
 * follows the rider's live power, steers the trainer by the road grade at 50 %
 * difficulty, and stands still while the ride is paused.
 */
#include <QApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include "account.h"
#include "interval.h"
#include "managerachievement.h"
#include "racecourse.h"
#include "racesimulation.h"
#include "radio.h"
#include "sensor.h"
#include "settings.h"
#include "soundplayer.h"
#include "trackpoint.h"
#include "userstudio.h"
#include "workout.h"
#include "workoutdialog.h"

namespace {
constexpr double kRiderKg = 72.0;
constexpr int kThreeWattsPerKg = 216;

Workout freeRide()
{
    QList<Interval> intervals;
    intervals.append(Interval());
    return Workout("null", Workout::OPEN_RIDE, intervals, "Race: Alpe d'Huez", "MaximumTrainer",
                   "Bot race", "-", Workout::T_OTHERS);
}
}

class TstWorkoutDialogRace : public QObject
{
    Q_OBJECT

private:
    Account *m_account = nullptr;

    void ride(WorkoutDialog *dialog, int watts, int milliseconds)
    {
        QElapsedTimer clock;
        clock.start();
        while (clock.elapsed() < milliseconds) {
            dialog->PowerDataReceived(1, watts);
            QTest::qWait(100);
        }
    }

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        qRegisterMetaType<Sensor>("Sensor");
        qRegisterMetaType<QList<Sensor>>("QList<Sensor>");
        qRegisterMetaType<QList<int>>("QList<int>");
        qRegisterMetaType<QVector<UserStudio>>("QVector<UserStudio>");
        qRegisterMetaType<QList<Trackpoint>>("QList<Trackpoint>");

        m_account = new Account(qApp);
        qApp->setProperty("Account", QVariant::fromValue<Account *>(m_account));
        qApp->setProperty("User_Settings", QVariant::fromValue<Settings *>(new Settings(qApp)));
        qApp->setProperty("SoundPlayer", QVariant::fromValue<SoundPlayer *>(new SoundPlayer(qApp)));
        qApp->setProperty("ManagerAchievement",
                          QVariant::fromValue<ManagerAchievement *>(new ManagerAchievement(qApp)));
    }

    void bundledCoursesAreInTheApp()
    {
        for (const char *id : {"longchamp", "alpe-dhuez", "mont-ventoux"})
            QVERIFY2(RaceCourse::fromFile(QStringLiteral(":/courses/%1.json").arg(QLatin1String(id))).has_value(), id);
    }

    void race_followsPowerAndSteersTheTrainer()
    {
        m_account->weight_kg = kRiderKg;
        m_account->bike_weight_kg = 8;
        m_account->control_trainer_resistance = true;
        m_account->virtual_shifting = false;
        m_account->enable_studio_mode = false;
        m_account->display_video = 0;
        m_account->isOffline = true;

        auto *dialog = new WorkoutDialog(freeRide(), QList<Radio>(), QVector<UserStudio>(), nullptr);
        QSignalSpy slopes(dialog, &WorkoutDialog::setSlope);
        QSignalSpy loads(dialog, &WorkoutDialog::setLoad);
        const auto course = RaceCourse::fromFile(QStringLiteral(":/courses/alpe-dhuez.json"));
        QVERIFY(course.has_value());
        dialog->setRace(*course, RaceBot::FieldSettings());
        QVERIFY(dialog->race());
        QCOMPARE(dialog->race()->riders().size(), 21);
        dialog->enableTrainerControl();

        // Nothing moves before the ride starts.
        ride(dialog, kThreeWattsPerKg, 600);
        QCOMPARE(dialog->race()->you().distanceM, 0.0);

        dialog->start_or_pause_workout();   // start
        ride(dialog, kThreeWattsPerKg, 3500);
        const double afterRiding = dialog->race()->you().distanceM;
        QVERIFY2(afterRiding > 5.0, qPrintable(QString::number(afterRiding)));
        QCOMPARE(dialog->race()->you().powerW, double(kThreeWattsPerKg));

        // The trainer gets the grade under you at half difficulty, never ERG.
        QVERIFY(!slopes.isEmpty());
        QVERIFY(loads.isEmpty());
        const double grade = course->gradeAt(dialog->race()->you().distanceM) * 100.0;
        QVERIFY2(std::abs(slopes.last().at(1).toDouble() - 0.5 * grade) < 0.5,
                 qPrintable(QStringLiteral("sent %1 %, road %2 %").arg(slopes.last().at(1).toDouble()).arg(grade)));

        dialog->start_or_pause_workout();   // pause
        const double atPause = dialog->race()->you().distanceM;
        ride(dialog, kThreeWattsPerKg, 1200);
        QCOMPARE(dialog->race()->you().distanceM, atPause);

        dialog->start_or_pause_workout();   // resume
        ride(dialog, kThreeWattsPerKg, 1200);
        QVERIFY(dialog->race()->you().distanceM > atPause);

        // No power for more than 3 s counts as freewheeling.
        QTest::qWait(3800);
        QCOMPARE(dialog->race()->you().powerW, 0.0);
        delete dialog;
    }
};

QTEST_MAIN(TstWorkoutDialogRace)
#include "tst_workout_dialog_race.moc"
