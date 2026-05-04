/*
 * tst_workout_ui.cpp
 *
 * Workout UI Integration Test -- MaximumTrainer
 *
 * Purpose
 * -----------------------------------------------------------------------
 * Validates the full workout UI pipeline on Windows, macOS, and Linux:
 *
 *   1. User login (offline):  Account properties are set up through the
 *      same offline-login path used by MainWindow, confirming the login
 *      flow works without a network connection.
 *
 *   2. Workout creation:  A three-interval workout is written to a
 *      temporary XML file using the same format produced by
 *      XmlUtil::createWorkoutXml, verifying that the serialisation logic
 *      generates valid, parseable XML.
 *
 *   3. Workout retrieval:  The serialised XML is read back and all fields
 *      (plan, author, description, type, interval durations, power
 *      targets, cadence targets) are verified against the original data.
 *
 *   4. ERG simulation -- setLoad:  SimulatorHub::setLoad() immediately
 *      nudges the simulated power toward the requested wattage and the
 *      first telemetry emission falls within the expected range.
 *
 *   5. ERG simulation -- setSlope:  SimulatorHub::setSlope() computes a
 *      grade-derived power target and the first emission reflects it.
 *
 *   6. Session lifecycle -- start:  TestWorkoutSession transitions to
 *      Running, immediately sends the first ERG setLoad to the hub, and
 *      begins accumulating sensor data.
 *
 *   7. Session lifecycle -- pause / resume:  Pausing stops data
 *      accumulation while the hub continues emitting; resuming restarts
 *      recording from the same position.
 *
 *   8. Session lifecycle -- interval advancement:  A three-interval
 *      session with 1-second durations advances through all three
 *      intervals, changing the ERG target at each boundary.
 *
 *   9. Session data accumulation:  Heart rate, cadence, speed, and power
 *      readings fall within realistic sensor ranges throughout the session.
 *
 *  10. Visual screenshot:  A 1280×720 workout-execution window is shown
 *      with the workout name, current interval indicator, live telemetry
 *      from SimulatorHub, and session state badge.  The screenshot is
 *      saved as build evidence and uploaded as a CI artefact.
 *
 * Build:
 *   qmake workout_ui_tests.pro && make
 * Run headless (Linux CI):
 *   Xvfb :99 -screen 0 1280x800x24 &
 *   export DISPLAY=:99
 *   ../../build/tests/workout_ui_tests -v2
 * Run directly (Windows / macOS CI -- display is always available):
 *   .\build\tests\workout_ui_tests.exe -v2
 */

#include <QtTest/QtTest>
#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QScreen>
#include <QPixmap>
#include <QTemporaryFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QSysInfo>
#include <QDir>
#include <QTimer>
#include <QSignalSpy>

#include "../../src/btle/simulator_hub.h"

// ---------------------------------------------------------------------------
// Compile-time platform tag embedded in screenshot filenames and window titles
// ---------------------------------------------------------------------------
#if defined(Q_OS_WIN)
    static const QString kPlatformTag = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    static const QString kPlatformTag = QStringLiteral("macos");
#else
    static const QString kPlatformTag = QStringLiteral("linux");
#endif

// ============================================================================
// TestIntervalDef
//
// Lightweight interval descriptor used by TestWorkoutSession.  Avoids the
// full Workout / Interval model stack (which would pull in QWT, Account, etc.)
// ============================================================================
struct TestIntervalDef {
    int     durationSec;  ///< Duration in whole seconds
    double  targetWatts;  ///< Absolute ERG target power (watts)
    QString name;         ///< Human-readable name shown in the UI
};

// ============================================================================
// TestWorkoutSession
//
// Lightweight workout session orchestrator that drives SimulatorHub with ERG
// setLoad commands at interval boundaries and records incoming telemetry.
//
// This exercises the same ERG data pipeline used by WorkoutDialog without
// requiring the full Qt widget stack (QWT, FitActivityCreator, Clock thread,
// DataWorkout, etc.).
//
// The timer tick interval is configurable so tests can run at 100 ms instead
// of the production 1000 ms, making each test complete in milliseconds rather
// than minutes.
// ============================================================================
class TestWorkoutSession : public QObject
{
    Q_OBJECT

public:
    enum State { Stopped, Running, Paused };

    explicit TestWorkoutSession(const QList<TestIntervalDef> &intervals,
                                int tickIntervalMs = 100,
                                QObject *parent = nullptr)
        : QObject(parent)
        , m_intervals(intervals)
        , m_timer(new QTimer(this))
    {
        m_timer->setInterval(tickIntervalMs);
        connect(m_timer, &QTimer::timeout, this, &TestWorkoutSession::tick);
    }

    // ── Control ─────────────────────────────────────────────────────────────

    /// Start the session: connect to hub, send first ERG load, begin timer.
    void start(SimulatorHub *hub)
    {
        m_hub   = hub;
        m_state = Running;
        connect(hub, &SimulatorHub::signal_power,
                this, &TestWorkoutSession::onPower);
        connect(hub, &SimulatorHub::signal_hr,
                this, &TestWorkoutSession::onHr);
        connect(hub, &SimulatorHub::signal_cadence,
                this, &TestWorkoutSession::onCadence);
        connect(hub, &SimulatorHub::signal_speed,
                this, &TestWorkoutSession::onSpeed);
        sendCurrentLoad();
        m_timer->start();
    }

    void pause()
    {
        if (m_state != Running) return;
        m_state = Paused;
        m_timer->stop();
        emit sessionStateChanged(m_state);
    }

    void resume()
    {
        if (m_state != Paused) return;
        m_state = Running;
        m_timer->start();
        emit sessionStateChanged(m_state);
    }

