#include <QtTest>
#include <QTimeZone>
#include <QTemporaryDir>
#include <QDateTime>
#include <QtEndian>

#include <fstream>
#include <cmath>

#include "fit_encode.hpp"
#include "fit_file_id_mesg.hpp"
#include "fit_session_mesg.hpp"
#include "fit_lap_mesg.hpp"
#include "fit_record_mesg.hpp"
#include "fit_decode.hpp"
#include "fit_mesg_broadcaster.hpp"
#include "fit_record_mesg_listener.hpp"
#include "fit_session_mesg_listener.hpp"

#include "fitactivityreader.h"
#include "fitactivitydetailreader.h"
#include "pmccalculator.h"
#include "ridesummary.h"
#include "workouthistorymodel.h"
#include "historyfilterproxymodel.h"
#include "ftpestimator.h"
#include "weeklysummary.h"

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
        const QDateTime start = QDateTime(QDate(2024, 1, 15), QTime(10, 0, 0), QTimeZone::UTC);
        const qint64 fitStart = start.toSecsSinceEpoch() - FIT_EPOCH_OFFSET;

        std::fstream file;
        file.open(path.toStdString(), std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);

        fit::Encode encode(fit::ProtocolVersion::V10);
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

    // Declared size of every 32-bit-typed field in the file's definition
    // messages, read straight from the bytes (independent of the SDK's types).
    static QList<int> declaredUint32FieldSizes(const QByteArray &fit)
    {
        QList<int> sizes;
        const int headerSize = quint8(fit[0]);
        const int dataEnd = headerSize + int(qFromLittleEndian<quint32>(fit.constData() + 4));
        QHash<int, int> dataSizeByLocal;
        int i = headerSize;
        while (i < dataEnd) {
            const quint8 recordHeader = quint8(fit[i++]);
            if (recordHeader & 0x80) {                     // compressed-timestamp data message
                i += dataSizeByLocal.value((recordHeader >> 5) & 0x3);
                continue;
            }
            const int local = recordHeader & 0x0F;
            if (!(recordHeader & 0x40)) {                  // data message
                i += dataSizeByLocal.value(local);
                continue;
            }
            const int numFields = quint8(fit[i + 4]);
            i += 5;
            int total = 0;
            for (int f = 0; f < numFields; ++f, i += 3) {
                const int size = quint8(fit[i + 1]);
                const quint8 baseType = quint8(fit[i + 2]);
                if (baseType == FIT_BASE_TYPE_UINT32 || baseType == FIT_BASE_TYPE_SINT32 || baseType == FIT_BASE_TYPE_UINT32Z)
                    sizes << size;
                total += size;
            }
            if (recordHeader & 0x20) {                     // developer fields
                const int numDevFields = quint8(fit[i++]);
                for (int f = 0; f < numDevFields; ++f, i += 3)
                    total += quint8(fit[i + 1]);
            }
            dataSizeByLocal[local] = total;
        }
        return sizes;
    }

    struct FirstRecordAndSession : fit::RecordMesgListener, fit::SessionMesgListener {
        std::unique_ptr<fit::RecordMesg>  record;
        std::unique_ptr<fit::SessionMesg> session;
        void OnMesg(fit::RecordMesg &mesg) override  { if (!record) record = std::make_unique<fit::RecordMesg>(mesg); }
        void OnMesg(fit::SessionMesg &mesg) override { session = std::make_unique<fit::SessionMesg>(mesg); }
    };

