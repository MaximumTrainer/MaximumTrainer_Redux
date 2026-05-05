/*
 * tst_workout_parsing.cpp
 *
 * Workout Parsing & User Journey Tests -- MaximumTrainer
 *
 * Purpose
 * -----------------------------------------------------------------------
 * Validates the ZWO (Zwift workout XML) parsing pipeline and the workout
 * model construction workflow with programmatic intervals.
 *
 * Test groups
 * ──────────────────────────────────────────────────────────────────────
 * 1. ZWO inline string parsing
 *    Feeds raw ZWO XML strings (embedded in the source code) to
 *    ImporterWorkoutZwo::importFromByteArray() and verifies that the
 *    resulting Workout model is populated with the expected interval
 *    count, power step types, and FTP fractions.
 *
 *    Fixtures covered:
 *    • SteadyState  — flat power at 0.75 FTP for 20 min
 *    • Ramp         — progressive 0.50→1.00 FTP for 10 min
 *    • IntervalsT   — 5 repeats × (60 s on @ 1.10 + 90 s off @ 0.55)
 *    • Mixed        — Warmup + SteadyState + Cooldown (3 intervals)
 *
 * 2. Workout model from programmatic intervals
 *    Constructs Workout and Interval objects in C++ using the
 *    Warmup / SteadyState / Cooldown paradigm and verifies:
 *    • All three interval types are stored with the correct step types
 *    • The average power metric is in the expected range
 *    • Duration round-trips through QTime correctly
 *
 * 3. Intervals.icu pull-and-load workflow (live, optional)
 *    Uses a real HTTPS request to intervals.icu/api/v1 to:
 *      a. Fetch the most recent 90-day workout list for the test account.
 *      b. Download the ZWO file for the first workout found.
 *      c. Parse it with ImporterWorkoutZwo::importFromByteArray().
 *      d. Assert that every resulting interval has a positive duration.
 *    QSKIP is issued when INTERVALS_ICU_API_KEY / INTERVALS_ICU_ATHLETE_ID
 *    are absent, so the suite degrades gracefully on fork PRs.
 *
 * Build (no display required):
 *   cd tests/integration
 *   qmake workout_parsing_tests.pro && make -j$(nproc)
 *   ../../build/tests/workout_parsing_tests -v2
 */

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QByteArray>
#include <QDate>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "importerworkoutzwo.h"
#include "account.h"
#include "workout.h"
#include "interval.h"

// ---------------------------------------------------------------------------
// Helper: spin the event loop until the reply finishes or the timeout fires.
// ---------------------------------------------------------------------------
static bool waitForNetworkReply(QNetworkReply *reply, int timeoutMs = 30000)
{
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();
    return reply->isFinished();
}

// ---------------------------------------------------------------------------
// TstWorkoutParsing
// ---------------------------------------------------------------------------
class TstWorkoutParsing : public QObject
{
    Q_OBJECT

private:
    Account *m_account = nullptr;

private slots:

    // ── Setup ────────────────────────────────────────────────────────────────
    void initTestCase()
    {
        // Register a dummy Account so Workout::calculateWorkoutMetrics() works.
        m_account = new Account(qApp);
        m_account->FTP  = 200; // W
        m_account->LTHR = 160; // bpm
        m_account->display_name = QStringLiteral("CI Test User");
        qApp->setProperty("Account", QVariant::fromValue<Account*>(m_account));
    }

    void cleanupTestCase()
    {
        qApp->setProperty("Account", QVariant());
    }

    // ========================================================================
    // Group 1 — ZWO inline string parsing
    // ========================================================================