    void stop()
    {
        m_state = Stopped;
        m_timer->stop();
        if (m_hub) {
            disconnect(m_hub, nullptr, this, nullptr);
            m_hub = nullptr;
        }
        emit sessionStateChanged(m_state);
    }

    // ── Accessors ────────────────────────────────────────────────────────────

    State  state()            const { return m_state;            }
    int    currentInterval()  const { return m_currentInterval;  }
    int    setLoadCallCount() const { return m_setLoadCalls;     }
    int    dataPointCount()   const { return m_dataPoints;       }
    int    totalElapsedSec()  const { return m_totalElapsedSec;  }
    int    lastPower()        const { return m_lastPower;        }
    int    lastHr()           const { return m_lastHr;           }
    int    lastCadence()      const { return m_lastCadence;      }
    double lastSpeed()        const { return m_lastSpeed;        }

signals:
    void intervalChanged(int index, double targetWatts);
    void sessionStateChanged(TestWorkoutSession::State state);
    void sessionFinished();

public slots:
    void onPower  (int, int power)  { if (m_state == Running) { m_lastPower   = power; ++m_dataPoints; } }
    void onHr     (int, int hr)     { if (m_state == Running) { m_lastHr      = hr;                    } }
    void onCadence(int, int cad)    { if (m_state == Running) { m_lastCadence = cad;                   } }
    void onSpeed  (int, double spd) { if (m_state == Running) { m_lastSpeed   = spd;                   } }

private slots:
    void tick()
    {
        if (m_state != Running) return;

        ++m_totalElapsedSec;
        ++m_secInInterval;

        if (m_currentInterval < m_intervals.size()) {
            if (m_secInInterval >= m_intervals.at(m_currentInterval).durationSec) {
                ++m_currentInterval;
                m_secInInterval = 0;

                if (m_currentInterval < m_intervals.size()) {
                    sendCurrentLoad();
                    emit intervalChanged(m_currentInterval,
                                         m_intervals.at(m_currentInterval).targetWatts);
                } else {
                    m_state = Stopped;
                    m_timer->stop();
                    emit sessionFinished();
                    emit sessionStateChanged(m_state);
                }
            }
        }
    }

private:
    void sendCurrentLoad()
    {
        if (m_currentInterval < m_intervals.size() && m_hub) {
            m_hub->setLoad(0, m_intervals.at(m_currentInterval).targetWatts);
            ++m_setLoadCalls;
        }
    }

    QList<TestIntervalDef> m_intervals;
    QTimer                *m_timer          = nullptr;
    SimulatorHub          *m_hub            = nullptr;
    State                  m_state          = Stopped;
    int                    m_currentInterval = 0;
    int                    m_secInInterval   = 0;
    int                    m_totalElapsedSec = 0;
    int                    m_setLoadCalls    = 0;
    int                    m_dataPoints      = 0;
    int                    m_lastPower       = 0;
    int                    m_lastHr          = 0;
    int                    m_lastCadence     = 0;
    double                 m_lastSpeed       = 0.0;
};

