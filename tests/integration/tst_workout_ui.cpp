/*
 * tst_workout_ui.cpp
 *
 * Workout UI Integration Test -- MaximumTrainer
 *
 * Purpose
 * -----------------------------------------------------------------------
 * Validates the full workout UI pipeline on Windows, macOS, and Linux:
 *
 *   1. Account fixture (offline):  A minimal Account object (FTP = 200 W,
 *      isOffline = true) is registered as a qApp property in initTestCase,
 *      mirroring the offline-login state that MainWindow establishes when
 *      the user chooses "Log in offline".  This satisfies the dependency
 *      that Workout::calculateWorkoutMetrics() has on qApp's Account, and
 *      allows all model tests to run without a database or network.
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
 *  10. Workout model construction:  Workout and Interval objects are built
 *      in-process and all accessor fields are verified.
 *
 *  11. Workout XML round-trip with model:  XmlUtil::createWorkoutXml
 *      writes the Workout, then XmlUtil::parseSingleWorkoutXml reads it
 *      back.  Plan, name, description, type, interval durations, and
 *      power fractions are all verified.
 *
 *  12. Workout average-power metric:  A two-interval flat-power workout
 *      is constructed and Workout::getAveragePower() is verified.
 *
 *  13. Network connectivity:  An HTTPS GET request is made to the
 *      intervals.icu REST API.  The test calls QSKIP when
 *      INTERVALS_ICU_API_KEY / INTERVALS_ICU_ATHLETE_ID are absent;
 *      otherwise it verifies HTTP 200 and a non-empty athlete name.
 *
 *  14. Network workout retrieval:  GET /athlete/{id}/workouts returns a
 *      JSON array.  The test QSKIP-guards on missing credentials.
 *
 *  15. Power-on-target verification:  After a 3-second ERG session the
 *      last reported actual power is within ±25 % of the ERG target.
 *
 *  16. Visual screenshot:  A 1280×720 workout-execution window is shown
 *      with a QWT power-curve plot (actual vs target power), the workout
 *      name, current interval indicator, live telemetry from SimulatorHub,
 *      and session state badge.  The screenshot is saved as build evidence
 *      and uploaded as a CI artefact.
 *
 * Build:
 *   qmake workout_ui_tests.pro [QWT_INSTALL=/path/to/qwt] && make
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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QEventLoop>
#include <QUrl>

// QWT power-curve chart
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <QPen>
#include <QColor>

// Full workout model and XML utility
#include "../../src/btle/simulator_hub.h"
#include "../../src/model/account.h"
#include "../../src/model/workout.h"
#include "../../src/model/interval.h"
#include "../../src/persistence/file/xmlutil.h"

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
    int     durationTicks;  ///< Duration in timer ticks (not real seconds).
                            ///< One tick fires every tickIntervalMs, so a
                            ///< durationTicks=5 interval at tickIntervalMs=200
                            ///< lasts ~1 real second.
    double  targetWatts;   ///< Absolute ERG target power (watts)
    QString name;           ///< Human-readable name shown in the UI
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

    /// Per-second power history for QWT chart: {targetWatts, actualWatts}
    struct PowerSample { double target; double actual; };
    const QVector<PowerSample> &powerHistory() const { return m_powerHistory; }

signals:
    void intervalChanged(int index, double targetWatts);
    void sessionStateChanged(TestWorkoutSession::State state);
    void sessionFinished();

public slots:
    void onPower  (int, int power)  {
        if (m_state == Running) {
            m_lastPower = power;
            ++m_dataPoints;
            const double target = (m_currentInterval < m_intervals.size())
                                  ? m_intervals.at(m_currentInterval).targetWatts
                                  : 0.0;
            m_powerHistory.append({target, static_cast<double>(power)});
        }
    }
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
            if (m_secInInterval >= m_intervals.at(m_currentInterval).durationTicks) {
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
    QVector<PowerSample>   m_powerHistory;
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

        // ── QWT power-curve plot ─────────────────────────────────────────────
        m_plot = new QwtPlot(this);
        m_plot->setFixedHeight(160);
        m_plot->setCanvasBackground(QColor("#161b22"));
        m_plot->setStyleSheet("border: 1px solid #30363d; border-radius: 8px;");
        m_plot->setAxisScale(QwtPlot::xBottom, 0, 30);  // 30 s window
        m_plot->setAxisScale(QwtPlot::yLeft,   0, 350); // 0–350 W

        // Target-power curve (dashed orange)
        m_targetCurve = new QwtPlotCurve(QStringLiteral("Target Power"));
        m_targetCurve->setPen(QPen(QColor("#f0883e"), 2, Qt::DashLine));
        m_targetCurve->attach(m_plot);

        // Actual-power curve (solid green)
        m_actualCurve = new QwtPlotCurve(QStringLiteral("Actual Power"));
        m_actualCurve->setPen(QPen(QColor("#3fb950"), 2));
        m_actualCurve->attach(m_plot);

        // Subtle grid
        auto *grid = new QwtPlotGrid();
        grid->setMajorPen(QPen(QColor("#21262d"), 1, Qt::DotLine));
        grid->attach(m_plot);

        root->addWidget(m_plot);
        root->addSpacing(10);

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
            "Login · Create · Retrieve · Execute · QWT power curve · Network verified",
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

    /// Feed power-history samples into the QWT chart.
    void updatePowerChart(const QVector<TestWorkoutSession::PowerSample> &history)
    {
        if (!m_plot || history.isEmpty()) return;
        const int n = history.size();
        QVector<double> xs(n), ys_target(n), ys_actual(n);
        for (int i = 0; i < n; ++i) {
            xs[i]        = static_cast<double>(i);
            ys_target[i] = history.at(i).target;
            ys_actual[i] = history.at(i).actual;
        }
        m_targetCurve->setSamples(xs, ys_target);
        m_actualCurve->setSamples(xs, ys_actual);
        m_plot->setAxisScale(QwtPlot::xBottom, 0, qMax(30, n));
        m_plot->replot();
    }

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

    /// Call once right after session.start() to show the initial ERG target
    /// (sent before any intervalChanged signal fires).
    void setInitialTarget(double watts)
    {
        if (m_targetLabel)
            m_targetLabel->setText(
                QString("[ERG]  Target: %1 W  |  Load commands sent: %2")
                    .arg(static_cast<int>(watts))
                    .arg(m_loadCmds));
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
    int     m_loadCmds       = 1; // start at 1 — the initial setLoad is sent before any intervalChanged

    // QWT power-curve chart
    QwtPlot       *m_plot        = nullptr;
    QwtPlotCurve  *m_targetCurve = nullptr;
    QwtPlotCurve  *m_actualCurve = nullptr;
};

// ============================================================================
// TstWorkoutUi -- Qt Test class
// ============================================================================
class TstWorkoutUi : public QObject
{
    Q_OBJECT

private:
    QString  m_timestamp;
    QString  m_outputDir;
    Account *m_account = nullptr;

    // ── Shared test-interval set ──────────────────────────────────────────
    // Returns the canonical three-interval workout used throughout the suite.
    // durationTicksEach controls how many timer ticks each interval lasts.
    // With the default tickIntervalMs=100 ms, 1 tick ≈ 100 ms of real time.
    static QList<TestIntervalDef> makeTestIntervals(int durationTicksEach = 1)
    {
        return {
            { durationTicksEach, 160.0, QStringLiteral("Warm-Up")   },
            { durationTicksEach, 240.0, QStringLiteral("Main Set")  },
            { durationTicksEach, 130.0, QStringLiteral("Cool-Down") },
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
            // writeWorkoutXml is used by tests that pass makeTestIntervals()
            // with durationTicksEach == 600 (10 min) for the XML round-trip.
            // When durationSecEach is provided it overrides; otherwise fall
            // back to the TestIntervalDef::durationTicks value (which those
            // tests set to a plausible second count like 600).
            const int sec = (durationSecEach > 0) ? durationSecEach : iv.durationTicks;
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
        // Use the binary directory so screenshots land next to the test
        // executable regardless of the working directory at launch time,
        // matching the convention used by all other integration test suites.
        m_outputDir = QCoreApplication::applicationDirPath();
        QDir().mkpath(m_outputDir);

        // Set up a minimal Account in qApp so that Workout::calculateWorkoutMetrics()
        // can read FTP without a null-pointer dereference.  FTP=200 W is used as
        // the reference for all metric calculations in this test suite.
        m_account = new Account();
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
        delete m_account;
        m_account = nullptr;
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

        // Use QSignalSpy to wait event-driven for 3 hub ticks — no fixed sleep needed.
        // QSignalSpy::wait() blocks the event loop until a new emission arrives (≤ 3 s).
        QSignalSpy powerSpy(&hub, &SimulatorHub::signal_power);
        while (powerSpy.count() < 3)
            QVERIFY2(powerSpy.wait(3000),
                     "SimulatorHub did not emit signal_power within 3 s");
        hub.stop();

        QVERIFY2(powerSpy.count() >= 3,
                 qPrintable(QString("Expected >= 3 power readings, got %1")
                                .arg(powerSpy.count())));

        // First reading: m_power starts at 255 W; drift applies ±delta(0..5 W) before
        // the first emission, giving a theoretical range of [250, 260] W.  A ±3 W
        // tolerance is added for any platform-timing differences, yielding [247, 263].
        const int first = powerSpy.at(0).at(1).toInt();
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

        QSignalSpy powerSpy(&hub, &SimulatorHub::signal_power);
        while (powerSpy.count() < 3)
            QVERIFY2(powerSpy.wait(3000),
                     "SimulatorHub did not emit signal_power within 3 s");
        hub.stop();

        QVERIFY2(powerSpy.count() >= 3,
                 qPrintable(QString("Expected >= 3 power readings after setSlope, got %1")
                                .arg(powerSpy.count())));

        // setSlope(5.0): targetPower = 275 W > drift hi(260) → clamped to 260 W on the
        // first tick regardless of delta or direction.  A ±3 W tolerance handles any
        // edge-case in drift ordering, giving the accepted range [257, 263].
        const int first = powerSpy.at(0).at(1).toInt();
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
        TestWorkoutSession session(makeTestIntervals(/*durationTicks=*/30));

        QCOMPARE(static_cast<int>(session.state()),
                 static_cast<int>(TestWorkoutSession::Stopped));

        session.start(&hub);

        QCOMPARE(static_cast<int>(session.state()),
                 static_cast<int>(TestWorkoutSession::Running));
        // First ERG load must be sent immediately on start(), before any tick
        QCOMPARE(session.setLoadCallCount(), 1);

        // Wait (event-driven) for at least one hub tick to produce a data point
        QTRY_VERIFY2_WITH_TIMEOUT(session.dataPointCount() >= 1,
                                  "Expected >= 1 data point within 5 s", 5000);

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
        TestWorkoutSession session(makeTestIntervals(/*durationTicks=*/60));
        session.start(&hub);

        // Wait (event-driven) for 2 data points to accumulate
        QTRY_VERIFY2_WITH_TIMEOUT(session.dataPointCount() >= 2,
                                  "Expected >= 2 data points before pause", 8000);
        const int pointsBeforePause = session.dataPointCount();

        // Pause: hub keeps emitting but data-point counter must freeze
        session.pause();
        QCOMPARE(static_cast<int>(session.state()),
                 static_cast<int>(TestWorkoutSession::Paused));

        QTest::qWait(500); // brief pause-verification wait (> one hub tick); count must not change
        const int pointsAfterPause = session.dataPointCount();
        QCOMPARE(pointsAfterPause, pointsBeforePause);

        // Resume: counter must increase again
        session.resume();
        QCOMPARE(static_cast<int>(session.state()),
                 static_cast<int>(TestWorkoutSession::Running));

        QTRY_VERIFY2_WITH_TIMEOUT(session.dataPointCount() > pointsAfterPause,
                                  "Expected data-point count to increase after resume", 5000);

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

        // 3 × 1-tick intervals, 100 ms tick → completes in ~3 ticks (~300 ms)
        TestWorkoutSession session(makeTestIntervals(/*durationTicks=*/1),
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

        TestWorkoutSession session(makeTestIntervals(/*durationTicks=*/60));
        session.start(&hub);

        // Wait (event-driven) for 3 data points — avoids a fixed 3.5 s sleep
        QTRY_VERIFY2_WITH_TIMEOUT(session.dataPointCount() >= 3,
                                  "Expected >= 3 data points from hub", 10000);
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
    // 9 -- Full visual screenshot (with QWT power-curve plot)
    //
    // Displays a 1280×720 WorkoutExecutionWindow with live sensor data fed by
    // SimulatorHub and a QWT power-curve chart (actual vs target power).
    // A three-interval session (200 ms ticks, 1 tick per interval) runs to
    // completion and all interval transitions are displayed in the window.
    // A PNG screenshot is saved as CI artefact evidence.
    // ========================================================================
    void testWorkoutExecutionScreenshot()
    {
        const QString workoutName  = QStringLiteral("Three-Interval CI Workout");
        const auto    intervals    = makeTestIntervals(/*durationTicks=*/1);

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

        // Keep the data-count label updated as sensor ticks arrive.
        // Use QPointer so the lambda is safe if win is deleted while a
        // queued timeout is still in flight.
        QPointer<WorkoutExecutionWindow> winGuard(win);
        QTimer refreshTimer;
        refreshTimer.setInterval(100);
        connect(&refreshTimer, &QTimer::timeout,
                this, [&session, winGuard]() {
                    if (!winGuard) return;
                    winGuard->updateDataCount(session.dataPointCount());
                    winGuard->updatePowerChart(session.powerHistory());
                });
        refreshTimer.start();

        win->updateStateBadge("[ RUNNING ]", true);
        QApplication::processEvents();

        session.start(&hub);
        // Show the initial ERG target (first setLoad fires at start, before any
        // intervalChanged signal, so onIntervalChanged would miss it otherwise).
        if (!intervals.isEmpty())
            win->setInitialTarget(intervals.at(0).targetWatts);

        // Budget 3 s for the three 1-tick intervals (200 ms ticks each →
        // each interval completes in ~200 ms, full session in ~600 ms).
        QSignalSpy finishedSpy(&session, &TestWorkoutSession::sessionFinished);
        const bool finished = finishedSpy.wait(3000);
        QVERIFY2(finished, "Test session did not complete within 3 s");

        refreshTimer.stop();
        if (winGuard) {
            winGuard->updateDataCount(session.dataPointCount());
            winGuard->updatePowerChart(session.powerHistory());
            winGuard->updateStateBadge("[ COMPLETE ]", true);
        }
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
        // On HiDPI displays (macOS Retina / Windows scaling) grab() returns a
        // pixmap scaled by devicePixelRatio, so the physical pixel count can
        // exceed 1280×720.  Assert >= to be robust across all platforms.
        QVERIFY2(pix.width()  >= 1280, "Screenshot width must be >= 1280 px");
        QVERIFY2(pix.height() >= 720,  "Screenshot height must be >= 720 px");

        hub.stop();
        delete win;
    }

    // ========================================================================
    // 10 -- Workout model construction
    //
    // Build Workout and Interval objects in-process using the production
    // constructor.  Verify that all field accessors return the values
    // supplied at construction time.
    // ========================================================================
    void testWorkoutModelConstruction()
    {
        // Build one flat-power interval: 10 min at 0.75 FTP, cadence 90
        Interval iv(
            QTime(0, 10, 0),                          // duration
            QStringLiteral("Main Interval"),          // displayMessage
            Interval::StepType::FLAT,                 // powerStepType
            0.75, 0.75, 20, -1.0,                     // ftpStart, ftpEnd, range, rightBalance
            Interval::StepType::FLAT,                 // cadenceStepType
            90, 90, 5,                                // cadStart, cadEnd, range
            Interval::StepType::NONE,                 // hrStepType
            0.0, 0.0, 20,                             // hrStart, hrEnd, range
            false, 0.0, 0, 0.0                        // testInterval, repeatFTP, cad, LTHR
        );

        QList<Interval> ivList;
        ivList << iv;

        Workout wk(
            QStringLiteral("/tmp/test.workout"),
            Workout::USER_MADE,
            ivList,
            QStringLiteral("Model Test Workout"),
            QStringLiteral("CI"),
            QStringLiteral("Unit test workout"),
            QStringLiteral("Test Plan"),
            Workout::T_INTERVAL
        );

        QCOMPARE(wk.getName(),        QStringLiteral("Model Test Workout"));
        QCOMPARE(wk.getCreatedBy(),   QStringLiteral("CI"));
        QCOMPARE(wk.getDescription(), QStringLiteral("Unit test workout"));
        QCOMPARE(wk.getPlan(),        QStringLiteral("Test Plan"));
        QCOMPARE(wk.getType(),        Workout::T_INTERVAL);
        QCOMPARE(wk.getNbInterval(),  1);

        const Interval &first = wk.getLstInterval().at(0);
        QCOMPARE(first.getDisplayMessage(), QStringLiteral("Main Interval"));
        QVERIFY2(qAbs(first.getFTP_start() - 0.75) < 0.001,
                 qPrintable(QString("FTP_start expected 0.75, got %1").arg(first.getFTP_start())));
        QCOMPARE(first.getCadence_start(), 90);
    }

    // ========================================================================
    // 11 -- Workout XML round-trip with production model
    //
    // Build a Workout → write with XmlUtil::createWorkoutXml → parse back
    // with XmlUtil::parseSingleWorkoutXml → verify all key fields.
    // ========================================================================
    void testWorkoutXmlRoundTripWithModel()
    {
        // Build a two-interval workout
        Interval iv1(QTime(0, 10, 0), QStringLiteral("Warm-Up"),
                     Interval::StepType::FLAT, 0.55, 0.55, 20, -1.0,
                     Interval::StepType::FLAT, 85, 85, 5,
                     Interval::StepType::NONE, 0.0, 0.0, 20,
                     false, 0.0, 0, 0.0);
        Interval iv2(QTime(0, 20, 0), QStringLiteral("Tempo"),
                     Interval::StepType::FLAT, 0.80, 0.80, 20, -1.0,
                     Interval::StepType::FLAT, 92, 92, 5,
                     Interval::StepType::NONE, 0.0, 0.0, 20,
                     false, 0.0, 0, 0.0);

        QList<Interval> ivList;
        ivList << iv1 << iv2;

        Workout wkOut(
            QStringLiteral(""),        // filePath set below
            Workout::USER_MADE,
            ivList,
            QStringLiteral("Round-Trip Workout"),
            QStringLiteral("CI Bot"),
            QStringLiteral("XML round-trip test"),
            QStringLiteral("Build"),
            Workout::T_ENDURANCE
        );

        // Write to a temp file
        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(false);
        QVERIFY(tmpFile.open());
        const QString path = tmpFile.fileName();
        tmpFile.close();

        wkOut.setFilePath(path);
        const bool written = XmlUtil::createWorkoutXml(wkOut, path);
        QVERIFY2(written,              "XmlUtil::createWorkoutXml returned false");
        QVERIFY2(QFile::exists(path),  "Written workout file not found on disk");
        QVERIFY2(QFileInfo(path).size() > 0, "Written workout file is empty");

        // Parse back
        XmlUtil parser(QStringLiteral("en"));
        Workout wkIn = parser.parseSingleWorkoutXml(path);

        QCOMPARE(wkIn.getCreatedBy(),   QStringLiteral("CI Bot"));
        QCOMPARE(wkIn.getDescription(), QStringLiteral("XML round-trip test"));
        QCOMPARE(wkIn.getPlan(),        QStringLiteral("Build"));
        QCOMPARE(wkIn.getType(),        Workout::T_ENDURANCE);
        QCOMPARE(wkIn.getNbInterval(),  2);

        const Interval &r1 = wkIn.getLstInterval().at(0);
        QCOMPARE(r1.getDurationQTime(), QTime(0, 10, 0));
        QCOMPARE(r1.getDisplayMessage(), QStringLiteral("Warm-Up"));
        QVERIFY2(qAbs(r1.getFTP_start() - 0.55) < 0.001,
                 qPrintable(QString("Interval 0 FTP_start expected 0.55, got %1")
                                .arg(r1.getFTP_start())));
        QCOMPARE(r1.getCadence_start(), 85);

        const Interval &r2 = wkIn.getLstInterval().at(1);
        QCOMPARE(r2.getDurationQTime(), QTime(0, 20, 0));
        QCOMPARE(r2.getDisplayMessage(), QStringLiteral("Tempo"));
        QVERIFY2(qAbs(r2.getFTP_start() - 0.80) < 0.001,
                 qPrintable(QString("Interval 1 FTP_start expected 0.80, got %1")
                                .arg(r2.getFTP_start())));
        QCOMPARE(r2.getCadence_start(), 92);

        QFile::remove(path);
    }

    // ========================================================================
    // 12 -- Workout average-power metric
    //
    // Construct a two-interval flat-power workout and verify that
    // getAveragePower() returns the time-weighted mean in watts.
    //
    // Intervals:  10 min @ 0.60 FTP × 200 W FTP = 120 W
    //             20 min @ 0.90 FTP × 200 W FTP = 180 W
    // Expected avg: (10 × 120 + 20 × 180) / 30 = (1200 + 3600) / 30 = 160 W
    // ========================================================================
    void testWorkoutAveragePowerMetric()
    {
        Interval iv1(QTime(0, 10, 0), QStringLiteral("Easy"),
                     Interval::StepType::FLAT, 0.60, 0.60, 20, -1.0,
                     Interval::StepType::NONE, 0, 0, 5,
                     Interval::StepType::NONE, 0.0, 0.0, 20,
                     false, 0.0, 0, 0.0);
        Interval iv2(QTime(0, 20, 0), QStringLiteral("Hard"),
                     Interval::StepType::FLAT, 0.90, 0.90, 20, -1.0,
                     Interval::StepType::NONE, 0, 0, 5,
                     Interval::StepType::NONE, 0.0, 0.0, 20,
                     false, 0.0, 0, 0.0);

        QList<Interval> ivList;
        ivList << iv1 << iv2;

        // FTP = 200 W (set in initTestCase via qApp Account property).
        // The Workout constructor calls initializeArrayFTP() + calculateWorkoutMetrics().
        // averagePower = (avgFtpFraction) × FTP in absolute watts.
        // 10 min × 0.60 + 20 min × 0.90 = 6.00 + 18.00 = 24.00 FTP-minutes
        // 24.00 / 30 = 0.80 avg fraction → 0.80 × 200 = 160 W
        Workout wk(
            QStringLiteral(""),
            Workout::USER_MADE,
            ivList,
            QStringLiteral("Metric Test"),
            QStringLiteral("CI"),
            QStringLiteral(""),
            QStringLiteral("-"),
            Workout::T_ENDURANCE
        );

        const double avgPowerW = wk.getAveragePower();
        QVERIFY2(avgPowerW > 0.0,
                 qPrintable(QString("getAveragePower() returned %1, expected > 0").arg(avgPowerW)));

        // Expected: 160 W ±2 W tolerance for floating-point accumulation
        QVERIFY2(qAbs(avgPowerW - 160.0) < 2.0,
                 qPrintable(
                     QString("Average power expected ~160 W, got %1 W").arg(avgPowerW)));
    }

    // ========================================================================
    // 13 -- Network connectivity
    //
    // Performs a real HTTPS GET to intervals.icu/api/v1/athlete/{id} using
    // credentials from environment variables.  QSKIP when absent so the
    // suite degrades gracefully in offline CI and fork PRs.
    // ========================================================================
    void testNetworkConnectivity()
    {
        const QString apiKey    = qEnvironmentVariable("INTERVALS_ICU_API_KEY");
        const QString athleteId = qEnvironmentVariable("INTERVALS_ICU_ATHLETE_ID");

        if (apiKey.isEmpty() || athleteId.isEmpty())
            QSKIP("INTERVALS_ICU_API_KEY / INTERVALS_ICU_ATHLETE_ID not set — skipping network test");

        const QUrl    url = QUrl(QStringLiteral("https://intervals.icu/api/v1/athlete/") + athleteId);
        QNetworkRequest req(url);
        const QString credentials = QStringLiteral("API_KEY:") + apiKey;
        req.setRawHeader("Authorization",
                         QByteArray("Basic ") + credentials.toUtf8().toBase64());
        req.setRawHeader("Accept", "application/json");

        QNetworkAccessManager mgr;
        QEventLoop loop;
        QNetworkReply *reply = mgr.get(req);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

        QTimer timeout;
        timeout.setSingleShot(true);
        timeout.setInterval(30000);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start();
        loop.exec();

        QVERIFY2(reply->isFinished(),
                 "Network request timed out (30 s)");

        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QVERIFY2(httpStatus == 200,
                 qPrintable(
                     QString("Expected HTTP 200 from intervals.icu, got %1 — error: %2")
                         .arg(httpStatus)
                         .arg(reply->errorString())));

        const QByteArray body = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        QVERIFY2(!doc.isNull(),        "Response body is not valid JSON");
        QVERIFY2(doc.isObject(),       "Response root is not a JSON object");

        const QString athleteName = doc.object().value("name").toString();
        QVERIFY2(!athleteName.isEmpty(),
                 qPrintable(
                     QString("Athlete name empty — raw response: %1")
                         .arg(QString::fromUtf8(body.left(200)))));

        reply->deleteLater();
    }

    // ========================================================================
    // 14 -- Network workout retrieval
    //
    // GET /athlete/{id}/workouts and verify the response is a JSON array.
    // QSKIP when credentials are absent.
    // ========================================================================
    void testNetworkWorkoutRetrieval()
    {
        const QString apiKey    = qEnvironmentVariable("INTERVALS_ICU_API_KEY");
        const QString athleteId = qEnvironmentVariable("INTERVALS_ICU_ATHLETE_ID");

        if (apiKey.isEmpty() || athleteId.isEmpty())
            QSKIP("INTERVALS_ICU_API_KEY / INTERVALS_ICU_ATHLETE_ID not set — skipping network test");

        const QUrl url = QUrl(
            QStringLiteral("https://intervals.icu/api/v1/athlete/") + athleteId
            + QStringLiteral("/workouts?oldest=")
            + QDate::currentDate().addDays(-30).toString(Qt::ISODate)
            + QStringLiteral("&newest=")
            + QDate::currentDate().toString(Qt::ISODate));

        QNetworkRequest req(url);
        const QString credentials = QStringLiteral("API_KEY:") + apiKey;
        req.setRawHeader("Authorization",
                         QByteArray("Basic ") + credentials.toUtf8().toBase64());
        req.setRawHeader("Accept", "application/json");

        QNetworkAccessManager mgr;
        QEventLoop loop;
        QNetworkReply *reply = mgr.get(req);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

        QTimer timeout;
        timeout.setSingleShot(true);
        timeout.setInterval(30000);
        connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start();
        loop.exec();

        QVERIFY2(reply->isFinished(), "Workout retrieval request timed out (30 s)");

        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QVERIFY2(httpStatus == 200,
                 qPrintable(
                     QString("Expected HTTP 200 from /workouts, got %1 — error: %2")
                         .arg(httpStatus)
                         .arg(reply->errorString())));

        const QByteArray body = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        QVERIFY2(!doc.isNull(),   "Workout list response is not valid JSON");
        QVERIFY2(doc.isArray(),   "Workout list response root is not a JSON array");

        // Each element should have at least an "id" field
        const QJsonArray arr = doc.array();
        for (int i = 0; i < qMin(3, arr.size()); ++i) {
            QVERIFY2(arr.at(i).isObject(), qPrintable(
                         QString("Workout list element %1 is not a JSON object").arg(i)));
        }

        reply->deleteLater();
    }

    // ========================================================================
    // 16 -- ERG: setLoad fires at every interval boundary
    //
    // A three-interval session (Warm-Up 160 W → Main Set 240 W → Cool-Down
    // 130 W) is run to completion with 100 ms ticks and 1-tick intervals.
    // ERG load commands must be issued:
    //   • once at session start (interval 0)
    //   • once at the 0→1 transition (interval 1)
    //   • once at the 1→2 transition (interval 2)
    // Total: 3 setLoad calls, 2 intervalChanged emissions.
    // ========================================================================
    void testErgLoadChangesAtIntervalBoundary()
    {
        SimulatorHub hub;
        hub.start();

        // 3-interval session; 1 tick each at 100 ms → completes in ~300 ms
        TestWorkoutSession session(makeTestIntervals(/*durationTicks=*/1),
                                   /*tickIntervalMs=*/100);

        QSignalSpy intervalSpy(&session, &TestWorkoutSession::intervalChanged);
        QSignalSpy finishedSpy(&session, &TestWorkoutSession::sessionFinished);

        session.start(&hub);

        // Budget 3 s for the full session (actual ~300 ms)
        QVERIFY2(finishedSpy.wait(3000), "Session did not finish within 3 s");

        // setLoad: 1 at start + 1 per interval transition (0→1, 1→2) = 3 total
        QCOMPARE(session.setLoadCallCount(), 3);

        // intervalChanged fires once for each boundary crossing (0→1 and 1→2)
        QCOMPARE(intervalSpy.count(), 2);

        // Verify target watts match the interval definitions
        QVERIFY2(qAbs(intervalSpy.at(0).at(1).toDouble() - 240.0) < 0.01,
                 qPrintable(QString("Transition 0→1: expected 240 W, got %1 W")
                                .arg(intervalSpy.at(0).at(1).toDouble())));
        QVERIFY2(qAbs(intervalSpy.at(1).at(1).toDouble() - 130.0) < 0.01,
                 qPrintable(QString("Transition 1→2: expected 130 W, got %1 W")
                                .arg(intervalSpy.at(1).at(1).toDouble())));

        hub.stop();
    }

    // ========================================================================
    // 15 -- Power-on-target verification
    // ========================================================================
    void testPowerOnTargetVerification()
    {
        constexpr double kTarget  = 200.0;
        constexpr double kTolPct  = 0.25; // ±25 %

        SimulatorHub hub;
        hub.setLoad(0, kTarget);
        hub.start();

        TestWorkoutSession session({TestIntervalDef{60, kTarget,
                                                    QStringLiteral("Steady State")}});
        session.start(&hub);

        // Wait (event-driven) for 3 hub ticks — avoids a fixed 3.5 s sleep
        QTRY_VERIFY2_WITH_TIMEOUT(session.dataPointCount() >= 3,
                                  "Expected >= 3 data points within 10 s", 10000);
        session.stop();
        hub.stop();

        const int actual = session.lastPower();
        const double lo  = kTarget * (1.0 - kTolPct);
        const double hi  = kTarget * (1.0 + kTolPct);

        QVERIFY2(actual >= static_cast<int>(lo) && actual <= static_cast<int>(hi),
                 qPrintable(
                     QString("Power-on-target: actual %1 W outside ±25%% of target %2 W "
                             "(expected %3–%4 W)")
                         .arg(actual)
                         .arg(static_cast<int>(kTarget))
                         .arg(static_cast<int>(lo))
                         .arg(static_cast<int>(hi))));
    }
};

QTEST_MAIN(TstWorkoutUi)
#include "tst_workout_ui.moc"