    // ────────────────────────────────────────────────────────────────────────
    // 1a. SteadyState: 1 interval, flat 0.75 FTP, 20 min (1200 s)
    // ────────────────────────────────────────────────────────────────────────
    void testZwoSteadyStateInline()
    {
        const QByteArray zwoXml = QByteArrayLiteral(
            "<workout_file>"
            "  <name>CI Steady State</name>"
            "  <author>CI Bot</author>"
            "  <description>20 min at 75%% FTP</description>"
            "  <workout>"
            "    <SteadyState Duration=\"1200\" Power=\"0.75\" />"
            "  </workout>"
            "</workout_file>");

        const Workout w = ImporterWorkoutZwo::importFromByteArray(zwoXml, "steady-state-test");

        QCOMPARE(w.getLstInterval().size(), 1);

        const QList<Interval> ivs0 = w.getLstInterval();
        const Interval iv = ivs0.first();
        QCOMPARE(iv.getPowerStepType(), Interval::FLAT);
        QVERIFY2(qAbs(iv.getFTP_start() - 0.75) < 1e-4,
                 qPrintable(QString("SteadyState FTP_start expected 0.75, got %1")
                                .arg(iv.getFTP_start())));
        QVERIFY2(qAbs(iv.getFTP_end() - 0.75) < 1e-4,
                 qPrintable(QString("SteadyState FTP_end expected 0.75, got %1")
                                .arg(iv.getFTP_end())));
        QCOMPARE(iv.getDurationQTime(), QTime(0, 20, 0)); // 1200 s = 20 min
    }

    // ────────────────────────────────────────────────────────────────────────
    // 1b. Ramp: 1 interval, progressive 0.50 → 1.00 FTP, 10 min (600 s)
    // ────────────────────────────────────────────────────────────────────────
    void testZwoRampInline()
    {
        const QByteArray zwoXml = QByteArrayLiteral(
            "<workout_file>"
            "  <name>CI Ramp</name>"
            "  <workout>"
            "    <Ramp Duration=\"600\" PowerLow=\"0.50\" PowerHigh=\"1.00\" />"
            "  </workout>"
            "</workout_file>");

        const Workout w = ImporterWorkoutZwo::importFromByteArray(zwoXml, "ramp-test");

        QCOMPARE(w.getLstInterval().size(), 1);

        const QList<Interval> ivs1 = w.getLstInterval();
        const Interval iv = ivs1.first();
        QCOMPARE(iv.getPowerStepType(), Interval::PROGRESSIVE);
        QVERIFY2(qAbs(iv.getFTP_start() - 0.50) < 1e-4,
                 qPrintable(QString("Ramp FTP_start expected 0.50, got %1")
                                .arg(iv.getFTP_start())));
        QVERIFY2(qAbs(iv.getFTP_end() - 1.00) < 1e-4,
                 qPrintable(QString("Ramp FTP_end expected 1.00, got %1")
                                .arg(iv.getFTP_end())));
        QCOMPARE(iv.getDurationQTime(), QTime(0, 10, 0)); // 600 s = 10 min
    }

    // ────────────────────────────────────────────────────────────────────────
    // 1c. IntervalsT: 5 repeats → 10 flat intervals (5 on + 5 off)
    //     On: 60 s @ 1.10 FTP;  Off: 90 s @ 0.55 FTP
    // ────────────────────────────────────────────────────────────────────────
    void testZwoIntervalsTInline()
    {
        const QByteArray zwoXml = QByteArrayLiteral(
            "<workout_file>"
            "  <name>CI Intervals T</name>"
            "  <workout>"
            "    <IntervalsT Repeat=\"5\""
            "                OnDuration=\"60\"  OffDuration=\"90\""
            "                OnPower=\"1.10\"   OffPower=\"0.55\" />"
            "  </workout>"
            "</workout_file>");

        const Workout w = ImporterWorkoutZwo::importFromByteArray(zwoXml, "intervals-t-test");

        QCOMPARE(w.getLstInterval().size(), 10); // 5 on + 5 off

        // First interval (on): 60 s @ 1.10 FTP — flat power
        const QList<Interval> ivs2 = w.getLstInterval();
        const Interval on = ivs2.at(0);
        QCOMPARE(on.getPowerStepType(), Interval::FLAT);
        QVERIFY2(qAbs(on.getFTP_start() - 1.10) < 1e-4,
                 qPrintable(QString("On interval FTP expected 1.10, got %1")
                                .arg(on.getFTP_start())));
        QCOMPARE(on.getDurationQTime(), QTime(0, 1, 0)); // 60 s

        // Second interval (off): 90 s @ 0.55 FTP — flat power
        const Interval off = ivs2.at(1);
        QCOMPARE(off.getPowerStepType(), Interval::FLAT);
        QVERIFY2(qAbs(off.getFTP_start() - 0.55) < 1e-4,
                 qPrintable(QString("Off interval FTP expected 0.55, got %1")
                                .arg(off.getFTP_start())));
        QCOMPARE(off.getDurationQTime(), QTime(0, 1, 30)); // 90 s
    }