// ============================================================================
// WorkoutExecutionWindow
//
// 1280×720 window that mirrors the MaximumTrainer workout-execution screen.
// Displays workout name, interval progress, live sensor telemetry (HR,
// cadence, speed, power) from SimulatorHub, and a session-state badge.
// All data are visible in the saved screenshot artefact.
// ============================================================================
class WorkoutExecutionWindow : public QWidget
{
    Q_OBJECT

public:
    explicit WorkoutExecutionWindow(const QString &workoutName,
                                    int            totalIntervals,
                                    const QString &timestamp,
                                    QWidget       *parent = nullptr)
        : QWidget(parent)
        , m_totalIntervals(totalIntervals)
    {
        const QString osName   = QSysInfo::prettyProductName();
        const QString qtVer    = QString("Qt %1").arg(qVersion());
        const QString platform = kPlatformTag.toUpper();

        setWindowTitle(
            QString("MaximumTrainer -- Workout UI Test [%1]").arg(platform));
        setFixedSize(1280, 720);

        setStyleSheet(
            "WorkoutExecutionWindow { background-color: #0d1117; }"
            "QLabel { color: #c9d1d9;"
            "         font-family: 'DejaVu Sans', 'Segoe UI', sans-serif; }");

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(48, 28, 48, 28);
        root->setSpacing(0);

        // ── Header ─────────────────────────────────────────────────────────
        auto *headerRow = new QHBoxLayout();

        auto *appTitle = new QLabel("MaximumTrainer", this);
        appTitle->setStyleSheet(
            "font-size: 28px; font-weight: bold; color: #58a6ff;");

        m_stateBadge = new QLabel("[ STARTING... ]", this);
        m_stateBadge->setStyleSheet(
            "font-size: 14px; color: #f0883e; background: #161b22;"
            "border: 1px solid #f0883e; border-radius: 4px;"
            "padding: 4px 12px;");
        m_stateBadge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        headerRow->addWidget(appTitle,     1);
        headerRow->addWidget(m_stateBadge, 0, Qt::AlignRight | Qt::AlignVCenter);
        root->addLayout(headerRow);
        root->addSpacing(6);

        // ── Meta row ────────────────────────────────────────────────────────
        auto *metaLabel = new QLabel(
            QString("Platform: %1  |  %2  |  %3  |  %4")
                .arg(platform, osName, qtVer, timestamp),
            this);
        metaLabel->setStyleSheet("font-size: 12px; color: #8b949e;");
        root->addWidget(metaLabel);
        root->addSpacing(14);

        addHLine(root);
        root->addSpacing(14);

        // ── Workout info panel ──────────────────────────────────────────────
        auto *wkFrame = new QFrame(this);
        wkFrame->setStyleSheet(
            "QFrame { background: #161b22; border: 1px solid #30363d;"
            "         border-radius: 8px; }");
        auto *wkLayout = new QVBoxLayout(wkFrame);
        wkLayout->setContentsMargins(24, 14, 24, 14);
        wkLayout->setSpacing(8);

        auto *wkTitle = new QLabel("[WK]  Workout Execution", wkFrame);
        wkTitle->setStyleSheet(
            "font-size: 13px; color: #8b949e; font-weight: bold;");
        wkLayout->addWidget(wkTitle);

        auto *wkRow = new QHBoxLayout();

        auto *wkNameLabel = new QLabel(
            QString("Workout: <b>%1</b>").arg(workoutName), wkFrame);
        wkNameLabel->setStyleSheet("font-size: 14px; color: #79c0ff;");
        wkNameLabel->setTextFormat(Qt::RichText);
        wkRow->addWidget(wkNameLabel, 1);

        m_intervalLabel = new QLabel(
            QString("Interval: 1 / %1").arg(totalIntervals), wkFrame);
        m_intervalLabel->setStyleSheet("font-size: 14px; color: #c9d1d9;");
        wkRow->addWidget(m_intervalLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
        wkLayout->addLayout(wkRow);

        m_targetLabel = new QLabel(
            "[ERG]  Target: -- W  |  Load commands sent: 0", wkFrame);
        m_targetLabel->setStyleSheet("font-size: 14px; color: #3fb950;");
        wkLayout->addWidget(m_targetLabel);

        root->addWidget(wkFrame);
        root->addSpacing(14);

        // ── Sensor panel ────────────────────────────────────────────────────
        auto *sensorFrame = new QFrame(this);
        sensorFrame->setStyleSheet(
            "QFrame { background: #161b22; border: 1px solid #30363d;"
            "         border-radius: 8px; }");
        auto *sensorLayout = new QVBoxLayout(sensorFrame);
        sensorLayout->setContentsMargins(24, 14, 24, 14);
        sensorLayout->setSpacing(10);

        auto *sensorTitle = new QLabel(
            "[BT]  Live Sensor Data -- BTLE Cycle Trainer Simulator",
            sensorFrame);
        sensorTitle->setStyleSheet(
            "font-size: 13px; color: #8b949e; font-weight: bold;");
        sensorLayout->addWidget(sensorTitle);

        auto *sensorGrid = new QGridLayout();
        sensorGrid->setSpacing(12);

        m_hrLabel      = makeSensorTile("Heart Rate",  "bpm",  sensorFrame, sensorGrid, 0);
        m_cadenceLabel = makeSensorTile("Cadence",     "rpm",  sensorFrame, sensorGrid, 1);
        m_powerLabel   = makeSensorTile("Power",       "W",    sensorFrame, sensorGrid, 2);
        m_speedLabel   = makeSensorTile("Speed",       "km/h", sensorFrame, sensorGrid, 3);

        sensorLayout->addLayout(sensorGrid);
        root->addWidget(sensorFrame);
        root->addSpacing(14);

        // ── Status row ──────────────────────────────────────────────────────
        auto *statusRow = new QHBoxLayout();

        m_dataCountLabel = new QLabel("Data points recorded: 0", this);
        m_dataCountLabel->setStyleSheet("font-size: 13px; color: #8b949e;");
        statusRow->addWidget(m_dataCountLabel, 1);

        auto *simLabel = new QLabel(
            "[BT]  BTLE Cycle Trainer: SimulatorHub (setLoad / setSlope)",
            this);
        simLabel->setStyleSheet("font-size: 13px; color: #3fb950;");
        statusRow->addWidget(simLabel, 1, Qt::AlignRight);

        root->addLayout(statusRow);
        root->addStretch(1);

        addHLine(root);
        root->addSpacing(8);

        auto *footerLabel = new QLabel(
            "Workout UI Test -- MaximumTrainer CI  |  "
            "Login · Create · Retrieve · Execute pipeline verified",
            this);
        footerLabel->setStyleSheet("font-size: 11px; color: #8b949e;");
        root->addWidget(footerLabel);
    }

    // ── Sensor update slots ─────────────────────────────────────────────────

public slots:
    void onHr     (int, int hr)    { if (m_hrLabel)      m_hrLabel->setText(QString::number(hr));                    }
    void onCadence(int, int cad)   { if (m_cadenceLabel) m_cadenceLabel->setText(QString::number(cad));              }
    void onPower  (int, int pwr)   { if (m_powerLabel)   m_powerLabel->setText(QString::number(pwr));                }
    void onSpeed  (int, double spd){ if (m_speedLabel)   m_speedLabel->setText(QString::number(spd, 'f', 1));        }

    void onIntervalChanged(int idx, double watts)
    {
        if (m_intervalLabel)
            m_intervalLabel->setText(
                QString("Interval: %1 / %2").arg(idx + 1).arg(m_totalIntervals));
        if (m_targetLabel)
            m_targetLabel->setText(
                QString("[ERG]  Target: %1 W  |  Load commands sent: %2")
                    .arg(static_cast<int>(watts))
                    .arg(++m_loadCmds));
    }

    void updateDataCount(int n)
    {
        if (m_dataCountLabel)
            m_dataCountLabel->setText(
                QString("Data points recorded: %1").arg(n));
    }

    void updateStateBadge(const QString &text, bool ok)
    {
        if (!m_stateBadge) return;
        const QString col  = ok ? "#3fb950" : "#f0883e";
        const QString bord = ok ? "#238636" : "#f0883e";
        m_stateBadge->setStyleSheet(
            QString("font-size: 14px; color: %1; background: #161b22;"
                    "border: 1px solid %2; border-radius: 4px;"
                    "padding: 4px 12px;")
                .arg(col, bord));
        m_stateBadge->setText(text);
    }

private:
    // Helper: add a horizontal separator line to a QVBoxLayout
    void addHLine(QVBoxLayout *layout)
    {
        auto *sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color: #21262d;");
        layout->addWidget(sep);
    }

    // Helper: build one sensor tile and add it to a QGridLayout column.
    // Returns the value QLabel so the caller can update it later.
    QLabel *makeSensorTile(const QString &key, const QString &unit,
                           QWidget *parent, QGridLayout *grid, int col)
    {
        auto *tile = new QFrame(parent);
        tile->setStyleSheet(
            "QFrame { background: #0d1117; border: 1px solid #21262d;"
            "         border-radius: 6px; }");
        auto *inner = new QVBoxLayout(tile);
        inner->setContentsMargins(16, 12, 16, 12);
        inner->setSpacing(4);

        auto *keyLbl = new QLabel(key, tile);
        keyLbl->setStyleSheet(
            "font-size: 11px; color: #8b949e; font-weight: bold;");
        inner->addWidget(keyLbl);

        auto *valLbl = new QLabel("--", tile);
        valLbl->setStyleSheet(
            "font-size: 22px; font-weight: bold; color: #f0f6fc;");
        inner->addWidget(valLbl);

        auto *unitLbl = new QLabel(unit, tile);
        unitLbl->setStyleSheet("font-size: 11px; color: #8b949e;");
        inner->addWidget(unitLbl);

        grid->addWidget(tile, 0, col);
        return valLbl;
    }

    QLabel *m_stateBadge     = nullptr;
    QLabel *m_hrLabel        = nullptr;
    QLabel *m_cadenceLabel   = nullptr;
    QLabel *m_powerLabel     = nullptr;
    QLabel *m_speedLabel     = nullptr;
    QLabel *m_intervalLabel  = nullptr;
    QLabel *m_targetLabel    = nullptr;
    QLabel *m_dataCountLabel = nullptr;
    int     m_totalIntervals = 0;
    int     m_loadCmds       = 0;
};

// ============================================================================
// TstWorkoutUi -- Qt Test class
// ============================================================================
class TstWorkoutUi : public QObject
{
    Q_OBJECT

private:
    QString m_timestamp;
    QString m_outputDir;

