/*
 * Drives a real WorkoutDialog with a controllable trainer and checks the
 * commands it sends on pause / resume. Regression guard for v0.0.324, where a resume
 * left the trainer in free ride, so watts climbed with cadence instead of
 * holding the ERG target.
 */
#include <QApplication>
#include <QLabel>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include "account.h"
#include "cadenceeditor.h"
#include "hreditor.h"
#include "interval.h"
#include "managerachievement.h"
#include "powereditor.h"
#include "radio.h"
#include "radiotablemodel.h"
#include "sensorstatusstrip.h"
#ifndef Q_OS_WASM
#include "device_session.h"
#endif
#include "sensor.h"
#include "settings.h"
#include "soundplayer.h"
#include "trackpoint.h"
#include "userstudio.h"
#include "workout.h"
#include "workoutdialog.h"
#include "workoutplotzoomer.h"

#include <qwt_scale_div.h>
#ifndef Q_OS_WASM
#include "zwift_click_manager.h"
#endif

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

    void openDialog(bool virtualShifting, int rampSeconds)
    {
        m_account->FTP = kFtpWatts;
        m_account->control_trainer_resistance = true;
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

    // ERG off in Devices: the trainer must never get a target-power command,
    // whether the rider uses virtual gears or a flat road.
    void ergOff_neverSendsErgCommand_data()
    {
        QTest::addColumn<bool>("virtualShifting");
        QTest::newRow("virtual gears") << true;
        QTest::newRow("flat road") << false;
    }

    void ergOff_neverSendsErgCommand()
    {
        QFETCH(bool, virtualShifting);
        m_account->FTP = kFtpWatts;
        m_account->control_trainer_resistance = false;
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

    void pauseAndResumeKeepErg()
    {
        openDialog(false, 0);

        m_dialog->start_or_pause_workout();   // pause
        QVERIFY(!sentFreeRide());
        m_dialog->start_or_pause_workout();   // resume
        QTest::qWait(300);
        QVERIFY(!sentFreeRide());
        for (const QList<QVariant> &load : std::as_const(*m_loads))
            QCOMPARE(load.at(1).toDouble(), double(kTargetWatts));
    }

    // Enter belongs to text fields; Space starts and pauses.
    // A single 0 W packet late in an interval used to stretch the power graph
    // down to 0, leaving the target band as a thin strip at the top.
    void powerGraphKeepsTheTargetBandInViewAfterADropout()
    {
        openDialog(false, 0);
        auto *zoomer = m_dialog->findChild<WorkoutPlotZoomer *>(QStringLiteral("wid_2_workoutPlot_PowerZoom"));
        QVERIFY(zoomer);
        double timeSec = 600.0;
        for (int sample = 0; sample < 20; ++sample, timeSec += 0.5) {
            zoomer->updateCurve(timeSec, kTargetWatts);
            zoomer->updateTextLabelValue(kTargetWatts);
        }
        zoomer->updateCurve(timeSec, 0);
        zoomer->updateTextLabelValue(0);
        zoomer->updateCurve(timeSec + 0.5, kTargetWatts);
        zoomer->updateTextLabelValue(kTargetWatts);
        zoomer->moveIntervalTime((timeSec + 0.5) * 1000.0);

        const QwtScaleDiv axis = zoomer->axisScaleDiv(QwtPlot::yLeft);
        const double share = (kTargetWatts - axis.lowerBound()) / (axis.upperBound() - axis.lowerBound());
        QVERIFY2(axis.lowerBound() > 0.0, qPrintable(QString::number(axis.lowerBound())));
        QVERIFY2(share >= 0.25 && share <= 0.75, qPrintable(QString::number(share)));
    }

    void radioListMovesAStationUpAndDown()
    {
        RadioTableModel model;
        const auto station = [](const QString &name) { return Radio(name, QString(), false, 128, QString(), QString()); };
        model.setRadios({station("A"), station("B"), station("C")});
        const auto names = [&model]() {
            QStringList list;
            for (const Radio &radio : model.getAllRadios())
                list << radio.getName();
            return list.join(',');
        };
        QVERIFY(model.moveRadio(2, -1));
        QCOMPARE(names(), QStringLiteral("A,C,B"));
        QVERIFY(model.moveRadio(0, +1));
        QCOMPARE(names(), QStringLiteral("C,A,B"));
        QVERIFY(!model.moveRadio(0, -1));
        QVERIFY(!model.moveRadio(2, +1));
        QCOMPARE(names(), QStringLiteral("C,A,B"));
    }

#ifndef Q_OS_WASM
    // Studio and simulated sensors keep their per-ride flow: the app-wide
    // session then holds no device, and nothing paired means nothing to connect.
    void deviceSession_holdsNothingOutsideSoloLive()
    {
        m_account->use_zwift_click = false;
        m_account->rowing_mode = false;
        DeviceSession session(m_account);

        m_account->use_simulated_sensors = true;
        session.reconfigure();
        QCOMPARE(session.mode(), DeviceSession::Mode::Simulated);
        QVERIFY(session.items().isEmpty());
        QVERIFY(!session.connector());

        m_account->use_simulated_sensors = false;
        m_account->enable_studio_mode = true;
        session.reconfigure();
        QCOMPARE(session.mode(), DeviceSession::Mode::Studio);
        QVERIFY(!session.connector());

        m_account->enable_studio_mode = false;
        session.reconfigure();
        QCOMPARE(session.mode(), DeviceSession::Mode::NothingPaired);

        session.release();
        QCOMPARE(session.mode(), DeviceSession::Mode::Released);
        session.reconnect();
        QCOMPARE(session.mode(), DeviceSession::Mode::NothingPaired);
    }
#endif

    void sensorStrip_staysVisibleWithoutAutoHide()
    {
        SensorStatusStrip strip;
        strip.setAutoHide(false);
        strip.show();
        strip.addSensor(1, QStringLiteral("Trainer"));
        strip.setState(1, SensorStatusStrip::State::Connected);
        QTest::qWait(5300);
        QVERIFY(strip.isVisible());
        strip.clear();
        QVERIFY(strip.chipText(1).isEmpty());
    }

    void enterIsNotAHotkey()
    {
        openDialog(false, 0);   // started
        m_dialog->show();
        QSignalSpy pauses(m_dialog, &WorkoutDialog::pauseClock);
        QTest::keyClick(m_dialog, Qt::Key_Return);
        QTest::keyClick(m_dialog, Qt::Key_Enter);
        QCOMPARE(pauses.count(), 0);
        QVERIFY(m_dialog->isVisible());
        QTest::keyClick(m_dialog, Qt::Key_Space);
        QCOMPARE(pauses.count(), 1);
    }

    // Starting a Bluetooth scan spins an event loop on Windows, so a half-built
    // dialog would get clock data (crash of 2026-11-25).
    void constructorStartsNoBluetoothScan()
    {
        m_account->use_zwift_click = true;
        m_account->enable_studio_mode = false;
        m_dialog = new WorkoutDialog(makeErgWorkout(), QList<Radio>(), QVector<UserStudio>(), nullptr);
        m_account->use_zwift_click = false;
        QVERIFY(m_dialog->findChildren<ZwiftClickManager *>().isEmpty());
    }

    // A progressive step shows "to" between its two values, uncut. The wide font
    // stands in for Windows scaling.
    void progressiveEditorsShowWholeToLabel()
    {
        Interval ramp;
        ramp.setTime(QTime(0, 5, 0));
        ramp.setPowerStepType(Interval::PROGRESSIVE);
        ramp.setCadenceStepType(Interval::PROGRESSIVE);
        ramp.setHrStepType(Interval::PROGRESSIVE);
        ramp.setRightPowerTarget(0.5);

        PowerEditor power;
        CadenceEditor cadence;
        HrEditor heartRate;
        power.setInterval(ramp);
        cadence.setInterval(ramp);
        heartRate.setInterval(ramp);

        QFont wideFont = QApplication::font();
        wideFont.setPointSize(13);
        QList<QPair<QWidget *, QString>> editors = {{&power, "label_toFtpTxt"},
                                                    {&cadence, "label_toCadenceTxt"},
                                                    {&heartRate, "label_toLthrTxt"}};
        for (const auto &[editor, labelName] : editors) {
            editor->setFont(wideFont);
            editor->resize(350, 90);
            editor->show();
            QVERIFY(QTest::qWaitForWindowExposed(editor));
            auto *label = editor->findChild<QLabel *>(labelName);
            QVERIFY2(label && label->isVisible(), qPrintable(labelName));
            QVERIFY2(label->width() >= label->sizeHint().width(), qPrintable(labelName));
        }
    }

};

QTEST_MAIN(TstWorkoutDialogErg)
#include "tst_workout_dialog_erg.moc"