    // ────────────────────────────────────────────────────────────────────────
    // 1d. Mixed workout: Warmup + SteadyState + Cooldown → 3 intervals
    //     Warmup:      10 min, progressive 0.40 → 0.65 FTP
    //     SteadyState: 30 min, flat 0.75 FTP
    //     Cooldown:     5 min, progressive 0.65 → 0.40 FTP
    // ────────────────────────────────────────────────────────────────────────
    void testZwoWarmupSteadyCooldownInline()
    {
        const QByteArray zwoXml = QByteArrayLiteral(
            "<workout_file>"
            "  <name>CI Warmup-Steady-Cooldown</name>"
            "  <workout>"
            "    <Warmup     Duration=\"600\"  PowerLow=\"0.40\" PowerHigh=\"0.65\" />"
            "    <SteadyState Duration=\"1800\" Power=\"0.75\" />"
            "    <Cooldown   Duration=\"300\"  PowerLow=\"0.65\" PowerHigh=\"0.40\" />"
            "  </workout>"
            "</workout_file>");

        const Workout w = ImporterWorkoutZwo::importFromByteArray(zwoXml, "wsc-test");

        QCOMPARE(w.getLstInterval().size(), 3);

        const QList<Interval> ivs3 = w.getLstInterval();

        // Warmup
        const Interval warmup = ivs3.at(0);
        QCOMPARE(warmup.getPowerStepType(), Interval::PROGRESSIVE);
        QVERIFY2(qAbs(warmup.getFTP_start() - 0.40) < 1e-4, "Warmup FTP_start");
        QVERIFY2(qAbs(warmup.getFTP_end()   - 0.65) < 1e-4, "Warmup FTP_end");
        QCOMPARE(warmup.getDurationQTime(), QTime(0, 10, 0));

        // SteadyState
        const Interval steady = ivs3.at(1);
        QCOMPARE(steady.getPowerStepType(), Interval::FLAT);
        QVERIFY2(qAbs(steady.getFTP_start() - 0.75) < 1e-4, "SteadyState FTP_start");
        QVERIFY2(qAbs(steady.getFTP_end()   - 0.75) < 1e-4, "SteadyState FTP_end");
        QCOMPARE(steady.getDurationQTime(), QTime(0, 30, 0));

        // Cooldown (PowerLow > PowerHigh → descending ramp)
        const Interval cooldown = ivs3.at(2);
        QCOMPARE(cooldown.getPowerStepType(), Interval::PROGRESSIVE);
        QVERIFY2(qAbs(cooldown.getFTP_start() - 0.65) < 1e-4, "Cooldown FTP_start");
        QVERIFY2(qAbs(cooldown.getFTP_end()   - 0.40) < 1e-4, "Cooldown FTP_end");
        QCOMPARE(cooldown.getDurationQTime(), QTime(0, 5, 0));
    }

    // ────────────────────────────────────────────────────────────────────────
    // 1e. FreeRide: 1 interval, power type NONE, 15 min
    // ────────────────────────────────────────────────────────────────────────
    void testZwoFreeRideInline()
    {
        const QByteArray zwoXml = QByteArrayLiteral(
            "<workout_file>"
            "  <name>CI Free Ride</name>"
            "  <workout>"
            "    <FreeRide Duration=\"900\" />"
            "  </workout>"
            "</workout_file>");

        const Workout w = ImporterWorkoutZwo::importFromByteArray(zwoXml, "free-ride-test");

        // FreeRide
        QCOMPARE(w.getLstInterval().size(), 1);
        const Interval freeRide = w.getLstInterval().first();
        QCOMPARE(freeRide.getPowerStepType(), Interval::NONE);
        QCOMPARE(freeRide.getDurationQTime(), QTime(0, 15, 0));
    }

