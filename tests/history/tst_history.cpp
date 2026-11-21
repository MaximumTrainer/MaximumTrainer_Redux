#include <QtTest>
#include <QTimeZone>
#include <QTemporaryDir>
#include <QDateTime>
#include <QtEndian>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

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
#include "powerzones.h"
#include "ridesummary.h"
#include "workouthistorymodel.h"
#include "historyfilterproxymodel.h"
#include "ftpestimator.h"
#include "weeklysummary.h"
#include "mmpcalculator.h"
#include "personalbesttracker.h"
#include "powerbests.h"

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
    void rideComparison_findsLastFullRideOfSameWorkout()
    {
        QCOMPARE(FitActivityReader::workoutNameFromFileName(
                     QStringLiteral("/h/2026-07-09(09-24-28)-MT-icu_i601941-Crunch 3-2 Micro 40-20s.fit")),
                 QStringLiteral("Crunch 3-2 Micro 40-20s"));

        QTemporaryDir dir;
        auto path = [&dir](const char *name) { return dir.filePath(QString::fromLatin1(name)); };
        writeSampleFit(path("2026-11-01(10-00-00)-MT-user-Threshold.fit"), 600);   // the match
        writeSampleFit(path("2026-11-05(10-00-00)-MT-user-Threshold.fit"), 60);    // abandoned: too short
        writeSampleFit(path("2026-11-06(10-00-00)-MT-user-Other.fit"), 600);       // another workout
        writeSampleFit(path("2026-11-10(10-00-00)-MT-user-Threshold.fit"), 600);   // this ride
        writeSampleFit(path("2026-11-12(10-00-00)-MT-user-Threshold.fit"), 600);   // later: ignored
        const QString current = path("2026-11-10(10-00-00)-MT-user-Threshold.fit");
        QCOMPARE(RideComparison::previousRideFile(dir.path(), current, 600),
                 path("2026-11-01(10-00-00)-MT-user-Threshold.fit"));
        QVERIFY(RideComparison::previousRideFile(dir.path(), path("2026-11-01(10-00-00)-MT-user-Threshold.fit"), 600)
                    .isEmpty());

        WorkoutHistorySummary before;
        before.valid = true;
        before.normalizedPower = 200;
        before.avgPowerW = 190;
        before.avgHrBpm = 150;
        before.tss = 50;
        WorkoutHistorySummary now = before;
        now.normalizedPower = 206;
        now.avgHrBpm = 147;
        now.tss = 53.5;
        const RideComparison diff = RideComparison::compare(now, before);
        QVERIFY(diff.valid && diff.hasPower && diff.hasHeartRate);
        QCOMPARE(diff.npDeltaW, 6);
        QCOMPARE(diff.hrDeltaBpm, -3);
        QCOMPARE(diff.tssDelta, 3.5);
        before.avgHrBpm = 0;
        QVERIFY(!RideComparison::compare(now, before).hasHeartRate);

        // Several rides of the workout, newest first, abandoned and other workouts skipped.
        writeSampleFit(path("2026-10-20(10-00-00)-MT-user-Threshold.fit"), 600);
        QCOMPARE(RideComparison::previousRideFiles(dir.path(), current, 600, 5),
                 (QStringList{path("2026-11-01(10-00-00)-MT-user-Threshold.fit"),
                              path("2026-10-20(10-00-00)-MT-user-Threshold.fit")}));
        QCOMPARE(RideComparison::previousRideFiles(dir.path(), current, 600, 1).size(), 1);
    }

    void rideComparison_efficiencyAndFtpFromTss()
    {
        // 1 h at NP 250 scored with FTP 250 is TSS 100; FTP 260 makes it 92.5.
        WorkoutHistorySummary ride;
        ride.valid = true;
        ride.durationSec = 3600;
        ride.normalizedPower = 250;
        ride.avgPowerW = 240;
        ride.avgHrBpm = 125;
        ride.tss = 100.0;
        QCOMPARE(RideComparison::impliedFtp(ride), 250);
        QCOMPARE(RideComparison::efficiency(ride), 2.0);
        WorkoutHistorySummary later = ride;
        later.tss = 100.0 * 250 * 250 / (260.0 * 260.0);
        later.avgHrBpm = 120;
        QCOMPARE(RideComparison::impliedFtp(later), 260);

        const RideComparison diff = RideComparison::compare(later, ride);
        QVERIFY(diff.hasEfficiency);
        QCOMPARE(qRound(diff.efficiencyDelta * 100), 8);        // 250/120 - 2.0
        QCOMPARE(diff.previousFtpW, 250);
        QCOMPARE(diff.currentFtpW, 260);

        ride.tss = 0.0;
        QCOMPARE(RideComparison::impliedFtp(ride), 0);
    }

    void rideTrend_ranksHeartRateAndEfficiency()
    {
        auto ride = [](int day, int hr, int np) {
            WorkoutHistorySummary summary;
            summary.valid = true;
            summary.startTime = QDateTime(QDate(2026, 6, day), QTime(10, 0));
            summary.avgHrBpm = hr;
            summary.normalizedPower = np;
            summary.avgPowerW = np - 4;
            return summary;
        };
        const WorkoutHistorySummary today = ride(30, 145, 202);
        const QList<WorkoutHistorySummary> previous = {ride(20, 149, 207), ride(10, 150, 199),
                                                       ride(5, 0, 210), ride(1, 152, 188)};   // newest first
        const RideTrend trend = RideTrend::build(today, previous);
        QVERIFY(trend.valid);
        QCOMPARE(trend.rides.size(), 4);                   // the ride without heart rate is left out
        QCOMPARE(trend.rides.first().date, QDate(2026, 6, 1));   // oldest first
        QVERIFY(trend.rides.last().isCurrent);
        QVERIFY(trend.currentIsBest());
        QCOMPARE(trend.currentHrRank, 1);
        QCOMPARE(trend.currentEfficiencyRank, 1);          // 202/145 beats 207/149
        QCOMPARE(trend.currentPowerW, 198);

        const RideTrend worse = RideTrend::build(ride(30, 151, 202), previous);
        QCOMPARE(worse.rides.at(worse.bestIndex).avgHrBpm, 149);
        QCOMPARE(worse.currentHrRank, 3);
        QVERIFY(!RideTrend::build(today, {}).valid);       // nothing to compare with
        QVERIFY(!RideTrend::build(ride(30, 0, 202), previous).valid);
    }

    void powerZones_rampBrushSwitchesAtZoneBounds()
    {
        const QVector<int> coggan = PowerZones::boundsPct({});
        const QBrush steady = PowerZones::brush(60, 70, coggan, 200);
        QCOMPARE(steady.style(), Qt::SolidPattern);
        QCOMPARE(steady.color().rgb(), PowerZones::color(1).rgb());

        // 50 -> 80 %: Z1 until 55 % (1/6 of the way), Z2 until 75 %, then Z3.
        const QBrush ramp = PowerZones::brush(50, 80, coggan, 200);
        QCOMPARE(ramp.style(), Qt::LinearGradientPattern);
        const QGradientStops stops = ramp.gradient()->stops();
        QCOMPARE(stops.first().second.rgb(), PowerZones::color(0).rgb());
        QCOMPARE(stops.last().second.rgb(), PowerZones::color(2).rgb());
        QVERIFY(stops.at(1).second.rgb() != PowerZones::color(0).rgb());   // faded toward Z2
        QVERIFY(qAbs(stops.at(1).first - 1.0 / 6) < 1e-9);
        QCOMPARE(stops.at(2).second.rgb(), PowerZones::color(1).rgb());    // true Z2 from the bound
        QCOMPARE(stops.size(), 6);
        QCOMPARE(stops.at(4).second.rgb(), PowerZones::color(2).rgb());     // true Z3 from 75 %

        // A cool-down runs the other way: starts Z3, ends Z1.
        const QGradientStops down = PowerZones::brush(80, 50, coggan, 200).gradient()->stops();
        QCOMPARE(down.first().second.rgb(), PowerZones::color(2).rgb());
        QCOMPARE(down.last().second.rgb(), PowerZones::color(0).rgb());
    }

    void rideSummary_zonesAndIntensity()
    {
        const QVector<int> coggan = PowerZones::boundsPct({});
        QCOMPARE(coggan, (QVector<int>{55, 75, 90, 105, 120, 150}));
        QCOMPARE(PowerZones::zoneForPct(55, coggan), 0);    // 55 % is still Z1
        QCOMPARE(PowerZones::zoneForPct(55.5, coggan), 1);
        QCOMPARE(PowerZones::zoneForPct(100, coggan), 3);   // threshold is Z4
        QCOMPARE(PowerZones::zoneForPct(151, coggan), 6);   // above 150 % is Z7
        // Intervals.icu zones end with an open 999 % zone, which is dropped.
        QCOMPARE(PowerZones::boundsPct({60, 80, 999}), (QVector<int>{60, 80}));
        QCOMPARE(PowerZones::label(2, 7, {}), QStringLiteral("Z3 Tempo"));
        QCOMPARE(PowerZones::label(1, 3, {QStringLiteral("Easy"), QStringLiteral("Steady"), QStringLiteral("Hard")}),
                 QStringLiteral("Z2 Steady"));
        QCOMPARE(PowerZones::label(1, 3, {}), QStringLiteral("Z2"));

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
        const RideSummary custom = RideSummary::build(summary, detail, 200, PowerZones::boundsPct({65, 105, 999}));
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

    void weeklyGoal_progressAndStreak()
    {
        const QDate today(2026, 11, 18);  // a Wednesday; its week starts Monday 16th
        const auto ride = [](const QDate &day, int minutes) {
            WorkoutHistorySummary summary;
            summary.startTime = QDateTime(day, QTime(7, 0));
            summary.durationSec = minutes * 60;
            summary.valid = true;
            return summary;
        };
        QList<WorkoutHistorySummary> history{
            ride(QDate(2026, 10, 20), 300),   // 4 weeks back: reached, but week 3 back breaks the run
            ride(QDate(2026, 11, 3), 120), ride(QDate(2026, 11, 5), 180),  // 2 weeks back: 5 h
            ride(QDate(2026, 11, 9), 299),    // last week: 4:59, within a minute of 5 h
            ride(QDate(2026, 11, 16), 90),    // this week so far
        };

        QCOMPARE(WeeklySummary::goalProgress(history, today, 0.0).goalHours, 0.0);

        WeeklySummary::GoalProgress progress = WeeklySummary::goalProgress(history, today, 5.0);
        QCOMPARE(progress.goalHours, 5.0);
        QCOMPARE(progress.hoursThisWeek, 1.5);
        QVERIFY(!progress.reached);
        QCOMPARE(progress.streakWeeks, 2);   // this week is still in progress

        history.append(ride(QDate(2026, 11, 17), 210));
        progress = WeeklySummary::goalProgress(history, today, 5.0);
        QVERIFY(progress.reached);
        QCOMPARE(progress.streakWeeks, 3);

        QCOMPARE(WeeklySummary::goalProgress(history, today, 6.0).streakWeeks, 0);
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

    // ── Power bests: per-ride curve, periods, cache ──────────────────────────
    void powerBests_slidingWindowAverages()
    {
        const QVector<int> power{100, 300, 500, 300, 100};
        const QVector<double> best = PowerBests::bestAverages(power, {1, 2, 3, 6});
        QCOMPARE(best[0], 500.0);
        QCOMPARE(best[1], 400.0);
        QCOMPARE(best[2], (300.0 + 500.0 + 300.0) / 3.0);
        QCOMPARE(best[3], 0.0);  // longer than the ride
    }

    void powerBests_periodKeepsTheRideOfEachBest()
    {
        const QDateTime now = QDateTime::currentDateTime();
        const auto ride = [](const QDateTime &date, const QString &name, double fiveSec) {
            PowerBests::Ride r;
            r.summary.startTime = date;
            r.summary.workoutName = name;
            r.bests = QVector<double>(MMP_DURATIONS.size(), 0.0);
            r.bests[MMP_DURATIONS.indexOf(5)] = fiveSec;
            return r;
        };
        const QList<PowerBests::Ride> rides{ride(now.addDays(-200), QStringLiteral("old"), 900),
                                            ride(now.addDays(-10), QStringLiteral("recent"), 700),
                                            ride(QDateTime(), QStringLiteral("undated"), 1000)};
        const qsizetype index = MMP_DURATIONS.indexOf(5);

        const QVector<PowerBests::Effort> allTime = PowerBests::bestEfforts(rides);
        QCOMPARE(allTime[index].watts, 1000.0);
        QCOMPARE(allTime[index].workoutName, QStringLiteral("undated"));

        const QVector<PowerBests::Effort> recent = PowerBests::bestEfforts(rides, now.addDays(-90));
        QCOMPARE(recent[index].watts, 700.0);
        QCOMPARE(recent[index].workoutName, QStringLiteral("recent"));
    }

    void powerBests_cacheSkipsDecodingAndNoticesChanges()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        PowerBests::setCacheFilePathForTesting(dir.filePath(QStringLiteral("cache/bests.json")));
        const QString path = dir.filePath(QStringLiteral("ride.fit"));
        writeSampleFit(path, 60);  // power 200..259 W
        WorkoutHistorySummary summary;
        summary.filePath = path;
        const qsizetype fiveSec = MMP_DURATIONS.indexOf(5);

        QList<PowerBests::Ride> rides = PowerBests::loadRides({summary});
        QCOMPARE(rides.size(), 1);
        QCOMPARE(rides.first().bests[fiveSec], 257.0);  // 255..259
        QVERIFY(QFile::exists(PowerBests::cacheFilePath()));

        // Tamper with the cache: a second load must read it instead of the file.
        QFile cache(PowerBests::cacheFilePath());
        QVERIFY(cache.open(QIODevice::ReadOnly));
        QJsonObject root = QJsonDocument::fromJson(cache.readAll()).object();
        cache.close();
        QJsonObject cachedRides = root.value(QStringLiteral("rides")).toObject();
        QCOMPARE(cachedRides.size(), 1);
        QJsonArray values = cachedRides.begin().value().toArray();
        values[fiveSec] = 999;
        cachedRides.insert(cachedRides.begin().key(), values);
        root.insert(QStringLiteral("rides"), cachedRides);
        QVERIFY(cache.open(QIODevice::WriteOnly | QIODevice::Truncate));
        cache.write(QJsonDocument(root).toJson());
        cache.close();
        QCOMPARE(PowerBests::loadRides({summary}).first().bests[fiveSec], 999.0);

        // A rewritten ride (new size) is decoded again.
        writeSampleFit(path, 90);
        QCOMPARE(PowerBests::loadRides({summary}).first().bests[fiveSec], 287.0);

        // Deleted rides drop out of the cache.
        PowerBests::loadRides({});
        QVERIFY(cache.open(QIODevice::ReadOnly));
        QVERIFY(!cache.readAll().contains("ride.fit"));
        PowerBests::setCacheFilePathForTesting(QString());
    }

    // ── Live personal bests ──────────────────────────────────────────────────
    static QVector<PersonalBestTracker::Event> feed(PersonalBestTracker &tracker, const QVector<double> &watts)
    {
        QVector<PersonalBestTracker::Event> events;
        for (double w : watts)
            events += tracker.addSecond(w);
        return events;
    }

    void personalBest_reportsThePeakOnceTheEffortEnds()
    {
        PersonalBestTracker tracker;
        tracker.start({{5, 500, 800}});
        // 5 s at 600 W, rising to 650 W, then easy riding.
        QVERIFY(feed(tracker, {600, 600, 600, 600, 600}).isEmpty());
        QVERIFY(feed(tracker, {650, 650}).isEmpty());
        const QVector<PersonalBestTracker::Event> events = feed(tracker, {100, 100, 100});
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().seconds, 5);
        QCOMPARE(events.first().watts, 620.0);  // 600,600,600,650,650
        QCOMPARE(events.first().previousWatts, 500.0);
        QVERIFY(!events.first().allTime);
        // The same level does not fire again in this ride.
        QVERIFY(feed(tracker, {700, 700, 700, 700, 700, 0, 0, 0}).isEmpty());
    }

    void personalBest_allTimeUpgradesAfterARecentBest()
    {
        PersonalBestTracker tracker;
        tracker.start({{5, 500, 800}});
        QCOMPARE(feed(tracker, {600, 600, 600, 600, 600, 0, 0, 0}).size(), 1);
        const QVector<PersonalBestTracker::Event> events = feed(tracker, {900, 900, 900, 900, 900, 0, 0, 0});
        QCOMPARE(events.size(), 1);
        QVERIFY(events.first().allTime);
        QCOMPARE(events.first().watts, 900.0);
        QCOMPARE(events.first().previousWatts, 800.0);
    }

    void personalBest_quietWithoutEarlierRidesOrTies()
    {
        PersonalBestTracker tracker;
        tracker.start({{5, 0, 0}, {60, 300, 300}});
        // 300.4 W rounds to the 300 W record, and 5 s has nothing to beat.
        QVERIFY(feed(tracker, QVector<double>(120, 300.4)).isEmpty());
        QVERIFY(tracker.finish().isEmpty());
    }

    void personalBest_finishReportsAnEffortInProgress()
    {
        PersonalBestTracker tracker;
        tracker.start({{5, 400, 0}});
        QVERIFY(feed(tracker, {-1, 500, 500, 500, 500, 500}).isEmpty());
        const QVector<PersonalBestTracker::Event> events = tracker.finish();
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.first().watts, 500.0);
        QVERIFY(!events.first().allTime);
        QVERIFY(PersonalBestTracker().addSecond(1000).isEmpty());  // not started
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
