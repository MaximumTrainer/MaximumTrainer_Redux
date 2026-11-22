/*
 * Drives a real WorkoutDialog with a controllable trainer and checks the
 * commands it sends on pause / resume, with the low-cadence ERG release
 * ("ERG rescue") on and off. Regression guard for v0.0.324, where a resume
 * left the trainer in free ride, so watts climbed with cadence instead of
 * holding the ERG target.
 */
#include <QApplication>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

#include "account.h"
#include "interval.h"
#include "managerachievement.h"
#include "radio.h"
#include "sensor.h"
#include "settings.h"
#include "soundplayer.h"
#include "trackpoint.h"
#include "userstudio.h"
#include "workout.h"
#include "workoutdialog.h"

Q_IMPORT_QML_PLUGIN(MaximumTrainer_GamePlugin)

namespace {
constexpr int kFtpWatts = 250;
constexpr double kTargetShareOfFtp = 0.64;
constexpr int kTargetWatts = 160;   // 0.64 * 250

Workout makeErgWorkout()
{
    Interval steady;
    steady.setTime(QTime(0, 20, 0));
    steady.setPowerStepType(Interval::FLAT);
    steady.setTargetFTP_start(kTargetShareOfFtp);
    steady.setTargetFTP_end(kTargetShareOfFtp);
    steady.setTargetFTP_range(5);
    steady.setRightPowerTarget(-1.0);
    return Workout("erg_test.workout", Workout::USER_MADE, {steady}, "ERG test", "MaximumTrainer",
                   "ERG pause/resume", "Base", Workout::T_ENDURANCE);
}

double lastLoad(const QSignalSpy &spy)
{
    return spy.isEmpty() ? -1.0 : spy.last().at(1).toDouble();
}
}

class TstWorkoutDialogErg : public QObject
{
    Q_OBJECT

private:
    Account *m_account = nullptr;
    WorkoutDialog *m_dialog = nullptr;
    QSignalSpy *m_loads = nullptr;
    QSignalSpy *m_slopes = nullptr;
    QSignalSpy *m_resistances = nullptr;

    void openDialog(bool ergRescue, bool virtualShifting, int rampSeconds)
    {
        m_account->FTP = kFtpWatts;
        m_account->control_trainer_resistance = true;
        m_account->erg_low_cadence_release = ergRescue;
        m_account->virtual_shifting = virtualShifting;
        m_account->erg_smoothing_duration_s = rampSeconds;
        m_account->enable_studio_mode = false;
        m_account->display_video = 0;
        m_account->isOffline = true;

        m_dialog = new WorkoutDialog(makeErgWorkout(), QList<Radio>(), QVector<UserStudio>(), nullptr);
        m_loads = new QSignalSpy(m_dialog, &WorkoutDialog::setLoad);
        m_slopes = new QSignalSpy(m_dialog, &WorkoutDialog::setSlope);
        m_resistances = new QSignalSpy(m_dialog, &WorkoutDialog::setResistance);
        m_dialog->setResistanceLevelSupported(virtualShifting);
        m_dialog->enableTrainerControl();

        m_dialog->start_or_pause_workout();
        QTRY_COMPARE_WITH_TIMEOUT(lastLoad(*m_loads), double(kTargetWatts), rampSeconds * 1000 + 3000);
        clearSpies();
    }

    void clearSpies()
    {
        m_loads->clear();
        m_slopes->clear();
        m_resistances->clear();
    }

    bool sentFreeRide() const { return !m_slopes->isEmpty() || !m_resistances->isEmpty(); }

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

    void cleanup()
    {
        delete m_loads;
        delete m_slopes;
        delete m_resistances;
        m_loads = m_slopes = m_resistances = nullptr;
        delete m_dialog;
        m_dialog = nullptr;
    }

    void ergRescueIsOffByDefault()
    {
        Account freshAccount;
        QVERIFY(!freshAccount.erg_low_cadence_release);
    }

    // ERG off in Devices: the trainer must never get a target-power command,
    // whether the rider uses virtual gears or a flat road.
    void ergOff_neverSendsErgCommand_data()
    {
        QTest::addColumn<bool>("virtualShifting");
        QTest::addColumn<bool>("ergRescue");
        QTest::newRow("virtual gears") << true << false;
        QTest::newRow("virtual gears, rescue on") << true << true;
        QTest::newRow("flat road") << false << false;
        QTest::newRow("flat road, rescue on") << false << true;
    }