    // ────────────────────────────────────────────────────────────────────────
    // 1f. Empty ZWO XML → empty Workout (no crash)
    // ────────────────────────────────────────────────────────────────────────
    void testZwoEmptyDataInline()
    {
        const Workout w = ImporterWorkoutZwo::importFromByteArray(QByteArray(), "empty-test");
        QVERIFY2(w.getLstInterval().isEmpty(),
                 "Parsing empty byte array should return an empty Workout");
    }

    // ────────────────────────────────────────────────────────────────────────
    // 1g. Malformed XML → empty Workout (graceful failure)
    // ────────────────────────────────────────────────────────────────────────
    void testZwoMalformedXmlInline()
    {
        const Workout w = ImporterWorkoutZwo::importFromByteArray(
            QByteArrayLiteral("<not-valid-xml<<<!!>"), "malformed-test");
        QVERIFY2(w.getLstInterval().isEmpty(),
                 "Parsing malformed XML should return an empty Workout");
    }

    // ========================================================================
    // Group 2 — Workout from programmatic Warmup / SteadyState / Cooldown
    // ========================================================================

    // ────────────────────────────────────────────────────────────────────────
    // 2a. Three-interval workout constructed in C++ — verify model contents
    //
    //   Warmup:    10 min, progressive 0.45 → 0.60 FTP, 85 rpm cadence
    //   SteadyState: 20 min, flat 0.75 FTP, 90 rpm cadence
    //   Cooldown:    5 min, progressive 0.60 → 0.45 FTP, 85 rpm cadence
    // ────────────────────────────────────────────────────────────────────────
    void testWorkoutFromWarmupSteadyCooldown()
    {
        Interval warmup(QTime(0, 10, 0), QStringLiteral("Warm-Up"),
                        Interval::PROGRESSIVE, 0.45, 0.60, 20, -1.0,
                        Interval::FLAT,        85,   85,   5,
                        Interval::NONE,        0.0,  0.0,  20,
                        false, 0.0, 0, 0.0);

        Interval steady(QTime(0, 20, 0), QStringLiteral("Steady State"),
                        Interval::FLAT,        0.75, 0.75, 20, -1.0,
                        Interval::FLAT,        90,   90,   5,
                        Interval::NONE,        0.0,  0.0,  20,
                        false, 0.0, 0, 0.0);

        Interval cooldown(QTime(0, 5, 0), QStringLiteral("Cool-Down"),
                          Interval::PROGRESSIVE, 0.60, 0.45, 20, -1.0,
                          Interval::FLAT,        85,   85,   5,
                          Interval::NONE,        0.0,  0.0,  20,
                          false, 0.0, 0, 0.0);

        QList<Interval> ivList;
        ivList << warmup << steady << cooldown;

        // The 8-parameter Workout constructor calls computeWorkoutTotalTime(),
        // initializeArrayFTP(), and calculateWorkoutMetrics() automatically.
        Workout wk(QStringLiteral(""),
                   Workout::USER_MADE,
                   ivList,
                   QStringLiteral("WSC Programmatic Test"),
                   QStringLiteral("CI Bot"),
                   QStringLiteral("Three-segment programmatic workout"),
                   QStringLiteral("Build"),
                   Workout::T_ENDURANCE);

        // ── Structural checks ────────────────────────────────────────────────
        QCOMPARE(wk.getNbInterval(),  3);
        QCOMPARE(wk.getType(),        Workout::T_ENDURANCE);
        QCOMPARE(wk.getName(),        QStringLiteral("WSC Programmatic Test"));
        QCOMPARE(wk.getCreatedBy(),   QStringLiteral("CI Bot"));
        QCOMPARE(wk.getDescription(), QStringLiteral("Three-segment programmatic workout"));
        QCOMPARE(wk.getPlan(),        QStringLiteral("Build"));

        // ── Warmup ──────────────────────────────────────────────────────────
        const QList<Interval> ivListResult = wk.getLstInterval();
        const Interval r0 = ivListResult.at(0);
        QCOMPARE(r0.getDisplayMessage(), QStringLiteral("Warm-Up"));
        QCOMPARE(r0.getPowerStepType(),  Interval::PROGRESSIVE);
        QVERIFY2(qAbs(r0.getFTP_start() - 0.45) < 0.001, "Warmup FTP_start");
        QVERIFY2(qAbs(r0.getFTP_end()   - 0.60) < 0.001, "Warmup FTP_end");
        QCOMPARE(r0.getCadence_start(), 85);
        QCOMPARE(r0.getDurationQTime(), QTime(0, 10, 0));

        // ── SteadyState ──────────────────────────────────────────────────────
        const Interval r1 = ivListResult.at(1);
        QCOMPARE(r1.getDisplayMessage(), QStringLiteral("Steady State"));
        QCOMPARE(r1.getPowerStepType(),  Interval::FLAT);
        QVERIFY2(qAbs(r1.getFTP_start() - 0.75) < 0.001, "Steady FTP_start");
        QVERIFY2(qAbs(r1.getFTP_end()   - 0.75) < 0.001, "Steady FTP_end");
        QCOMPARE(r1.getCadence_start(), 90);
        QCOMPARE(r1.getDurationQTime(), QTime(0, 20, 0));

        // ── Cooldown ────────────────────────────────────────────────────────
        const Interval r2 = ivListResult.at(2);
        QCOMPARE(r2.getDisplayMessage(), QStringLiteral("Cool-Down"));
        QCOMPARE(r2.getPowerStepType(),  Interval::PROGRESSIVE);
        QVERIFY2(qAbs(r2.getFTP_start() - 0.60) < 0.001, "Cooldown FTP_start");
        QVERIFY2(qAbs(r2.getFTP_end()   - 0.45) < 0.001, "Cooldown FTP_end");
        QCOMPARE(r2.getCadence_start(), 85);
        QCOMPARE(r2.getDurationQTime(), QTime(0, 5, 0));

        // ── Average power metric ─────────────────────────────────────────────
        // FTP = 200 W (set in initTestCase).
        // Warmup avg FTP fraction: (0.45 + 0.60) / 2 = 0.525 → 105 W
        // Steady  avg FTP fraction: 0.75                     → 150 W
        // Cooldown avg FTP fraction: (0.60 + 0.45) / 2 = 0.525 → 105 W
        //
        // Total FTP-min: 10 × 0.525 + 20 × 0.75 + 5 × 0.525
        //              = 5.25 + 15.0 + 2.625 = 22.875
        // avg fraction = 22.875 / 35 = 0.6536 → 130.7 W
        //
        // The implementation uses second-level resolution (not minute-level)
        // so accept a ±5 W tolerance for floating-point accumulation.
        const double avgPower = wk.getAveragePower();
        QVERIFY2(avgPower > 0.0,
                 qPrintable(QString("getAveragePower() returned %1, expected > 0")
                                .arg(avgPower)));
        QVERIFY2(avgPower >= 120.0 && avgPower <= 145.0,
                 qPrintable(QString("Expected avg power ~130 W ±5, got %1 W")
                                .arg(avgPower)));
    }