private slots:
    void rideSummary_zonesAndIntensity()
    {
        const QVector<int> coggan = RideSummary::zoneBounds(200, {});
        QCOMPARE(coggan, (QVector<int>{110, 150, 180, 210, 240, 300}));
        QCOMPARE(RideSummary::powerZone(110, coggan), 0);   // 55 % is still Z1
        QCOMPARE(RideSummary::powerZone(111, coggan), 1);
        QCOMPARE(RideSummary::powerZone(200, coggan), 3);   // 100 % is Z4
        QCOMPARE(RideSummary::powerZone(301, coggan), 6);   // above 150 % is Z7
        // Intervals.icu bounds end with an open 999 % zone, which is dropped.
        QCOMPARE(RideSummary::zoneBounds(200, {120, 160, 1998}), (QVector<int>{120, 160}));

        WorkoutHistorySummary summary;
        summary.valid = true;
        summary.durationSec = 600;
        summary.normalizedPower = 180;
        summary.tss = 12.5;
        WorkoutHistoryDetail detail;
        // 4 s in Z2, a 60 s pause gap (capped to 5 s) in Z4, 1 s at the end in Z6, no-power sample ignored.
        const int seconds[] = {0, 1, 2, 3, 4, 64, 65};
        const int watts[]   = {120, 120, 120, 120, 200, 260, -1};
        for (int i = 0; i < 7; ++i) {
            ActivityRecordPoint point;
            point.elapsedSec = seconds[i];
            point.power = watts[i];
            point.heartRate = 140 + i;
            detail.records.append(point);
        }
        const RideSummary ride = RideSummary::build(summary, detail, 200);
        QVERIFY(ride.valid);
        QVERIFY(ride.hasPower);
        QCOMPARE(ride.zoneSeconds.size(), 7);
        QCOMPARE(ride.zoneSeconds.at(1), 4);
        QCOMPARE(ride.zoneSeconds.at(3), 5);
        QCOMPARE(ride.zoneSeconds.at(5), 1);
        QCOMPARE(ride.zoneTotalSeconds(), 10);
        // The athlete's own zones: 3 zones from 2 bounds.
        const RideSummary custom = RideSummary::build(summary, detail, 200, {130, 210, 1998});
        QCOMPARE(custom.zoneSeconds, (QVector<int>{4, 5, 1}));
        QCOMPARE(ride.maxPowerW, 260);
        QCOMPARE(ride.maxHrBpm, 146);
        QCOMPARE(ride.intensityFactor, 0.9);
        QCOMPARE(ride.tss, 12.5);
    }

    void weeklySummary_bucketsByMondayWeek()
    {
        QCOMPARE(WeeklySummary::mondayOf(QDate(2026, 11, 19)), QDate(2026, 11, 16));   // Thu
        QCOMPARE(WeeklySummary::mondayOf(QDate(2026, 11, 16)), QDate(2026, 11, 16));   // Mon
        QCOMPARE(WeeklySummary::mondayOf(QDate(2026, 11, 22)), QDate(2026, 11, 16));   // Sun

        auto ride = [](const QDate &day, int minutes, double tss, double km) {
            WorkoutHistorySummary s;
            s.startTime = QDateTime(day, QTime(18, 0), QTimeZone::LocalTime);
            s.durationSec = minutes * 60;
            s.tss = tss;
            s.totalDistanceKm = km;
            s.valid = true;
            return s;
        };
        const QDate today(2026, 11, 19);
        const QVector<WeeklySummary::Week> weeks = WeeklySummary::recentWeeks(
            {ride(QDate(2026, 11, 17), 60, 70, 30), ride(QDate(2026, 11, 19), 30, 40, 15),
             ride(QDate(2026, 11, 12), 90, 100, 45),
             ride(QDate(2026, 8, 1), 60, 80, 30),          // older than 12 weeks
             WorkoutHistorySummary()},                       // no start time
            today, 12);
        QCOMPARE(weeks.size(), 12);
        QCOMPARE(weeks.last().monday, QDate(2026, 11, 16));
        QCOMPARE(weeks.first().monday, QDate(2026, 8, 31));
        QCOMPARE(weeks.last().rides, 2);
        QCOMPARE(weeks.last().hours, 1.5);
        QCOMPARE(weeks.last().tss, 110.0);
        QCOMPARE(weeks.last().km, 45.0);
        QCOMPARE(weeks.at(10).rides, 1);
        QCOMPARE(weeks.at(10).tss, 100.0);
        int total = 0;
        for (const auto &week : weeks)
            total += week.rides;
        QCOMPARE(total, 3);
    }

    void ftpEstimate_rulesOfThumb()
    {
        QCOMPARE(FtpEstimator::ftpFromBest20(250.0), 238);
        // At least 5 W and 2% over the current FTP.
        QVERIFY(!FtpEstimator::worthSuggesting(200, 204, 0));
        QVERIFY(FtpEstimator::worthSuggesting(200, 205, 0));
        QVERIFY(!FtpEstimator::worthSuggesting(300, 305, 0));
        QVERIFY(FtpEstimator::worthSuggesting(300, 306, 0));
        // A dismissed estimate is only suggested again when it grows.
        QVERIFY(!FtpEstimator::worthSuggesting(200, 214, 214));
        QVERIFY(FtpEstimator::worthSuggesting(200, 220, 214));
        // Never a decrease.
        QVERIFY(!FtpEstimator::worthSuggesting(250, 238, 0));
    }

    void ftpEstimate_fromRecentRides()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString longRide  = dir.filePath(QStringLiteral("long.fit"));
        const QString shortRide = dir.filePath(QStringLiteral("short.fit"));
        const QString oldRide   = dir.filePath(QStringLiteral("old.fit"));
        writeSampleFit(longRide, 1300);   // power 200 + i W each second
        writeSampleFit(shortRide, 600);   // under 20 minutes
        writeSampleFit(oldRide, 1300);
        const QDateTime now = QDateTime::currentDateTime();
        QFile old(oldRide);
        QVERIFY(old.open(QIODevice::ReadWrite));
        QVERIFY(old.setFileTime(now.addDays(-(FtpEstimator::kWindowDays + 1)), QFileDevice::FileModificationTime));
        old.close();

        const QList<QString> recent = FtpEstimator::recentRides(dir.path(), now);
        QCOMPARE(recent.size(), 2);
        QVERIFY(!recent.contains(QFileInfo(oldRide).absoluteFilePath()));

        // Best 1200 s window of 200..1499 W is seconds 100..1299: mean 899.5 W.
        const FtpEstimator::Estimate estimate = FtpEstimator::fromRides(recent);
        QVERIFY(estimate.valid);
        QCOMPARE(estimate.best20W, 900);
        QCOMPARE(estimate.ftpW, FtpEstimator::ftpFromBest20(899.5));

        QVERIFY(!FtpEstimator::fromRides({QFileInfo(shortRide).absoluteFilePath()}).valid);
        QVERIFY(!FtpEstimator::fromRides({}).valid);
    }

    void dateFilterKeepsUndatedActivities()
    {
        WorkoutHistorySummary dated;
        dated.workoutName = QStringLiteral("Dated");
        dated.startTime = QDateTime(QDate(2024, 1, 15), QTime(10, 0), QTimeZone::UTC);
        WorkoutHistorySummary undated;
        undated.workoutName = QStringLiteral("Undated");

        WorkoutHistoryModel model;
        model.setHistory({dated, undated});
        HistoryFilterProxyModel proxy;
        proxy.setSourceModel(&model);

        proxy.setDateRange(QDate(2025, 1, 1), QDate(2025, 12, 31));
        QCOMPARE(proxy.rowCount(), 1);
        QCOMPARE(proxy.index(0, WorkoutHistoryModel::ColWorkout).data().toString(), QStringLiteral("Undated"));

        proxy.setDateRange(QDate(2024, 1, 1), QDate(2024, 12, 31));
        QCOMPARE(proxy.rowCount(), 2);
    }


    // ── FIT encoding: 32-bit fields are declared 4 bytes wide ─────────────────
    // With 8-byte FIT_UINT32 (no FIT_USE_STDINT_H on LP64) the encoder declared
    // every uint32 field as 8 bytes: decoders saw a 2-element array and Garmin's
    // own decoder rejected the file. Each such field must decode to one value.
    void encodedUint32FieldsHoldOneValue()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("activity.fit");
        writeSampleFit(path, 10);

        std::fstream file(path.toStdString(), std::ios::in | std::ios::binary);
        QVERIFY(file.is_open());
        char header[14];
        file.read(header, sizeof(header));
        QCOMPARE(int(header[0]), 14);
        QCOMPARE(QByteArray(header + 8, 4), QByteArray(".FIT"));
        file.seekg(0);

        QFile raw(path);
        QVERIFY(raw.open(QIODevice::ReadOnly));
        const QList<int> uint32Sizes = declaredUint32FieldSizes(raw.readAll());
        QVERIFY(!uint32Sizes.isEmpty());
        for (int size : uint32Sizes)
            QCOMPARE(size, 4);

        fit::Decode decode;
        fit::MesgBroadcaster broadcaster;
        FirstRecordAndSession capture;
        broadcaster.AddListener(static_cast<fit::RecordMesgListener &>(capture));
        broadcaster.AddListener(static_cast<fit::SessionMesgListener &>(capture));
        QVERIFY(decode.Read(&file, &broadcaster, &broadcaster, nullptr));
        QVERIFY(capture.record);
        QVERIFY(capture.session);

        for (const auto &[mesg, fieldName] : { std::pair<fit::Mesg *, const char *>{capture.record.get(),  "timestamp"},
                                               {capture.record.get(),  "distance"},
                                               {capture.session.get(), "start_time"},
                                               {capture.session.get(), "total_timer_time"} }) {
            const fit::Field *field = mesg->GetField(std::string(fieldName));
            QVERIFY2(field, fieldName);
            QVERIFY2(field->GetNumValues() == 1, fieldName);
        }
    }

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
                 QDateTime(QDate(2024, 1, 15), QTime(10, 0, 0), QTimeZone::UTC));
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
        s.startTime = QDateTime(day, QTime(8, 0), QTimeZone::LocalTime);
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
        a.startTime = QDateTime(day, QTime(8, 0), QTimeZone::LocalTime);
        a.tss = 60.0;
        a.valid = true;
        WorkoutHistorySummary b;
        b.startTime = QDateTime(day, QTime(18, 0), QTimeZone::LocalTime);
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
        good.startTime = QDateTime(day, QTime(8, 0), QTimeZone::LocalTime);
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