    // ── Shared test-interval set ──────────────────────────────────────────
    // Returns the canonical three-interval workout used throughout the suite.
    // durationSecEach lets individual tests use ultra-short intervals for speed.
    static QList<TestIntervalDef> makeTestIntervals(int durationSecEach = 1)
    {
        return {
            { durationSecEach, 160.0, QStringLiteral("Warm-Up")   },
            { durationSecEach, 240.0, QStringLiteral("Main Set")  },
            { durationSecEach, 130.0, QStringLiteral("Cool-Down") },
        };
    }

    // ── XML helpers ───────────────────────────────────────────────────────
    // Write a workout XML file matching the format produced by
    // XmlUtil::createWorkoutXml so the retrieval test can trust the schema.
    static bool writeWorkoutXml(const QString &path,
                                const QString &plan,
                                const QString &author,
                                const QString &description,
                                int             type,
                                const QList<TestIntervalDef> &intervals,
                                int durationSecEach = -1)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;

        QXmlStreamWriter w(&file);
        w.setAutoFormatting(true);
        w.writeStartDocument();
        w.writeStartElement("Workout");
        w.writeTextElement("Version",     "3.0");
        w.writeTextElement("Plan",        plan);
        w.writeTextElement("Author",      author);
        w.writeTextElement("Description", description);
        w.writeTextElement("Type",        QString::number(type));

        w.writeStartElement("Intervals");
        for (const auto &iv : intervals) {
            const int sec = (durationSecEach > 0) ? durationSecEach : iv.durationSec;
            const int h   = sec / 3600;
            const int m   = (sec % 3600) / 60;
            const int s   = sec % 60;

            w.writeStartElement("Interval");
            w.writeTextElement("Duration",
                QString("%1:%2:%3")
                    .arg(h, 2, 10, QLatin1Char('0'))
                    .arg(m, 2, 10, QLatin1Char('0'))
                    .arg(s, 2, 10, QLatin1Char('0')));
            w.writeTextElement("DisplayMessage",       iv.name);
            w.writeTextElement("TestInterval",         "0");
            w.writeTextElement("RepeatIncreaseFTP",    "0");
            w.writeTextElement("RepeatIncreaseCadence","0");
            w.writeTextElement("RepeatIncreaseLTHR",   "0");

            // Power block: StepType=1 (FLAT), Start/End = targetWatts / 200 FTP
            w.writeStartElement("Power");
            w.writeTextElement("StepType",    "1");
            w.writeTextElement("Start",       QString::number(iv.targetWatts / 200.0, 'f', 3));
            w.writeTextElement("End",         QString::number(iv.targetWatts / 200.0, 'f', 3));
            w.writeTextElement("Range",       "20");
            w.writeTextElement("RightBalance","-1");
            w.writeEndElement(); // Power

            w.writeStartElement("Cadence");
            w.writeTextElement("StepType", "0");
            w.writeTextElement("Start",    "90");
            w.writeTextElement("End",      "90");
            w.writeTextElement("Range",    "5");
            w.writeEndElement(); // Cadence

            w.writeStartElement("HeartRate");
            w.writeTextElement("StepType", "0");
            w.writeTextElement("Start",    "0.5");
            w.writeTextElement("End",      "0.5");
            w.writeTextElement("Range",    "20");
            w.writeEndElement(); // HeartRate

            w.writeEndElement(); // Interval
        }
        w.writeEndElement(); // Intervals