    void ergOff_neverSendsErgCommand()
    {
        QFETCH(bool, virtualShifting);
        QFETCH(bool, ergRescue);
        m_account->FTP = kFtpWatts;
        m_account->control_trainer_resistance = false;
        m_account->erg_low_cadence_release = ergRescue;
        m_account->virtual_shifting = virtualShifting;
        m_account->erg_smoothing_duration_s = 3;
        m_account->enable_studio_mode = false;
        m_account->display_video = 0;
        m_account->isOffline = true;

        m_dialog = new WorkoutDialog(makeErgWorkout(), QList<Radio>(), QVector<UserStudio>(), nullptr);
        m_loads = new QSignalSpy(m_dialog, &WorkoutDialog::setLoad);
        m_slopes = new QSignalSpy(m_dialog, &WorkoutDialog::setSlope);
        m_resistances = new QSignalSpy(m_dialog, &WorkoutDialog::setResistance);
        m_dialog->setResistanceLevelSupported(true);
        m_dialog->enableTrainerControl();

        m_dialog->start_or_pause_workout();   // start
        QTest::qWait(2500);                   // clock ticks re-send the gear
        m_dialog->start_or_pause_workout();   // pause
        m_dialog->start_or_pause_workout();   // resume
        QTest::qWait(1500);
        QVERIFY(sentFreeRide());
        QVERIFY2(m_loads->isEmpty(), "ERG off sent a target-power (ERG) command");
        delete m_dialog;                      // closing must not send one either
        m_dialog = nullptr;
        QVERIFY2(m_loads->isEmpty(), "ERG off sent a target-power (ERG) command on close");
    }

    void rescueOff_pauseAndResumeKeepErg()
    {
        openDialog(false, false, 0);

        m_dialog->start_or_pause_workout();   // pause
        QVERIFY(!sentFreeRide());
        m_dialog->start_or_pause_workout();   // resume
        QTest::qWait(300);
        QVERIFY(!sentFreeRide());
        for (const QList<QVariant> &load : std::as_const(*m_loads))
            QCOMPARE(load.at(1).toDouble(), double(kTargetWatts));
    }

    void rescueOn_resumeRestoresErgAtOnce()
    {
        openDialog(true, false, 0);

        m_dialog->start_or_pause_workout();   // pause: free ride on a flat road
        QCOMPARE(m_slopes->size(), 1);
        QCOMPARE(m_slopes->last().at(1).toDouble(), 0.0);
        clearSpies();

        m_dialog->start_or_pause_workout();   // resume: ERG back with no recovery wait
        QVERIFY(!m_loads->isEmpty());
        QCOMPARE(lastLoad(*m_loads), double(kTargetWatts));
        QVERIFY(!sentFreeRide());
    }

    void rescueOn_virtualGears_resumeRestoresErg()
    {
        openDialog(true, true, 0);

        m_dialog->start_or_pause_workout();   // pause: free ride on the virtual gear
        QVERIFY(!m_resistances->isEmpty());
        QVERIFY(m_loads->isEmpty());
        clearSpies();

        m_dialog->start_or_pause_workout();   // resume
        QCOMPARE(lastLoad(*m_loads), double(kTargetWatts));
        QTest::qWait(1500);   // a clock tick must not put the gear back in charge
        QVERIFY(m_resistances->isEmpty());
        QCOMPARE(lastLoad(*m_loads), double(kTargetWatts));
    }

    void rescueOn_resumeRampsUpToTarget()
    {
        openDialog(true, false, 3);

        m_dialog->start_or_pause_workout();   // pause
        clearSpies();
        m_dialog->start_or_pause_workout();   // resume
        QVERIFY(!m_loads->isEmpty());
        QVERIFY(m_loads->first().at(1).toDouble() < kTargetWatts);
        QTRY_COMPARE_WITH_TIMEOUT(lastLoad(*m_loads), double(kTargetWatts), 6000);
        QVERIFY(!sentFreeRide());
    }

    void rescueOn_secondPauseReleasesAgain()
    {
        openDialog(true, false, 0);

        for (int cycle = 0; cycle < 2; ++cycle) {
            clearSpies();
            m_dialog->start_or_pause_workout();   // pause
            QCOMPARE(m_slopes->size(), 1);
            clearSpies();
            m_dialog->start_or_pause_workout();   // resume
            QCOMPARE(lastLoad(*m_loads), double(kTargetWatts));
        }
    }
};

QTEST_MAIN(TstWorkoutDialogErg)
#include "tst_workout_dialog_erg.moc"
