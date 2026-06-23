#include <QtTest>
#include <QTemporaryDir>
#include <QDateTime>

#include <fstream>
#include <cmath>

#include "fit_encode.hpp"
#include "fit_file_id_mesg.hpp"
#include "fit_session_mesg.hpp"
#include "fit_lap_mesg.hpp"
#include "fit_record_mesg.hpp"

#include "fitactivityreader.h"
#include "fitactivitydetailreader.h"
#include "pmccalculator.h"

// Garmin FIT epoch (1989-12-31 UTC) offset from the Unix epoch, in seconds.
static constexpr qint64 FIT_EPOCH_OFFSET = 631065600LL;

class TestHistory : public QObject
{
    Q_OBJECT

private:
    // Writes a synthetic but valid activity FIT file with a session summary,
    // one lap, and `nRecords` per-second records. Returns the FIT start time
    // (seconds since the FIT epoch) so callers can assert the decoded date.
    static qint64 writeSampleFit(const QString &path, int nRecords)
    {
        const QDateTime start = QDateTime(QDate(2024, 1, 15), QTime(10, 0, 0), Qt::UTC);
        const qint64 fitStart = start.toSecsSinceEpoch() - FIT_EPOCH_OFFSET;

        std::fstream file;
        file.open(path.toStdString(), std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);

        fit::Encode encode;
        encode.Open(file);

        fit::FileIdMesg fileId;
        fileId.SetTimeCreated(fitStart);
        fileId.SetManufacturer(FIT_MANUFACTURER_DEVELOPMENT);
        fileId.SetType(FIT_FILE_ACTIVITY);
        encode.Write(fileId);

        for (int i = 0; i < nRecords; ++i) {
            fit::RecordMesg rec;
            rec.SetTimestamp(fitStart + i);
            rec.SetPower(200 + i);
            rec.SetHeartRate(140 + i);
            rec.SetCadence(90);
            rec.SetDistance(static_cast<FIT_FLOAT32>(i * 10));  // metres
            encode.Write(rec);
        }

        fit::LapMesg lap;
        lap.SetTotalElapsedTime(static_cast<FIT_FLOAT32>(nRecords));
        lap.SetAvgPower(205);
        lap.SetMaxPower(260);
        lap.SetNormalizedPower(215);
        lap.SetAvgHeartRate(150);
        lap.SetMaxHeartRate(175);
        lap.SetAvgCadence(92);
        lap.SetTotalDistance(static_cast<FIT_FLOAT32>(nRecords * 10));
        encode.Write(lap);

        fit::SessionMesg session;
        session.SetStartTime(fitStart);
        session.SetTimestamp(fitStart + nRecords);
        session.SetTotalTimerTime(static_cast<FIT_FLOAT32>(nRecords));
        session.SetSport(FIT_SPORT_CYCLING);
        session.SetAvgPower(205);
        session.SetNormalizedPower(215);
        session.SetAvgHeartRate(150);
        session.SetAvgCadence(92);
        session.SetTrainingStressScore(85.0f);
        session.SetTotalDistance(static_cast<FIT_FLOAT32>(nRecords * 10));  // metres
        session.SetTotalCalories(450);
        encode.Write(session);

        encode.Close();
        file.close();
        return fitStart;
    }

private slots:

    // ── FIT indexing: session summary ────────────────────────────────────────
    void summaryReadsSessionFields()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("activity.fit");
        writeSampleFit(path, 60);

        const WorkoutHistorySummary s = FitActivityReader::readFile(path);