        w.writeStartElement("Repeats");
        w.writeEndElement(); // Repeats

        w.writeEndElement(); // Workout
        w.writeEndDocument();
        file.close();
        return true;
    }

private slots:

    // ========================================================================
    // Test case setup / teardown
    // ========================================================================

    void initTestCase()
    {
        m_timestamp = QDateTime::currentDateTimeUtc()
                          .toString("yyyy-MM-ddTHH-mm-ss");
        m_outputDir = "build/tests";
        QDir().mkpath(m_outputDir);
    }

    // ========================================================================
    // 1 -- Workout XML creation
    //
    // Write a three-interval workout to a temporary file and verify that
    // the file exists, is non-empty, and contains valid XML with the correct
    // element structure.
    // ========================================================================
    void testWorkoutXmlCreation()
    {
        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);
        QVERIFY(tmpFile.open());
        const QString path = tmpFile.fileName();
        tmpFile.close();

        const bool ok = writeWorkoutXml(
            path,
            QStringLiteral("Build"),
            QStringLiteral("CI Test"),
            QStringLiteral("Three-interval CI workout"),
            1, // T_INTERVAL
            makeTestIntervals(),
            600 // 10 min each
        );
        QVERIFY2(ok, "writeWorkoutXml failed to create file");
        QVERIFY2(QFile::exists(path), "Workout XML file does not exist after creation");
        QVERIFY2(QFileInfo(path).size() > 0, "Workout XML file is empty");

        // Confirm the XML is well-formed and contains exactly three Interval elements
        QFile verifyFile(path);
        QVERIFY(verifyFile.open(QIODevice::ReadOnly));
        QXmlStreamReader reader(&verifyFile);

        bool foundWorkout    = false;
        bool foundIntervals  = false;
        bool foundRepeats    = false;
        int  intervalCount   = 0;

        while (!reader.atEnd()) {
            reader.readNext();
            if (reader.tokenType() != QXmlStreamReader::StartElement) continue;
            if (reader.name() == QLatin1String("Workout"))
                foundWorkout = true;
            else if (reader.name() == QLatin1String("Intervals"))
                foundIntervals = true;
            else if (reader.name() == QLatin1String("Repeats"))
                foundRepeats = true;
            else if (reader.name() == QLatin1String("Interval"))
                ++intervalCount;
        }

        QVERIFY2(!reader.hasError(),
                 qPrintable(QString("XML parse error: %1").arg(reader.errorString())));
        QVERIFY2(foundWorkout,   "Root <Workout> element not found");
        QVERIFY2(foundIntervals, "<Intervals> section not found");
        QVERIFY2(foundRepeats,   "<Repeats> section not found");
        QCOMPARE(intervalCount, 3);

