/*
 * Drives a real WorkoutDialog in bot-race mode: the race starts with the ride,
 * follows the rider's live power, steers the trainer by the road grade at 50 %
 * difficulty, and stands still while the ride is paused.
 */
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include "account.h"
#include "interval.h"
#include "managerachievement.h"
#include "racecourse.h"
#include "racesimulation.h"
#include "achievementcatalog.h"
#include "raceview.h"
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

    // A sprint: 50 bots, you at 900 W, the view redrawn every 40 ms tick.
    // Each frame (one race step plus one full redraw) must stay well inside
    // the 40 ms budget, or the race would stutter right when it matters.
    void raceView_keepsUpWithASprint()
    {
        const auto course = RaceCourse::fromFile(QStringLiteral(":/courses/alpe-dhuez.json"));
        QVERIFY(course.has_value());
        RaceSimulation race(*course, QStringLiteral("You"), kRiderKg, 8.0, 0.30);
        for (const RaceBot &bot : RaceBot::field({RaceBot::kMaxBots, 2.2, 4.0}, 9))
            race.addBot(bot);
        for (int tick = 0; tick < 4 * 600; ++tick) {   // ten minutes in, the bunch has formed
            race.setYourPower(kThreeWattsPerKg);
            race.step(0.25);
        }
        RaceView view(&race);
        view.resize(1350, 640);
        QImage frame(view.size(), QImage::Format_ARGB32_Premultiplied);

        constexpr int kFrames = 150;
        QElapsedTimer clock;
        clock.start();
        for (int index = 0; index < kFrames; ++index) {
            race.setYourPower(900);
            race.step(0.04);
            view.render(&frame);
        }
        const double frameMs = clock.nsecsElapsed() / 1e6 / kFrames;
        qInfo("sprint with %d bots: %.2f ms per frame (budget 40 ms)", RaceBot::kMaxBots, frameMs);
        QVERIFY2(frameMs < 20.0, qPrintable(QStringLiteral("%1 ms per frame").arg(frameMs)));
    }

    // The bottom pane (metric widgets) must open at the height it had when
    // the last workout was closed, not grow back to the default.
    void bottomPane_keepsItsHeightAcrossWorkouts_data()
    {
        QTest::addColumn<bool>("firstIsCourse");
        QTest::addColumn<bool>("secondIsCourse");
        QTest::newRow("workout then workout") << false << false;
        QTest::newRow("workout then course") << false << true;
        QTest::newRow("course then course") << true << true;
        QTest::newRow("course then workout") << true << false;
    }

    void bottomPane_keepsItsHeightAcrossWorkouts()
    {
        QFETCH(bool, firstIsCourse);
        QFETCH(bool, secondIsCourse);
        const auto course = RaceCourse::fromFile(QStringLiteral(":/courses/longchamp.json"));
        m_account->enable_studio_mode = false;
        m_account->isOffline = true;
        constexpr int kBottomPx = 130;

        auto *first = new WorkoutDialog(freeRide(), QList<Radio>(), QVector<UserStudio>(), nullptr);
        if (firstIsCourse)
            first->setRace(*course, RaceBot::FieldSettings());
        first->show();
        first->resize(1280, 820);
        QTest::qWait(300);
        auto *splitter = first->findChild<QSplitter *>(QStringLiteral("splitter"));
        QVERIFY(splitter);
        QList<int> sizes = splitter->sizes();
        const int total = sizes.at(1) + sizes.at(2) + sizes.at(3);
        sizes[3] = kBottomPx;
        sizes[1] = firstIsCourse ? total - kBottomPx : (total - kBottomPx) / 2;
        sizes[2] = total - kBottomPx - sizes[1];
        splitter->setSizes(sizes);
        QTest::qWait(100);
        const int setTo = splitter->sizes().at(3);
        QMetaObject::invokeMethod(first, "closeWindow");
        delete first;

        auto *second = new WorkoutDialog(freeRide(), QList<Radio>(), QVector<UserStudio>(), nullptr);
        if (secondIsCourse)
            second->setRace(*course, RaceBot::FieldSettings());
        second->show();
        QTest::qWait(300);
        // The window manager maximises or goes fullscreen after the window
        // appears: the extra height must go to the video/race area.
        second->resize(1600, 1100);
        QTest::qWait(300);
        const int reopened = second->findChild<QSplitter *>(QStringLiteral("splitter"))->sizes().at(3);
        qInfo("bottom pane: closed at %d px, reopened at %d px", setTo, reopened);
        QVERIFY2(std::abs(reopened - setTo) <= 4,
                 qPrintable(QStringLiteral("closed at %1 px, reopened at %2 px").arg(setTo).arg(reopened)));
        delete second;
    }

    // In a bunch your dot keeps its lane: it only moves over when the lane is
    // taken, one lane at a time, and doesn't flick from side to side.
    void bunch_ridersKeepTheirLane()
    {
        const auto course = RaceCourse::fromFile(QStringLiteral(":/courses/longchamp.json"));
        RaceSimulation race(*course, QStringLiteral("You"), kRiderKg, 8.0, 0.30);
        for (const RaceBot &bot : RaceBot::defaultField(5))
            race.addBot(bot);
        RaceView view(&race);
        view.resize(1350, 640);
        QImage frame(view.size(), QImage::Format_ARGB32_Premultiplied);

        int changes = 0;
        int previous = -1;
        for (int tick = 0; tick < 25 * 120; ++tick) {   // two minutes at 25 frames a second
            race.setYourPower(kThreeWattsPerKg);
            race.step(0.04);
            view.render(&frame);
            const int lane = view.laneOf(0);
            QVERIFY(lane >= 0 && lane <= 3);
            if (previous >= 0 && lane != previous) {
                ++changes;
                QVERIFY2(std::abs(lane - previous) == 1, "jumped more than one lane");
            }
            previous = lane;
        }
        qInfo("your lane changed %d times in two minutes of bunch riding", changes);
        QVERIFY2(changes <= 12, qPrintable(QStringLiteral("%1 lane changes").arg(changes)));
    }

    void everyCourseHasAnAchievement()
    {
        const QDir courses(QStringLiteral(":/courses"));
        const QStringList files = courses.entryList({QStringLiteral("*.json")}, QDir::Files);
        QVERIFY(!files.isEmpty());
        for (const QString &file : files) {
            const auto course = RaceCourse::fromFile(courses.filePath(file));
            QVERIFY(course.has_value());
            const auto *definition = AchievementCatalog::find(AchievementCatalog::Events::courseFinished(course->id()));
            QVERIFY2(definition, qPrintable(course->id()));
            QCOMPARE(definition->category, AchievementCatalog::Category::Courses);
        }
    }

    // Crossing the line of a course earns its achievement, once.
    void finishingACourse_earnsItsAchievement()
    {
        m_account->weight_kg = kRiderKg;
        m_account->bike_weight_kg = 8;
        m_account->enable_studio_mode = false;
        m_account->isOffline = true;
        // A 60 m stretch carrying a real course id, so the ride is short.
        const auto shortCourse = RaceCourse::fromJson(
            R"({"id":"longchamp","name":"Test","place":"Here","laps":1,"loop":false,)"
            R"("points":[[0,48.86,2.24,40],[30,48.8603,2.2401,40],[60,48.8606,2.2402,40]]})");
        QVERIFY(shortCourse.has_value());

        auto *manager = qApp->property("ManagerAchievement").value<ManagerAchievement *>();
        QSignalSpy earned(manager, &ManagerAchievement::achievementCompleted);
        auto *dialog = new WorkoutDialog(freeRide(), QList<Radio>(), QVector<UserStudio>(), nullptr);
        dialog->setRace(*shortCourse, RaceBot::FieldSettings{1, 2.0, 2.0});
        dialog->start_or_pause_workout();
        QElapsedTimer clock;
        clock.start();
        while (!dialog->race()->youFinished() && clock.elapsed() < 30000) {
            dialog->PowerDataReceived(1, 400);
            QTest::qWait(100);
        }
        QVERIFY(dialog->race()->youFinished());
        ride(dialog, 400, 600);   // more ticks after the line: still only one award

        int forThisCourse = 0;
        for (const QList<QVariant> &signal : std::as_const(earned))
            if (signal.at(0).value<Achievement>().getName() == QStringLiteral("Longchamp Regular"))
                ++forThisCourse;
        QCOMPARE(forThisCourse, 1);
        delete dialog;
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
        auto *startButton = dialog->findChild<QPushButton *>(QStringLiteral("pushButton_start"));
        QVERIFY(startButton);
        QCOMPARE(startButton->text(), QStringLiteral("Start Course"));
        auto *title = dialog->findChild<QLabel *>(QStringLiteral("label_nameWorkout"));
        QVERIFY(title);
        QCOMPARE(title->text(), course->name());
        for (int index = 1; index < dialog->race()->riders().size(); ++index)
            QCOMPARE(dialog->race()->gapToYouSec(index), 0.0);   // all on the start line
        dialog->enableTrainerControl();

        // Warming up: your W/kg and the bots' show, but nothing moves.
        ride(dialog, kThreeWattsPerKg, 600);
        QCOMPARE(dialog->race()->you().distanceM, 0.0);
        QCOMPARE(dialog->race()->you().powerW, double(kThreeWattsPerKg));
        for (const RaceSimulation::Rider &rider : dialog->race()->riders())
            QVERIFY2(rider.powerW > 0.0, qPrintable(rider.name));

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