        QVERIFY(s.valid);
        QCOMPARE(s.durationSec, 60);
        QCOMPARE(s.avgPowerW, 205);
        QCOMPARE(s.normalizedPower, 215);
        QCOMPARE(s.avgHrBpm, 150);
        QCOMPARE(s.avgCadence, 92);
        QCOMPARE(s.calories, 450);
        QCOMPARE(qRound(s.tss), 85);
        QVERIFY(qAbs(s.totalDistanceKm - 0.6) < 1e-6);  // 600 m
        QCOMPARE(s.startTime.toUTC(),
                 QDateTime(QDate(2024, 1, 15), QTime(10, 0, 0), Qt::UTC));
    }

    void summaryHandlesMissingFile()
    {
        const WorkoutHistorySummary s = FitActivityReader::readFile("/no/such/file.fit");
        QVERIFY(!s.valid);
    }

    // ── FIT indexing: detail records + laps ──────────────────────────────────
    void detailReadsRecordsAndLaps()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("activity.fit");
        writeSampleFit(path, 60);

        const WorkoutHistoryDetail d = FitActivityDetailReader::readFile(path);

        QVERIFY(d.valid);
        QCOMPARE(d.records.size(), 60);
        QVERIFY(d.hasPower());
        QVERIFY(d.hasHeartRate());
        QVERIFY(d.hasCadence());

        // Records are zero-based on elapsed time, with the values we wrote.
        QCOMPARE(d.records.first().elapsedSec, 0);
        QCOMPARE(d.records.first().power, 200);
        QCOMPARE(d.records.last().elapsedSec, 59);
        QCOMPARE(d.records.last().power, 259);

        QCOMPARE(d.laps.size(), 1);
        QCOMPARE(d.laps.first().avgPowerW, 205);
        QCOMPARE(d.laps.first().maxPowerW, 260);
        QCOMPARE(d.laps.first().normalizedPower, 215);
    }

    // ── TSS aggregation: PmcCalculator ───────────────────────────────────────
    void pmcAggregatesSingleActivity()
    {
        const QDate day(2024, 3, 10);
        WorkoutHistorySummary s;
        s.startTime = QDateTime(day, QTime(8, 0), Qt::LocalTime);
        s.tss = 100.0;
        s.valid = true;

        const QList<PmcPoint> pts = PmcCalculator::compute({s}, day);
        QVERIFY(!pts.isEmpty());

        const PmcPoint &last = pts.last();
        QCOMPARE(last.date, day);
        QCOMPARE(qRound(last.tss), 100);
        // CTL/ATL on the activity day, all prior days TSS=0:
        //   CTL = 100 * (1 - e^-1/42) ≈ 2.36 ; ATL = 100 * (1 - e^-1/7) ≈ 13.34
        QVERIFY(qAbs(last.ctl - 100.0 * (1.0 - std::exp(-1.0 / 42.0))) < 0.01);
        QVERIFY(qAbs(last.atl - 100.0 * (1.0 - std::exp(-1.0 / 7.0)))  < 0.01);
    }

    void pmcSumsSameDayActivities()
    {
        const QDate day(2024, 3, 10);
        WorkoutHistorySummary a;
        a.startTime = QDateTime(day, QTime(8, 0), Qt::LocalTime);
        a.tss = 60.0;
        a.valid = true;
        WorkoutHistorySummary b;
        b.startTime = QDateTime(day, QTime(18, 0), Qt::LocalTime);
        b.tss = 40.0;
        b.valid = true;

        const QList<PmcPoint> pts = PmcCalculator::compute({a, b}, day);
        QVERIFY(!pts.isEmpty());
        QCOMPARE(qRound(pts.last().tss), 100);  // 60 + 40 aggregated on the same day
    }

    // Activities whose FIT file had no session data have an invalid startTime.
    // These must be skipped, not crash/hang the day-by-day EMA loop.
    void pmcSkipsInvalidDates()
    {
        const QDate day(2024, 3, 10);
        WorkoutHistorySummary good;
        good.startTime = QDateTime(day, QTime(8, 0), Qt::LocalTime);
        good.tss = 100.0;
        good.valid = true;
        WorkoutHistorySummary junk;          // invalid startTime, no data
        junk.workoutName = "broken";

        const QList<PmcPoint> pts = PmcCalculator::compute({junk, good, junk}, day);
        QVERIFY(!pts.isEmpty());
        QCOMPARE(pts.last().date, day);
        QCOMPARE(qRound(pts.last().tss), 100);
    }

    void pmcAllInvalidReturnsEmpty()
    {
        WorkoutHistorySummary junk;          // invalid startTime
        junk.workoutName = "broken";
        QVERIFY(PmcCalculator::compute({junk}).isEmpty());
    }
};

QTEST_MAIN(TestHistory)
#include "tst_history.moc"