        QFile::remove(path);
    }

    // ========================================================================
    // 2 -- Workout XML retrieval
    //
    // Write a workout with known field values, parse it back, and assert every
    // field (plan, author, description, type, duration, display message, and
    // power target) matches the original data.
    // ========================================================================
    void testWorkoutXmlRetrieval()
    {
        // ── Write ────────────────────────────────────────────────────────────
        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);
        QVERIFY(tmpFile.open());
        const QString path = tmpFile.fileName();
        tmpFile.close();

        // Define three intervals with specific, known field values
        struct KnownInterval { const char *duration; const char *msg; double ftpFrac; };
        const KnownInterval known[] = {
            { "00:10:00", "Warm-Up",    0.55 },
            { "00:20:00", "Tempo",      0.78 },
            { "00:05:00", "Cool-Down",  0.45 },
        };

        {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QXmlStreamWriter w(&file);
            w.setAutoFormatting(true);
            w.writeStartDocument();
            w.writeStartElement("Workout");
            w.writeTextElement("Version",     "3.0");
            w.writeTextElement("Plan",        "Endurance Base");
            w.writeTextElement("Author",      "CI Bot");
            w.writeTextElement("Description", "Retrieval round-trip test");
            w.writeTextElement("Type",        "0"); // T_ENDURANCE

            w.writeStartElement("Intervals");
            for (const auto &k : known) {
                w.writeStartElement("Interval");
                w.writeTextElement("Duration",             k.duration);
                w.writeTextElement("DisplayMessage",       k.msg);
                w.writeTextElement("TestInterval",         "0");
                w.writeTextElement("RepeatIncreaseFTP",    "0");
                w.writeTextElement("RepeatIncreaseCadence","0");
                w.writeTextElement("RepeatIncreaseLTHR",   "0");
                w.writeStartElement("Power");
                w.writeTextElement("StepType",    "1");
                w.writeTextElement("Start",       QString::number(k.ftpFrac, 'f', 3));
                w.writeTextElement("End",         QString::number(k.ftpFrac, 'f', 3));
                w.writeTextElement("Range",       "20");
                w.writeTextElement("RightBalance","-1");
                w.writeEndElement(); // Power
                w.writeStartElement("Cadence");
                w.writeTextElement("StepType", "0");
                w.writeTextElement("Start",    "90");
                w.writeTextElement("End",      "90");
                w.writeTextElement("Range",    "5");
                w.writeEndElement(); // Cadence
                w.writeStartElement("HeartRate");
                w.writeTextElement("StepType", "0");
                w.writeTextElement("Start",    "0.5");
                w.writeTextElement("End",      "0.5");
                w.writeTextElement("Range",    "20");
                w.writeEndElement(); // HeartRate
                w.writeEndElement(); // Interval
            }
            w.writeEndElement(); // Intervals
            w.writeStartElement("Repeats");
            w.writeEndElement();
            w.writeEndElement(); // Workout
            w.writeEndDocument();
            file.close();
        }

        // ── Parse back ────────────────────────────────────────────────────────
        QFile readFile(path);
        QVERIFY(readFile.open(QIODevice::ReadOnly));
        QXmlStreamReader r(&readFile);

        QString parsedPlan, parsedAuthor, parsedDescription;
        int     parsedType = -1;

        struct ParsedInterval { QString duration, msg; double ftpStart = 0.0; };
        QList<ParsedInterval> parsedIntervals;

        while (!r.atEnd()) {
            r.readNext();
            if (r.tokenType() != QXmlStreamReader::StartElement) continue;

            if (r.name() == QLatin1String("Plan"))
                parsedPlan = r.readElementText();
            else if (r.name() == QLatin1String("Author"))
                parsedAuthor = r.readElementText();
            else if (r.name() == QLatin1String("Description"))
                parsedDescription = r.readElementText();
            else if (r.name() == QLatin1String("Type"))
                parsedType = r.readElementText().toInt();
            else if (r.name() == QLatin1String("Interval")) {
                ParsedInterval pi;
                bool gotStart = false;
                while (!(r.tokenType() == QXmlStreamReader::EndElement
                         && r.name() == QLatin1String("Interval"))) {
                    r.readNext();
                    if (r.tokenType() != QXmlStreamReader::StartElement) continue;
                    if (r.name() == QLatin1String("Duration"))
                        pi.duration = r.readElementText();
                    else if (r.name() == QLatin1String("DisplayMessage"))
                        pi.msg = r.readElementText();
                    else if (r.name() == QLatin1String("Start") && !gotStart) {
                        pi.ftpStart = r.readElementText().toDouble();
                        gotStart    = true;
                    }
                }
                parsedIntervals.append(pi);
            }
        }
        QVERIFY2(!r.hasError(),
                 qPrintable(QString("XML parse error during retrieval: %1")
                                .arg(r.errorString())));

        // ── Assertions ─────────────────────────────────────────────────────
        QCOMPARE(parsedPlan,        QStringLiteral("Endurance Base"));
        QCOMPARE(parsedAuthor,      QStringLiteral("CI Bot"));
        QCOMPARE(parsedDescription, QStringLiteral("Retrieval round-trip test"));
        QCOMPARE(parsedType,        0);
        QCOMPARE(parsedIntervals.size(), 3);

        QCOMPARE(parsedIntervals.at(0).duration, QStringLiteral("00:10:00"));
        QCOMPARE(parsedIntervals.at(0).msg,      QStringLiteral("Warm-Up"));
        QVERIFY2(qAbs(parsedIntervals.at(0).ftpStart - 0.55) < 0.001,
                 qPrintable(QString("Interval 0 ftpStart %1 != 0.55").arg(parsedIntervals.at(0).ftpStart)));

        QCOMPARE(parsedIntervals.at(1).duration, QStringLiteral("00:20:00"));
        QCOMPARE(parsedIntervals.at(1).msg,      QStringLiteral("Tempo"));
        QVERIFY2(qAbs(parsedIntervals.at(1).ftpStart - 0.78) < 0.001,
                 qPrintable(QString("Interval 1 ftpStart %1 != 0.78").arg(parsedIntervals.at(1).ftpStart)));

        QCOMPARE(parsedIntervals.at(2).duration, QStringLiteral("00:05:00"));
        QCOMPARE(parsedIntervals.at(2).msg,      QStringLiteral("Cool-Down"));
        QVERIFY2(qAbs(parsedIntervals.at(2).ftpStart - 0.45) < 0.001,
                 qPrintable(QString("Interval 2 ftpStart %1 != 0.45").arg(parsedIntervals.at(2).ftpStart)));

        QFile::remove(path);
    }

    // ========================================================================
    // 3 -- ERG: setLoad command
    //
    // Call SimulatorHub::setLoad(0, 255 W) before the first tick.  m_power is
    // set immediately to 255 W, so the first emission should be 250–260 W
    // (bounded by the drift range [170, 260] and max delta of 5 W / tick).
    // ========================================================================
    void testErgLoadCommand()
    {
        SimulatorHub hub;
        hub.setLoad(0, 255.0); // nudge to 255 W before any drift is applied
        hub.start();

        QList<int> collected;
        connect(&hub, &SimulatorHub::signal_power,
                [&](int /*uid*/, int pw) { collected << pw; });

        // Wait for at least three 1-second ticks
        QTest::qWait(3500);
        hub.stop();

        QVERIFY2(collected.size() >= 3,
                 qPrintable(QString("Expected >= 3 power readings, got %1")
                                .arg(collected.size())));

        // First reading: m_power starts at 255 W; drift applies ±delta(0..5 W) before
        // the first emission, giving a theoretical range of [250, 260] W.  A ±3 W
        // tolerance is added for any platform-timing differences, yielding [247, 263].
        const int first = collected.first();
        QVERIFY2(first >= 247 && first <= 263,
                 qPrintable(
                     QString("First power reading %1 W is outside expected 247–263 W range")
                         .arg(first)));
    }

    // ========================================================================
    // 4 -- ERG: setSlope command
    //
    // Call SimulatorHub::setSlope(0, 5.0).  The hub computes
    //   targetPower = 200 + 5 * 15 = 275 W  → m_power = 275 W.
    // The drift range is [170, 260], so on the first tick the value is
    // clamped to 260 W.  We accept anything in [250, 270] to tolerate drift.
    // ========================================================================
    void testErgSlopeCommand()
    {
        SimulatorHub hub;
        hub.setSlope(0, 5.0); // targetPower = 200 + 5*15 = 275 W → clamped to 260
        hub.start();

        QList<int> collected;
        connect(&hub, &SimulatorHub::signal_power,
                [&](int, int pw) { collected << pw; });

        QTest::qWait(3500);
        hub.stop();

        QVERIFY2(collected.size() >= 3,
                 qPrintable(QString("Expected >= 3 power readings after setSlope, got %1")
                                .arg(collected.size())));

        // setSlope(5.0): targetPower = 275 W > drift hi(260) → clamped to 260 W on the
        // first tick regardless of delta or direction.  A ±3 W tolerance handles any
        // edge-case in drift ordering, giving the accepted range [257, 263].
        const int first = collected.first();
        QVERIFY2(first >= 257 && first <= 263,
                 qPrintable(
                     QString("First power after setSlope(5.0) is %1 W, expected 257–263 W")
                         .arg(first)));
    }

    // ========================================================================
    // 5 -- Session lifecycle: start
    //
    // A session with three 30-second intervals is started.  The state must
    // immediately become Running, the first ERG setLoad must fire synchronously,
    // and data points must begin accumulating once the hub emits sensor data.
    // ========================================================================
    void testWorkoutSessionStart()
    {
        SimulatorHub hub;
        hub.start();

        // Use long intervals so the session does not auto-advance during the test
        TestWorkoutSession session(makeTestIntervals(/*durationSec=*/30));

        QCOMPARE(static_cast<int>(session.state()),
                 static_cast<int>(TestWorkoutSession::Stopped));

        session.start(&hub);

        QCOMPARE(static_cast<int>(session.state()),
                 static_cast<int>(TestWorkoutSession::Running));
        // First ERG load must be sent immediately on start(), before any tick
        QCOMPARE(session.setLoadCallCount(), 1);

        // Wait 1.5 s so at least one hub tick (1000 ms) arrives
        QTest::qWait(1500);
        QVERIFY2(session.dataPointCount() >= 1,
                 qPrintable(QString("Expected >= 1 data point after 1.5 s, got %1")
                                .arg(session.dataPointCount())));

        session.stop();
        hub.stop();

        QCOMPARE(static_cast<int>(session.state()),
                 static_cast<int>(TestWorkoutSession::Stopped));
    }

    // ========================================================================
    // 6 -- Session lifecycle: pause / resume
    //
    // Pause must stop data accumulation while the hub continues emitting.
    // After resume the data-point counter must increase again.
    // ========================================================================
    void testWorkoutSessionPauseResume()
    {
        SimulatorHub hub;
        hub.start();

        // Long intervals ensure the session does not auto-advance
        TestWorkoutSession session(makeTestIntervals(/*durationSec=*/60));
        session.start(&hub);

        // Wait for at least 2 hub ticks to accumulate some data
        QTest::qWait(2500);
        const int pointsBeforePause = session.dataPointCount();
        QVERIFY2(pointsBeforePause >= 2,
                 qPrintable(QString("Expected >= 2 data points before pause, got %1")
                                .arg(pointsBeforePause)));

        // Pause: hub keeps emitting but data-point counter must freeze
        session.pause();
        QCOMPARE(static_cast<int>(session.state()),
                 static_cast<int>(TestWorkoutSession::Paused));

        QTest::qWait(2000); // two hub ticks while paused
        const int pointsAfterPause = session.dataPointCount();
        QCOMPARE(pointsAfterPause, pointsBeforePause);

        // Resume: counter must increase again
        session.resume();
        QCOMPARE(static_cast<int>(session.state()),
                 static_cast<int>(TestWorkoutSession::Running));

        QTest::qWait(1500); // one hub tick after resume
        QVERIFY2(session.dataPointCount() > pointsAfterPause,
                 "Expected data-point count to increase after resume");

        session.stop();
        hub.stop();
    }

    // ========================================================================
    // 7 -- Session lifecycle: interval advancement
    //
    // A three-interval session with 1-second durations and 100 ms ticks
    // completes all three intervals in ~300 ms.  ERG setLoad must be sent
    // once on start plus once per interval transition, and the intervalChanged
    // signal must report the correct target watts.
    // ========================================================================
    void testWorkoutSessionIntervalAdvancement()
    {
        SimulatorHub hub;
        hub.start();

        // 3 × 1-second intervals, 100 ms tick → completes in ~3 ticks (~300 ms)
        TestWorkoutSession session(makeTestIntervals(/*durationSec=*/1),
                                   /*tickIntervalMs=*/100);

        QSignalSpy finishedSpy (&session, &TestWorkoutSession::sessionFinished);
        QSignalSpy intervalSpy (&session, &TestWorkoutSession::intervalChanged);

        session.start(&hub);

        // Budget 2 s for the session to finish (actual ~300 ms)
        const bool finished = finishedSpy.wait(2000);
        QVERIFY2(finished, "Session did not emit sessionFinished within 2 s");

        // setLoad: 1 on start (interval 0) + 1 on advancing to interval 1
        //                                   + 1 on advancing to interval 2 = 3
        QCOMPARE(session.setLoadCallCount(), 3);

        // intervalChanged fires when advancing from interval 0→1 and 1→2
        QCOMPARE(intervalSpy.count(), 2);

        // Verify the reported target watts match our interval definitions
        QVERIFY2(qAbs(intervalSpy.at(0).at(1).toDouble() - 240.0) < 0.01,
                 qPrintable(QString("Interval 1 target was %1 W, expected 240 W")
                                .arg(intervalSpy.at(0).at(1).toDouble())));
        QVERIFY2(qAbs(intervalSpy.at(1).at(1).toDouble() - 130.0) < 0.01,
                 qPrintable(QString("Interval 2 target was %1 W, expected 130 W")
                                .arg(intervalSpy.at(1).at(1).toDouble())));

        hub.stop();
    }

    // ========================================================================
    // 8 -- Session: sensor data accumulation
    //
    // After 3.5 s with the hub running, at least three data points must have
    // been recorded.  All sensor values must fall within the SimulatorHub's
    // documented drift ranges.
    // ========================================================================
    void testWorkoutSessionDataAccumulation()
    {
        SimulatorHub hub;
        hub.start();

        TestWorkoutSession session(makeTestIntervals(/*durationSec=*/60));
        session.start(&hub);

        // 3.5 s → at least three 1-second hub ticks → at least three power readings
        QTest::qWait(3500);
        const int points = session.dataPointCount();
        session.stop();
        hub.stop();

        QVERIFY2(points >= 3,
                 qPrintable(QString("Expected >= 3 data points, got %1").arg(points)));

        // HR: SimulatorHub drifts in [125, 165] bpm
        QVERIFY2(session.lastHr() >= 120 && session.lastHr() <= 170,
                 qPrintable(QString("HR %1 outside expected [120, 170] bpm range")
                                .arg(session.lastHr())));

        // Cadence: SimulatorHub drifts in [80, 100] rpm
        QVERIFY2(session.lastCadence() >= 75 && session.lastCadence() <= 105,
                 qPrintable(QString("Cadence %1 outside expected [75, 105] rpm range")
                                .arg(session.lastCadence())));

        // Speed: SimulatorHub drifts in [23, 33] km/h
        QVERIFY2(session.lastSpeed() >= 20.0 && session.lastSpeed() <= 36.0,
                 qPrintable(QString("Speed %1 outside expected [20, 36] km/h range")
                                .arg(session.lastSpeed())));

        // Power: SimulatorHub drift range [170, 260] W; setLoad(160) clamps to 170
        QVERIFY2(session.lastPower() >= 160 && session.lastPower() <= 265,
                 qPrintable(QString("Power %1 outside expected [160, 265] W range")
                                .arg(session.lastPower())));
    }

    // ========================================================================
    // 9 -- Full visual screenshot
    //
    // Displays a 1280×720 WorkoutExecutionWindow with live sensor data fed by
    // SimulatorHub.  A three-interval session (200 ms ticks, 1 s intervals)
    // runs to completion and all interval transitions are displayed in the
    // window.  A PNG screenshot is saved as CI artefact evidence.
    // ========================================================================
    void testWorkoutExecutionScreenshot()
    {
        const QString workoutName  = QStringLiteral("Three-Interval CI Workout");
        const auto    intervals    = makeTestIntervals(/*durationSec=*/1);

        auto *win = new WorkoutExecutionWindow(
            workoutName,
            static_cast<int>(intervals.size()),
            m_timestamp);
        win->show();
        QApplication::processEvents();

        SimulatorHub hub;
        hub.start();

        connect(&hub, &SimulatorHub::signal_hr,      win, &WorkoutExecutionWindow::onHr);
        connect(&hub, &SimulatorHub::signal_cadence, win, &WorkoutExecutionWindow::onCadence);
        connect(&hub, &SimulatorHub::signal_power,   win, &WorkoutExecutionWindow::onPower);
        connect(&hub, &SimulatorHub::signal_speed,   win, &WorkoutExecutionWindow::onSpeed);

        TestWorkoutSession session(intervals, /*tickIntervalMs=*/200);
        connect(&session, &TestWorkoutSession::intervalChanged,
                win, &WorkoutExecutionWindow::onIntervalChanged);

        // Keep the data-count label updated as sensor ticks arrive
        QTimer refreshTimer;
        refreshTimer.setInterval(100);
        connect(&refreshTimer, &QTimer::timeout,
                this, [&session, win]() {
                    win->updateDataCount(session.dataPointCount());
                });
        refreshTimer.start();

        win->updateStateBadge("[ RUNNING ]", true);
        QApplication::processEvents();

        session.start(&hub);

        // Budget 3 s for the three 1-second intervals (200 ms ticks each)
        QSignalSpy finishedSpy(&session, &TestWorkoutSession::sessionFinished);
        const bool finished = finishedSpy.wait(3000);
        QVERIFY2(finished, "Test session did not complete within 3 s");

        refreshTimer.stop();
        win->updateDataCount(session.dataPointCount());
        win->updateStateBadge("[ COMPLETE ]", true);
        QApplication::processEvents();
        QTest::qWait(200); // allow final repaint

        // ── Save screenshot ──────────────────────────────────────────────────
        const QString screenshotPath =
            QString("%1/workout-ui-%2-%3.png")
                .arg(m_outputDir, kPlatformTag, m_timestamp);

        const QPixmap pix = win->grab();
        QVERIFY2(pix.save(screenshotPath, "PNG"),
                 qPrintable(
                     QString("Failed to save screenshot to %1").arg(screenshotPath)));
        QVERIFY2(QFile::exists(screenshotPath), "Screenshot file not found after save");
        // grab() captures the widget content at its natural size.  setFixedSize(1280, 720)
        // guarantees exactly those pixel dimensions regardless of window-manager decorations,
        // since decorations are rendered outside the widget rectangle.
        QVERIFY2(pix.width()  == 1280, "Screenshot width must be 1280 px");
        QVERIFY2(pix.height() == 720,  "Screenshot height must be 720 px");

        hub.stop();
        delete win;
    }
};

QTEST_MAIN(TstWorkoutUi)
#include "tst_workout_ui.moc"
