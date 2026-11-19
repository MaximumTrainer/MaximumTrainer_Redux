#include <QtTest>
#include <QTemporaryDir>
#include <fstream>

#include "fit_decode.hpp"
#include "fit_mesg_broadcaster.hpp"
#include "fit_workout_mesg_listener.hpp"
#include "fit_workout_step_mesg_listener.hpp"
#include "account.h"
#include "fitworkoutwriter.h"
#include "workout.h"

namespace {
Interval power(int minutes, const char *name, Interval::StepType type, double start, double end,
               int rangeWatts, int cadence = -1)
{
    return Interval(QTime(0, minutes, 0), QString::fromLatin1(name), type, start, end, rangeWatts, -1,
                    cadence > 0 ? Interval::FLAT : Interval::NONE, cadence, cadence, 5,
                    Interval::NONE, -1, -1, 0, false, 0, 0, 0);
}

Workout sample()
{
    QList<Interval> intervals;
    intervals << power(10, "Warm-up", Interval::PROGRESSIVE, 0.50, 0.70, 10)   // ramp -> 10 steps
              << power(8, "Threshold", Interval::FLAT, 0.95, 0.95, 10, 90)
              << power(4, "Recover", Interval::FLAT, 0.50, 0.50, 10)
              << power(5, "Cool-down", Interval::FLAT, 0.55, 0.55, 10);
    return Workout(QString(), Workout::USER_MADE, intervals, QStringLiteral("Export Test"),
                   QStringLiteral("CI"), QStringLiteral("A test"), QStringLiteral("Plan"), Workout::T_INTERVAL);
}

struct Reader : fit::WorkoutMesgListener, fit::WorkoutStepMesgListener {
    QString name;
    int validSteps = -1;
    QVector<fit::WorkoutStepMesg> steps;
    void OnMesg(fit::WorkoutMesg &m) override
    {
        name = QString::fromStdWString(m.GetWktName());
        validSteps = m.GetNumValidSteps();
    }
    void OnMesg(fit::WorkoutStepMesg &m) override { steps.append(m); }
};
} // namespace

class TstWorkoutExport : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        auto *account = new Account(this);
        account->FTP = 200;
        account->LTHR = 170;
        account->isOffline = true;
        qApp->setProperty("Account", QVariant::fromValue<Account *>(account));
    }

    void steps_flatRampAndIntensity()
    {
        const auto steps = FitWorkoutWriter::buildSteps(sample(), 200, 170);
        QCOMPARE(steps.size(), 10 + 3);
        // Ramp 50 -> 70 % in ten 1-minute slices, each at its mid-point.
        QCOMPARE(steps.first().seconds, 60);
        QCOMPARE((steps.first().powerLowPct + steps.first().powerHighPct) / 2, 51);
        QCOMPARE((steps.at(9).powerLowPct + steps.at(9).powerHighPct) / 2, 69);
        QCOMPARE(steps.first().intensity, FitWorkoutWriter::Intensity::Warmup);
        // 10 W range at 200 W FTP is 5 % either side.
        const auto &threshold = steps.at(10);
        QCOMPARE(threshold.seconds, 480);
        QCOMPARE(threshold.powerLowPct, 90);
        QCOMPARE(threshold.powerHighPct, 100);
        QVERIFY(threshold.hasCadence);
        QCOMPARE(threshold.cadenceLow, 85);
        QCOMPARE(threshold.cadenceHigh, 95);
        QCOMPARE(threshold.intensity, FitWorkoutWriter::Intensity::Active);
        QCOMPARE(steps.at(11).intensity, FitWorkoutWriter::Intensity::Recovery);
        QCOMPARE(steps.last().intensity, FitWorkoutWriter::Intensity::Cooldown);
        int total = 0;
        for (const auto &step : steps)
            total += step.seconds;
        QCOMPARE(total, 27 * 60);
    }

    void write_decodesAsAFitWorkout()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("export.fit"));
        QString error;
        QVERIFY2(FitWorkoutWriter::write(sample(), 200, 170, path, &error), qPrintable(error));

        std::fstream file(path.toStdString(), std::ios::in | std::ios::binary);
        fit::Decode decode;
        QVERIFY(decode.CheckIntegrity(file));
        file.clear();
        file.seekg(0);
        fit::MesgBroadcaster broadcaster;
        Reader reader;
        broadcaster.AddListener(static_cast<fit::WorkoutMesgListener &>(reader));
        broadcaster.AddListener(static_cast<fit::WorkoutStepMesgListener &>(reader));
        QVERIFY(decode.Read(file, broadcaster));

        QCOMPARE(reader.name, QStringLiteral("Export Test"));
        QCOMPARE(reader.validSteps, 13);
        QCOMPARE(reader.steps.size(), 13);
        const auto &threshold = reader.steps.at(10);
        QCOMPARE(threshold.GetDurationType(), FIT_WKT_STEP_DURATION_TIME);
        QCOMPARE(qRound(threshold.GetDurationTime()), 480);
        QCOMPARE(threshold.GetTargetType(), FIT_WKT_STEP_TARGET_POWER);
        QCOMPARE(int(threshold.GetCustomTargetPowerLow()), 90);
        QCOMPARE(int(threshold.GetCustomTargetPowerHigh()), 100);
        QCOMPARE(threshold.GetSecondaryTargetType(), FIT_WKT_STEP_TARGET_CADENCE);
    }

    void write_rejectsAnEmptyWorkout()
    {
        QTemporaryDir dir;
        QString error;
        QVERIFY(!FitWorkoutWriter::write(Workout(), 200, 170, dir.filePath(QStringLiteral("e.fit")), &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TstWorkoutExport)
#include "tst_workout_export.moc"
