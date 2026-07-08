/*
 * tst_workout_io.cpp
 *
 * Workout I/O Tests -- MaximumTrainer
 *
 * Purpose
 * -----------------------------------------------------------------------
 * Validates the workout import/export pipeline, including ZWO parsing from
 * inline source strings and a mock Intervals.icu API response workflow.
 * All tests run headlessly (no display required) because they exercise only
 * the model and network/XML parsing layers.
 *
 * Test coverage:
 *
 *   ZWO Parsing (ImporterWorkoutZwo)
 *   ─────────────────────────────────
 *   1. testZwo_steadyState              — single SteadyState at 0.75 FTP
 *   2. testZwo_rampIsProgressive        — single Ramp (PROGRESSIVE step type)
 *   3. testZwo_rampPowerRange           — Ramp power bounds (0.50 → 1.00)
 *   4. testZwo_intervalsTExpansion      — IntervalsT Repeat=5 → 10 intervals
 *   5. testZwo_intervalsTOnOffPower     — correct on/off power values
 *   6. testZwo_freeRide                 — FreeRide → NONE power type
 *   7. testZwo_mixedWorkout             — 10 intervals from mixed structure
 *   8. testZwo_nameFromTag              — <name> element → Workout::getName
 *   9. testZwo_emptyInput               — empty byte array → empty Workout
 *  10. testZwo_malformedXml            — bad XML → empty Workout (no crash)
 *  11. testZwo_powerZero               — Power="0.0" is a valid flat interval
 *  12. testZwo_powerAboveFtp           — Power="1.5" accepted as-is
 *
 *   Intervals.icu Mock API Response
 *   ─────────────────────────────────
 *  13. testIntervalsIcuMock_zwoParseFromJson
 *        — Simulate the "Pull and Load" workflow:
 *          • Build a mock JSON response identical to what
 *            GET /athlete/{id}/workouts would return.
 *          • Extract the "workoutContent" ZWO field from the JSON.
 *          • Pass the ZWO string to ImporterWorkoutZwo::importFromByteArray.
 *          • Verify the Workout object is correctly populated.
 *
 *  14. testIntervalsIcuMock_emptyWorkoutList
 *        — A mock JSON response with an empty array produces no workout.
 *
 *  15. testIntervalsIcuMock_missingZwoField
 *        — A JSON workout object without a "workoutContent" field is handled
 *          gracefully (falls back to empty Workout, no crash).
 *
 *   XmlUtil Round-trip (native .xml format)
 *   ─────────────────────────────────────────
 *  16. testXmlRoundTrip_intervalCount  — create + parse XML; interval count matches
 *  17. testXmlRoundTrip_power          — power fractions survive the round-trip
 *  18. testXmlRoundTrip_duration       — interval durations survive the round-trip
 *  19. testXmlRoundTrip_workoutName    — workout name survives the round-trip
 *  20. testXmlRoundTrip_plan           — plan field survives the round-trip
 *
 * Build:
 *   qmake workout_io_tests.pro && make
 * Run:
 *   ../../build/tests/workout_io_tests -v2
 */

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QByteArray>
#include <QString>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QDir>
#include <QDirIterator>

#include "../../src/app/util.h"
#include "../../src/persistence/file/importerworkoutzwo.h"
#include "../../src/persistence/file/xmlutil.h"
#include "../../src/model/account.h"
#include "../../src/model/workout.h"
#include "../../src/model/interval.h"

// ---------------------------------------------------------------------------
// TstWorkoutIo — QTest class
// ---------------------------------------------------------------------------
class TstWorkoutIo : public QObject
{
    Q_OBJECT

private:
    Account *m_account = nullptr;

private slots:

    void initTestCase()
    {
        m_account = new Account(this);
        m_account->FTP  = 200;
        m_account->LTHR = 160;
        m_account->email       = QStringLiteral("test@ci.example");
        m_account->email_clean = QStringLiteral("testciexample");
        m_account->isOffline   = true;
        qApp->setProperty("Account", QVariant::fromValue<Account *>(m_account));
    }

    void cleanupTestCase()
    {
        qApp->setProperty("Account", QVariant());
    }

    // =========================================================================
    // ZWO PARSING TESTS
    // =========================================================================