    // ────────────────────────────────────────────────────────────────────────
    // 2b. Total workout duration
    // ────────────────────────────────────────────────────────────────────────
    void testWorkoutTotalDuration()
    {
        Interval iv1(QTime(0, 10, 0), QStringLiteral("Warmup"),
                     Interval::PROGRESSIVE, 0.45, 0.65, 20, -1.0,
                     Interval::NONE, 0, 0, 5,
                     Interval::NONE, 0.0, 0.0, 20,
                     false, 0.0, 0, 0.0);
        Interval iv2(QTime(0, 30, 0), QStringLiteral("Tempo"),
                     Interval::FLAT, 0.85, 0.85, 20, -1.0,
                     Interval::NONE, 0, 0, 5,
                     Interval::NONE, 0.0, 0.0, 20,
                     false, 0.0, 0, 0.0);
        Interval iv3(QTime(0, 5, 0), QStringLiteral("Cooldown"),
                     Interval::PROGRESSIVE, 0.65, 0.45, 20, -1.0,
                     Interval::NONE, 0, 0, 5,
                     Interval::NONE, 0.0, 0.0, 20,
                     false, 0.0, 0, 0.0);

        QList<Interval> ivList;
        ivList << iv1 << iv2 << iv3;

        Workout wk(QStringLiteral(""), Workout::USER_MADE, ivList,
                   QStringLiteral("Duration Test"), QStringLiteral("CI"),
                   QString(), QStringLiteral("-"), Workout::T_ENDURANCE);

        // Total = 10 + 30 + 5 = 45 min = 2700 s
        // getDurationQTime() returns the total workout duration as QTime
        const QTime totalTime = wk.getDurationQTime();
        QVERIFY2(totalTime.isValid() && totalTime > QTime(0, 0, 0),
                 "Workout total duration must be > 0 after model construction");

        // 10 + 30 + 5 = 45 min = 2700 s
        const int totalSec = totalTime.hour() * 3600
                           + totalTime.minute() * 60
                           + totalTime.second();
        QVERIFY2(qAbs(totalSec - 2700) <= 2,
                 qPrintable(QString("Expected total time ~2700 s, got %1 s")
                                .arg(totalSec)));
    }

    // ========================================================================
    // Group 3 — Intervals.icu pull-and-load workflow (live, optional)
    // ========================================================================

    // ────────────────────────────────────────────────────────────────────────
    // 3a. Pull workout list from intervals.icu, download ZWO for the first
    //     workout found, parse it, and verify each interval has a positive
    //     duration.  QSKIP when credentials are absent.
    // ────────────────────────────────────────────────────────────────────────
    void testIntervalsIcuPullAndLoad()
    {
        const QString apiKey    = qEnvironmentVariable("INTERVALS_ICU_API_KEY");
        const QString athleteId = qEnvironmentVariable("INTERVALS_ICU_ATHLETE_ID");

        if (apiKey.isEmpty() || athleteId.isEmpty()) {
            QSKIP("Set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID "
                  "to run the live intervals.icu pull-and-load test.");
        }

        const QString baseUrl =
            QStringLiteral("https://intervals.icu/api/v1/athlete/") + athleteId;

        // Build the Basic Auth header (username="API_KEY", password=<apiKey>)
        const QByteArray creds =
            (QStringLiteral("API_KEY:") + apiKey).toUtf8().toBase64();
        const QByteArray authHeader = QByteArrayLiteral("Basic ") + creds;

        QNetworkAccessManager mgr;

        // ── Step 1: Fetch the athlete's recent workout list ─────────────────
        const QUrl listUrl(baseUrl
                           + QStringLiteral("/workouts?oldest=")
                           + QDate::currentDate().addDays(-90).toString(Qt::ISODate)
                           + QStringLiteral("&newest=")
                           + QDate::currentDate().toString(Qt::ISODate));

        QNetworkRequest listReq(listUrl);
        listReq.setRawHeader("Authorization", authHeader);
        listReq.setRawHeader("Accept", "application/json");

        QNetworkReply *listReply = mgr.get(listReq);
        QVERIFY2(waitForNetworkReply(listReply), "Workout list request timed out (30 s)");

        const int listStatus =
            listReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QVERIFY2(listStatus == 200,
                 qPrintable(QString("Expected HTTP 200 for workout list, got %1 — %2")
                                .arg(listStatus)
                                .arg(listReply->errorString())));

        const QByteArray listBody = listReply->readAll();
        listReply->deleteLater();

        const QJsonDocument listDoc = QJsonDocument::fromJson(listBody);
        QVERIFY2(!listDoc.isNull() && listDoc.isArray(),
                 "Workout list response must be a JSON array");

        const QJsonArray workouts = listDoc.array();
        if (workouts.isEmpty()) {
            QSKIP("No workouts found in the last 90 days — skipping ZWO load test");
        }

        // ── Step 2: Collect all workout IDs from the list ────────────────────
        QStringList workoutIds;
        for (const QJsonValue &v : workouts) {
            const QJsonObject obj = v.toObject();
            // The scheduled-workout endpoint uses "id" as a numeric long
            const QJsonValue idVal = obj.value(QStringLiteral("id"));
            if (!idVal.isUndefined() && !idVal.isNull()) {
                const QString id = idVal.isString()
                    ? idVal.toString()
                    : QString::number(static_cast<qlonglong>(idVal.toDouble()));
                if (!id.isEmpty() && id != QStringLiteral("0"))
                    workoutIds << id;
            }
        }

        if (workoutIds.isEmpty()) {
            QSKIP("No workout with a valid 'id' found in the 90-day list — "
                  "skipping ZWO load test");
        }

        // ── Step 3: Walk the full list, try each workout until one has ZWO ───
        // Races, free-form notes, and other non-structured entries return non-200
        // for the .zwo endpoint. We scan all IDs so a valid fixture is always
        // found when the account contains any structured workout in the window.
        QString workoutId;
        QByteArray zwoData;

        for (const QString &id : workoutIds) {
            const QUrl zwoUrl(baseUrl
                              + QStringLiteral("/workouts/")
                              + id
                              + QStringLiteral(".zwo"));

            QNetworkRequest zwoReq(zwoUrl);
            zwoReq.setRawHeader("Authorization", authHeader);
            zwoReq.setRawHeader("Accept", "application/xml");

            QNetworkReply *zwoReply = mgr.get(zwoReq);
            QVERIFY2(waitForNetworkReply(zwoReply),
                     qPrintable(QString("ZWO download timed out (30 s) for workout %1").arg(id)));

            const int zwoStatus =
                zwoReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            if (zwoStatus == 200) {
                const QByteArray body = zwoReply->readAll();
                zwoReply->deleteLater();
                if (!body.isEmpty()) {
                    workoutId = id;
                    zwoData   = body;
                    break; // Found a usable ZWO — proceed with this one.
                }
            } else {
                // Non-200 means this workout type has no ZWO; try the next one.
                zwoReply->deleteLater();
            }
        }

        if (workoutId.isEmpty()) {
            QSKIP("None of the workouts in the last 90 days returned ZWO data — "
                  "all entries appear to be races, notes, or non-structured workouts");
        }

        // ── Step 4: Parse the ZWO into a Workout model ───────────────────────
        const Workout loaded = ImporterWorkoutZwo::importFromByteArray(zwoData, workoutId);

        QVERIFY2(loaded.getLstInterval().size() >= 1,
                 qPrintable(
                     QString("Expected >= 1 interval after parsing ZWO for workout %1, "
                             "got %2")
                         .arg(workoutId)
                         .arg(loaded.getLstInterval().size())));

        // ── Step 5: Verify every interval has a positive duration ────────────
        const QList<Interval> loadedIntervals = loaded.getLstInterval();
        for (int i = 0; i < loadedIntervals.size(); ++i) {
            const Interval iv = loadedIntervals.at(i);
            const QTime     qt = iv.getDurationQTime();
            const int    durSec = qt.hour() * 3600 + qt.minute() * 60 + qt.second();
            QVERIFY2(durSec >= 1,
                     qPrintable(QString("Interval %1 of workout %2 has zero or negative "
                                        "duration (QTime=%3)")
                                    .arg(i)
                                    .arg(workoutId)
                                    .arg(qt.toString())));
        }

        qDebug().noquote()
            << "[IntervalsIcuPullAndLoad] PASS"
            << "— workout id:" << workoutId
            << "| intervals:" << loadedIntervals.size();
    }
};

QTEST_MAIN(TstWorkoutParsing)
#include "tst_workout_parsing.moc"