    // -----------------------------------------------------------------------
    // 1. SteadyState — one interval, FLAT, 0.75 FTP, 30-minute duration
    // -----------------------------------------------------------------------
    void testZwo_steadyState()
    {
        static const QByteArray xml = R"(
<workout_file>
  <name>Steady State Test</name>
  <workout>
    <SteadyState Duration="1800" Power="0.75"/>
  </workout>
</workout_file>)";

        const Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "test");
        QCOMPARE(w.getLstInterval().size(), 1);

        const Interval iv = w.getLstInterval().first();
        QCOMPARE(iv.getPowerStepType(), Interval::FLAT);
        QVERIFY(qAbs(iv.getFTP_start() - 0.75) < 1e-6);
        QVERIFY(qAbs(iv.getFTP_end()   - 0.75) < 1e-6);
        QCOMPARE(iv.getDurationQTime(), QTime(0, 30, 0));

        qDebug().noquote() << "[Zwo/SteadyState] 1 FLAT interval at 0.75 FTP, 30 min — PASS";
    }

    // -----------------------------------------------------------------------
    // 2. Ramp — step type must be PROGRESSIVE
    // -----------------------------------------------------------------------
    void testZwo_rampIsProgressive()
    {
        static const QByteArray xml = R"(
<workout_file>
  <workout>
    <Ramp Duration="600" PowerLow="0.50" PowerHigh="1.00"/>
  </workout>
</workout_file>)";

        const Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "ramp");
        QVERIFY(!w.getLstInterval().isEmpty());
        QCOMPARE(w.getLstInterval().first().getPowerStepType(), Interval::PROGRESSIVE);

        qDebug().noquote() << "[Zwo/Ramp] PROGRESSIVE step type — PASS";
    }

    // -----------------------------------------------------------------------
    // 3. Ramp power range
    // -----------------------------------------------------------------------
    void testZwo_rampPowerRange()
    {
        static const QByteArray xml = R"(
<workout_file>
  <workout>
    <Ramp Duration="600" PowerLow="0.50" PowerHigh="1.00"/>
  </workout>
</workout_file>)";

        const Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "ramp");
        QVERIFY(!w.getLstInterval().isEmpty());
        const Interval iv = w.getLstInterval().first();
        QVERIFY(qAbs(iv.getFTP_start() - 0.50) < 1e-6);
        QVERIFY(qAbs(iv.getFTP_end()   - 1.00) < 1e-6);

        qDebug().noquote() << "[Zwo/Ramp] Power range 0.50 → 1.00 — PASS";
    }

    // -----------------------------------------------------------------------
    // 4. IntervalsT Repeat=5 → 10 intervals
    // -----------------------------------------------------------------------
    void testZwo_intervalsTExpansion()
    {
        static const QByteArray xml = R"(
<workout_file>
  <workout>
    <IntervalsT Repeat="5" OnDuration="60" OffDuration="30"
                OnPower="1.10" OffPower="0.55"/>
  </workout>
</workout_file>)";

        const Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "ints");
        QCOMPARE(w.getLstInterval().size(), 10);

        qDebug().noquote() << "[Zwo/IntervalsT] Repeat=5 → 10 intervals — PASS";
    }

    // -----------------------------------------------------------------------
    // 5. IntervalsT on/off power values
    // -----------------------------------------------------------------------
    void testZwo_intervalsTOnOffPower()
    {
        static const QByteArray xml = R"(
<workout_file>
  <workout>
    <IntervalsT Repeat="3" OnDuration="90" OffDuration="60"
                OnPower="1.15" OffPower="0.50"/>
  </workout>
</workout_file>)";

        const Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "ints");
        QVERIFY(w.getLstInterval().size() >= 2);

        // First interval (index 0) = on-power
        QVERIFY(qAbs(w.getLstInterval().at(0).getFTP_start() - 1.15) < 1e-6);
        // Second interval (index 1) = off-power
        QVERIFY(qAbs(w.getLstInterval().at(1).getFTP_start() - 0.50) < 1e-6);

        qDebug().noquote() << "[Zwo/IntervalsT] On=1.15, Off=0.50 — PASS";
    }

    // -----------------------------------------------------------------------
    // 6. FreeRide → NONE power type
    // -----------------------------------------------------------------------
    void testZwo_freeRide()
    {
        static const QByteArray xml = R"(
<workout_file>
  <workout>
    <FreeRide Duration="600"/>
  </workout>
</workout_file>)";

        const Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "free");
        QCOMPARE(w.getLstInterval().size(), 1);
        QCOMPARE(w.getLstInterval().first().getPowerStepType(), Interval::NONE);

        qDebug().noquote() << "[Zwo/FreeRide] NONE power type — PASS";
    }

    // -----------------------------------------------------------------------
    // 7. Mixed workout: Warmup(1) + Steady(1) + IntervalsT×3(6) + FreeRide(1)
    //    + Cooldown(1) = 10 intervals
    // -----------------------------------------------------------------------
    void testZwo_mixedWorkout()
    {
        static const QByteArray xml = R"(
<workout_file>
  <name>Mixed Workout</name>
  <workout>
    <Warmup     Duration="600"  PowerLow="0.40" PowerHigh="0.75"/>
    <SteadyState Duration="300" Power="0.75"/>
    <IntervalsT  Repeat="3"    OnDuration="120" OffDuration="60"
                 OnPower="1.15" OffPower="0.50"/>
    <FreeRide    Duration="600"/>
    <Cooldown    Duration="600"  PowerLow="0.75" PowerHigh="0.40"/>
  </workout>
</workout_file>)";

        const Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "mixed");
        QCOMPARE(w.getLstInterval().size(), 10);

        // First interval must be the Warmup ramp (PROGRESSIVE)
        QCOMPARE(w.getLstInterval().first().getPowerStepType(), Interval::PROGRESSIVE);

        qDebug().noquote() << "[Zwo/Mixed] 10 intervals, first is PROGRESSIVE — PASS";
    }

    // -----------------------------------------------------------------------
    // 8. <name> element → Workout::getName() contains the value
    // -----------------------------------------------------------------------
    void testZwo_nameFromTag()
    {
        static const QByteArray xml = R"(
<workout_file>
  <name>Named ZWO Workout</name>
  <workout>
    <SteadyState Duration="600" Power="0.80"/>
  </workout>
</workout_file>)";

        const Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "fallback");
        QVERIFY2(w.getName().contains(QLatin1String("Named ZWO Workout")),
                 qPrintable(QString("Expected 'Named ZWO Workout' in '%1'").arg(w.getName())));

        qDebug().noquote()
            << "[Zwo/Name] Name extracted:" << w.getName() << "— PASS";
    }

    // -----------------------------------------------------------------------
    // 9. Empty input → empty Workout, no crash
    // -----------------------------------------------------------------------
    void testZwo_emptyInput()
    {
        const Workout w = ImporterWorkoutZwo::importFromByteArray(QByteArray(), "empty");
        QVERIFY(w.getLstInterval().isEmpty());
        qDebug().noquote() << "[Zwo/Empty] Empty input → empty Workout — PASS";
    }

    // -----------------------------------------------------------------------
    // 10. Malformed XML → empty Workout, no crash
    // -----------------------------------------------------------------------
    void testZwo_malformedXml()
    {
        const Workout w = ImporterWorkoutZwo::importFromByteArray("<bad <<< xml", "bad");
        QVERIFY(w.getLstInterval().isEmpty());
        qDebug().noquote() << "[Zwo/Malformed] Malformed XML → empty Workout — PASS";
    }

    // -----------------------------------------------------------------------
    // 11. Power="0.0" is a valid flat interval (not discarded)
    // -----------------------------------------------------------------------
    void testZwo_powerZero()
    {
        static const QByteArray xml = R"(
<workout_file>
  <workout>
    <SteadyState Duration="60" Power="0.0"/>
  </workout>
</workout_file>)";

        const Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "zero");
        QCOMPARE(w.getLstInterval().size(), 1);
        QCOMPARE(w.getLstInterval().first().getPowerStepType(), Interval::FLAT);
        QVERIFY(qAbs(w.getLstInterval().first().getFTP_start()) < 1e-6);

        qDebug().noquote() << "[Zwo/PowerZero] Power=0.0 accepted — PASS";
    }

    // -----------------------------------------------------------------------
    // 12. Power above FTP (1.5) is stored as-is
    // -----------------------------------------------------------------------
    void testZwo_powerAboveFtp()
    {
        static const QByteArray xml = R"(
<workout_file>
  <workout>
    <SteadyState Duration="120" Power="1.5"/>
  </workout>
</workout_file>)";

        const Workout w = ImporterWorkoutZwo::importFromByteArray(xml, "high");
        QVERIFY(!w.getLstInterval().isEmpty());
        QVERIFY(qAbs(w.getLstInterval().first().getFTP_start() - 1.5) < 1e-6);

        qDebug().noquote() << "[Zwo/PowerHigh] Power=1.5 stored as-is — PASS";
    }

    // =========================================================================
    // INTERVALS.ICU MOCK API RESPONSE TESTS
    //
    // These tests simulate the "Pull and Load" workflow without any real network
    // call.  A JSON response identical in structure to the one returned by
    // GET https://intervals.icu/api/v1/athlete/{id}/workouts is constructed
    // in-process and fed to the ZWO parser.
    // =========================================================================

    // -----------------------------------------------------------------------
    // 13. Mock workout list → extract ZWO → parse Workout model
    //
    // This exercises the full data flow:
    //   JSON (mock)  →  extract "workoutContent" field  →
    //   ImporterWorkoutZwo::importFromByteArray  →  Workout model
    //
    // The JSON structure mirrors the real intervals.icu /workouts response.
    // -----------------------------------------------------------------------
    void testIntervalsIcuMock_zwoParseFromJson()
    {
        // Build a mock JSON response (identical structure to the real API).
        // The important field is "workoutContent" — the embedded ZWO XML string.
        const QString mockZwoXml = QStringLiteral(
            "<workout_file>"
            "<name>Intervals.icu Mock Workout</name>"
            "<author>CI Mock</author>"
            "<description>Pulled from intervals.icu (mocked)</description>"
            "<workout>"
            "<Warmup Duration=\"600\" PowerLow=\"0.40\" PowerHigh=\"0.75\"/>"
            "<SteadyState Duration=\"1800\" Power=\"0.80\"/>"
            "<Cooldown Duration=\"300\" PowerLow=\"0.70\" PowerHigh=\"0.40\"/>"
            "</workout>"
            "</workout_file>");

        QJsonObject mockWorkout;
        mockWorkout["id"]             = QStringLiteral("mock_001");
        mockWorkout["name"]           = QStringLiteral("Intervals.icu Mock Workout");
        mockWorkout["description"]    = QStringLiteral("Pulled from intervals.icu (mocked)");
        mockWorkout["workoutContent"] = mockZwoXml;
        mockWorkout["tss"]            = 55;
        mockWorkout["duration"]       = 2700;

        QJsonArray mockResponseArray;
        mockResponseArray.append(mockWorkout);

        const QJsonDocument doc(mockResponseArray);
        const QByteArray jsonBytes = doc.toJson(QJsonDocument::Compact);

        // ── Parse the JSON (mimics what Tab_Intervals_Icu does) ──────────────
        const QJsonDocument parsedDoc = QJsonDocument::fromJson(jsonBytes);
        QVERIFY2(!parsedDoc.isNull(), "Mock JSON round-trip failed");
        QVERIFY2(parsedDoc.isArray(), "Mock response must be a JSON array");

        const QJsonArray arr = parsedDoc.array();
        QVERIFY2(!arr.isEmpty(), "Mock workout array must not be empty");

        const QJsonObject firstWorkout = arr.at(0).toObject();
        const QString workoutContent =
            firstWorkout.value(QStringLiteral("workoutContent")).toString();
        QVERIFY2(!workoutContent.isEmpty(),
                 "workoutContent field must be present and non-empty");

        // ── Parse the embedded ZWO string ────────────────────────────────────
        const QByteArray zwoBytes = workoutContent.toUtf8();
        const Workout w = ImporterWorkoutZwo::importFromByteArray(
            zwoBytes,
            firstWorkout.value(QStringLiteral("name")).toString());

        QCOMPARE(w.getLstInterval().size(), 3);

        // Warmup (PROGRESSIVE)
        QCOMPARE(w.getLstInterval().at(0).getPowerStepType(), Interval::PROGRESSIVE);
        // SteadyState (FLAT, 0.80)
        QCOMPARE(w.getLstInterval().at(1).getPowerStepType(), Interval::FLAT);
        QVERIFY(qAbs(w.getLstInterval().at(1).getFTP_start() - 0.80) < 1e-6);
        // Cooldown (PROGRESSIVE)
        QCOMPARE(w.getLstInterval().at(2).getPowerStepType(), Interval::PROGRESSIVE);

        QVERIFY2(w.getName().contains(QLatin1String("Intervals.icu Mock Workout")),
                 qPrintable(QString("Workout name should contain 'Intervals.icu Mock Workout', got: '%1'")
                                .arg(w.getName())));

        qDebug().noquote()
            << "[Mock/IntervalsIcu] JSON → ZWO → 3 intervals, name correct — PASS";
    }

    // -----------------------------------------------------------------------
    // 14. Empty workout list → no workout produced
    // -----------------------------------------------------------------------
    void testIntervalsIcuMock_emptyWorkoutList()
    {
        const QJsonArray emptyArray;
        const QJsonDocument doc(emptyArray);
        const QByteArray jsonBytes = doc.toJson();

        const QJsonDocument parsed = QJsonDocument::fromJson(jsonBytes);
        QVERIFY(parsed.isArray());
        QVERIFY(parsed.array().isEmpty());

        // No workout can be extracted from an empty array.
        qDebug().noquote()
            << "[Mock/IntervalsIcu] Empty workout list handled gracefully — PASS";
    }

    // -----------------------------------------------------------------------
    // 15. Missing "workoutContent" field → graceful fallback (empty Workout)
    // -----------------------------------------------------------------------
    void testIntervalsIcuMock_missingZwoField()
    {
        QJsonObject workout;
        workout["id"]   = QStringLiteral("no_zwo_001");
        workout["name"] = QStringLiteral("Workout Without Content");
        // intentionally omit "workoutContent"

        const QString workoutContent =
            workout.value(QStringLiteral("workoutContent")).toString();
        QVERIFY2(workoutContent.isEmpty(),
                 "Missing workoutContent field must return an empty string");

        // Passing an empty string must produce an empty Workout without crash.
        const Workout w =
            ImporterWorkoutZwo::importFromByteArray(workoutContent.toUtf8(), "fallback");
        QVERIFY(w.getLstInterval().isEmpty());

        qDebug().noquote()
            << "[Mock/IntervalsIcu] Missing workoutContent → empty Workout — PASS";
    }

    // =========================================================================
    // XMLUTIL ROUND-TRIP TESTS (native MaximumTrainer .xml format)
    // =========================================================================

    // Helper: build a three-interval Workout object using the domain model.
    static Workout buildTestWorkout()
    {
        QList<Interval> intervals;
        // Interval 1: Warmup — PROGRESSIVE 0.40 → 0.70 FTP, 10 min
        {
            Interval iv;
            iv.setTime(QTime(0, 10, 0));
            iv.setDisplayMsg(QStringLiteral("Warm-Up"));
            iv.setPowerStepType(Interval::PROGRESSIVE);
            iv.setTargetFTP_start(0.40);
            iv.setTargetFTP_end(0.70);
            iv.setTargetFTP_range(20);
            iv.setCadenceStepType(Interval::FLAT);
            iv.setTargetCadence_start(85);
            iv.setTargetCadence_end(85);
            iv.setTargetCadence_range(5);
            iv.setHrStepType(Interval::NONE);
            iv.setTargetHR_start(0.0);
            iv.setTargetHR_end(0.0);
            iv.setTargetHR_range(20);
            iv.setTestInterval(false);
            iv.setRepeatIncreaseFTP(0.0);
            iv.setRepeatIncreaseCadence(0);
            iv.setRepeatIncreaseLTHR(0.0);
            intervals.append(iv);
        }
        // Interval 2: Main Set — FLAT 0.85 FTP, 20 min
        {
            Interval iv;
            iv.setTime(QTime(0, 20, 0));
            iv.setDisplayMsg(QStringLiteral("Main Set"));
            iv.setPowerStepType(Interval::FLAT);
            iv.setTargetFTP_start(0.85);
            iv.setTargetFTP_end(0.85);
            iv.setTargetFTP_range(20);
            iv.setCadenceStepType(Interval::FLAT);
            iv.setTargetCadence_start(90);
            iv.setTargetCadence_end(90);
            iv.setTargetCadence_range(5);
            iv.setHrStepType(Interval::NONE);
            iv.setTargetHR_start(0.0);
            iv.setTargetHR_end(0.0);
            iv.setTargetHR_range(20);
            iv.setTestInterval(false);
            iv.setRepeatIncreaseFTP(0.0);
            iv.setRepeatIncreaseCadence(0);
            iv.setRepeatIncreaseLTHR(0.0);
            intervals.append(iv);
        }
        // Interval 3: Cooldown — PROGRESSIVE 0.70 → 0.40 FTP, 10 min
        {
            Interval iv;
            iv.setTime(QTime(0, 10, 0));
            iv.setDisplayMsg(QStringLiteral("Cool-Down"));
            iv.setPowerStepType(Interval::PROGRESSIVE);
            iv.setTargetFTP_start(0.70);
            iv.setTargetFTP_end(0.40);
            iv.setTargetFTP_range(20);
            iv.setCadenceStepType(Interval::FLAT);
            iv.setTargetCadence_start(80);
            iv.setTargetCadence_end(80);
            iv.setTargetCadence_range(5);
            iv.setHrStepType(Interval::NONE);
            iv.setTargetHR_start(0.0);
            iv.setTargetHR_end(0.0);
            iv.setTargetHR_range(20);
            iv.setTestInterval(false);
            iv.setRepeatIncreaseFTP(0.0);
            iv.setRepeatIncreaseCadence(0);
            iv.setRepeatIncreaseLTHR(0.0);
            intervals.append(iv);
        }

        return Workout(
            QStringLiteral(""),       // filePath
            Workout::USER_MADE,       // workoutNameEnum
            intervals,                // lstInterval
            QStringLiteral("Round-Trip Workout"), // name
            QStringLiteral("CI"),     // createdBy
            QStringLiteral("XML round-trip test"), // description
            QStringLiteral("Base"),   // plan
            Workout::T_ENDURANCE      // type
        );
    }

    // -----------------------------------------------------------------------
    // 16. XmlUtil round-trip: interval count
    // -----------------------------------------------------------------------
    void testXmlRoundTrip_intervalCount()
    {
        const Workout original = buildTestWorkout();

        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);
        QVERIFY(tmpFile.open());
        const QString path = tmpFile.fileName();
        tmpFile.close();

        QVERIFY2(XmlUtil::createWorkoutXml(original, path),
                 qPrintable(QString("Failed to write workout XML to: %1").arg(path)));

        XmlUtil util;
        const Workout parsed = util.parseSingleWorkoutXml(path);

        QCOMPARE(parsed.getLstInterval().size(), 3);
        qDebug().noquote()
            << "[XmlRoundTrip] Interval count 3 survived round-trip — PASS";

        QFile::remove(path);
    }

    // -----------------------------------------------------------------------
    // 17. XmlUtil round-trip: power fractions
    // -----------------------------------------------------------------------
    void testXmlRoundTrip_power()
    {
        const Workout original = buildTestWorkout();

        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);
        QVERIFY(tmpFile.open());
        const QString path = tmpFile.fileName();
        tmpFile.close();

        QVERIFY(XmlUtil::createWorkoutXml(original, path));

        XmlUtil util;
        const Workout parsed = util.parseSingleWorkoutXml(path);
        QVERIFY(parsed.getLstInterval().size() >= 2);

        // Second interval (index 1) is the FLAT main set at 0.85 FTP
        const Interval iv = parsed.getLstInterval().at(1);
        QCOMPARE(iv.getPowerStepType(), Interval::FLAT);
        QVERIFY(qAbs(iv.getFTP_start() - 0.85) < 1e-3);

        qDebug().noquote()
            << "[XmlRoundTrip] FLAT power 0.85 survived round-trip — PASS";

        QFile::remove(path);
    }

    // -----------------------------------------------------------------------
    // 18. XmlUtil round-trip: interval durations
    // -----------------------------------------------------------------------
    void testXmlRoundTrip_duration()
    {
        const Workout original = buildTestWorkout();

        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);
        QVERIFY(tmpFile.open());
        const QString path = tmpFile.fileName();
        tmpFile.close();

        QVERIFY(XmlUtil::createWorkoutXml(original, path));

        XmlUtil util;
        const Workout parsed = util.parseSingleWorkoutXml(path);
        QVERIFY(parsed.getLstInterval().size() >= 2);

        // Second interval (Main Set) is 20 min
        QCOMPARE(parsed.getLstInterval().at(1).getDurationQTime(), QTime(0, 20, 0));

        qDebug().noquote()
            << "[XmlRoundTrip] 20-min interval duration survived round-trip — PASS";

        QFile::remove(path);
    }

    // -----------------------------------------------------------------------
    // 19. XmlUtil round-trip: workout name
    //
    // The native MaximumTrainer XML format does NOT store the workout name
    // inside the XML — it derives the name from the file's base name on
    // parse.  This test verifies that behaviour: a file named
    // "round_trip_workout.xml" produces getName() == "round_trip_workout".
    // -----------------------------------------------------------------------
    void testXmlRoundTrip_workoutName()
    {
        const Workout original = buildTestWorkout();

        // Use a known file name so we can predict what parseSingleWorkoutXml
        // will return as the workout name (the file's base name).
        const QString path =
            QDir::temp().absoluteFilePath(QStringLiteral("round_trip_workout.xml"));
        QFile::remove(path);  // ensure clean state

        QVERIFY2(XmlUtil::createWorkoutXml(original, path),
                 qPrintable(QString("Failed to write workout XML to: %1").arg(path)));

        XmlUtil util;
        const Workout parsed = util.parseSingleWorkoutXml(path);

        // The native XML format uses the file's base name as the workout name.
        QCOMPARE(parsed.getName(), QStringLiteral("round_trip_workout"));

        qDebug().noquote()
            << "[XmlRoundTrip] Workout name derived from file base name:"
            << parsed.getName() << "— PASS";

        QFile::remove(path);
    }

    // -----------------------------------------------------------------------
    // 20. XmlUtil round-trip: plan field
    // -----------------------------------------------------------------------
    void testXmlRoundTrip_plan()
    {
        const Workout original = buildTestWorkout();

        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);
        QVERIFY(tmpFile.open());
        const QString path = tmpFile.fileName();
        tmpFile.close();

        QVERIFY(XmlUtil::createWorkoutXml(original, path));

        XmlUtil util;
        const Workout parsed = util.parseSingleWorkoutXml(path);

        QCOMPARE(parsed.getPlan(), QStringLiteral("Base"));

        qDebug().noquote()
            << "[XmlRoundTrip] Plan field 'Base' survived round-trip — PASS";

        QFile::remove(path);
    }

    // =========================================================================
    // BUNDLED TRAINING PLANS — read from the included_workout/ directory the
    // .pro copies next to the test binary, so the tests also run on CI test
    // shards that only download the build artifact (no source checkout).
    // =========================================================================

    static QString bundledPlansRoot()
    {
        return QCoreApplication::applicationDirPath()
               + QStringLiteral("/included_workout");
    }

    // -----------------------------------------------------------------------
    // 21. Every bundled .workout file parses into a valid workout: at least
    //     one interval after repeat expansion, positive durations, and power
    //     targets in a sane 0.2–1.5 FTP band (intervals without a power
    //     target must carry an HR target instead).
    // -----------------------------------------------------------------------
    void testBundledWorkouts_allParseValid()
    {
        QVERIFY2(QDir(bundledPlansRoot()).exists(),
                 "included_workout/ not found next to the test binary");

        XmlUtil util;
        int filesChecked = 0;

        QDirIterator it(bundledPlansRoot(),
                        {QStringLiteral("*.workout")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString file = it.fileName();
            const Workout workout = util.parseSingleWorkoutXml(path);

            QVERIFY2(workout.getNbInterval() > 0,
                     qPrintable(file + QStringLiteral(": no intervals after expansion")));

            const QList<Interval> intervals = workout.getLstInterval();
            for (const Interval &interval : intervals) {
                QVERIFY2(QTime(0, 0, 0) < interval.getDurationQTime(),
                         qPrintable(file + QStringLiteral(": interval with zero duration")));
                if (interval.getPowerStepType() != Interval::StepType::NONE) {
                    QVERIFY2(interval.getFTP_start() >= 0.2 && interval.getFTP_start() <= 1.5,
                             qPrintable(file + QStringLiteral(": power start out of range")));
                    QVERIFY2(interval.getFTP_end() >= 0.2 && interval.getFTP_end() <= 1.5,
                             qPrintable(file + QStringLiteral(": power end out of range")));
                }
                else {
                    // No power target: the interval must carry an HR target
                    // (fraction of LTHR) so it still guides the rider.
                    QVERIFY2(interval.getHRStepType() != Interval::StepType::NONE,
                             qPrintable(file + QStringLiteral(": interval with neither power nor HR target")));
                    QVERIFY2(interval.getHR_start() >= 0.5 && interval.getHR_start() <= 1.1,
                             qPrintable(file + QStringLiteral(": HR start out of range")));
                    QVERIFY2(interval.getHR_end() >= 0.5 && interval.getHR_end() <= 1.1,
                             qPrintable(file + QStringLiteral(": HR end out of range")));
                }
            }
            filesChecked++;
        }

        QVERIFY2(filesChecked > 0, "no bundled .workout resources found");
        qDebug().noquote() << "[BundledPlans]" << filesChecked
                           << "bundled workout resources parsed and validated — PASS";
    }

    // -----------------------------------------------------------------------
    // 22. The bundled training plans ship the expected number of workouts,
    //     grouped by their Plan field.
    // -----------------------------------------------------------------------
    void testBundledPlans_expectedCounts()
    {
        XmlUtil util;
        QMap<QString, int> planCounts;

        QDirIterator it(bundledPlansRoot(),
                        {QStringLiteral("*.workout")}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const Workout workout = util.parseSingleWorkoutXml(it.next());
            planCounts[workout.getPlan()]++;
        }

        const QMap<QString, int> expected = {
            { QStringLiteral("Base Camp"),       12 },
            { QStringLiteral("FTP Kickstart"),    9 },
            { QStringLiteral("Polarized 3x"),     3 },
            { QStringLiteral("VO2 Shock Block"),  6 },
            { QStringLiteral("Lunch Crunch"),     9 },
            { QStringLiteral("Heart Rate Base"),  9 },
        };
        QCOMPARE(planCounts, expected);

        qDebug().noquote()
            << "[BundledPlans] plan counts and Plan fields all match — PASS";
    }

    // =========================================================================
    // LOCAL .SAVE FILE (XmlUtil::parseLocalSaveFile)
    // =========================================================================

    // -----------------------------------------------------------------------
    // 23. A .save file with an EMPTY AthleteId (as left behind by a logout)
    //     must not clobber a live athlete id — that wiped the id right after
    //     every OAuth login and made the session unrestorable on relaunch.
    //     A populated AthleteId must still load normally.
    // -----------------------------------------------------------------------
    void testLocalSaveFile_emptyAthleteIdDoesNotClobber()
    {
        const QString savePath = Util::getMaximumTrainerDocumentPath()
                                 + QDir::separator() + m_account->email_clean
                                 + QStringLiteral(".save");
        const auto writeSave = [&savePath](const QString &athleteId) {
            QFile f(savePath);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(QStringLiteral(
                        "<?xml version=\"1.0\"?>\n<MaximumTrainer>\n"
                        "  <WorkoutDone></WorkoutDone>\n"
                        "  <IntervalsIcu>\n"
                        "    <AthleteId>%1</AthleteId>\n"
                        "    <ApiKey></ApiKey>\n"
                        "  </IntervalsIcu>\n"
                        "</MaximumTrainer>\n").arg(athleteId).toUtf8());
            f.close();
        };

        // Poisoned file (empty id) must not wipe the live value.
        m_account->intervals_icu_athlete_id = QStringLiteral("i-live-42");
        m_account->intervals_icu_api_key    = QStringLiteral("live-key");
        writeSave(QString());
        XmlUtil::parseLocalSaveFile(m_account);
        QCOMPARE(m_account->intervals_icu_athlete_id, QStringLiteral("i-live-42"));
        QCOMPARE(m_account->intervals_icu_api_key,    QStringLiteral("live-key"));

        // A populated file still loads.
        writeSave(QStringLiteral("i-from-file"));
        XmlUtil::parseLocalSaveFile(m_account);
        QCOMPARE(m_account->intervals_icu_athlete_id, QStringLiteral("i-from-file"));

        QFile::remove(savePath);
        m_account->intervals_icu_athlete_id.clear();
        m_account->intervals_icu_api_key.clear();

        qDebug().noquote()
            << "[LocalSave] empty AthleteId no longer clobbers the live id — PASS";
    }
};

QTEST_MAIN(TstWorkoutIo)
#include "tst_workout_io.moc"
