/*
 * tst_offline_mode.cpp
 *
 * Offline Mode Integration Test -- MaximumTrainer
 *
 * Purpose
 * -----------------------------------------------------------------------
 * Validates that MaximumTrainer's core offline functionality works
 * correctly across Windows, macOS, and Linux without a network connection
 * and without physical Bluetooth hardware:
 *
 *   1. Local workout access: A workout XML file is created on disk and
 *      opened / parsed correctly using only the local file system.
 *
 *   2. BTLE cycling simulation: SimulatorHub broadcasts realistic
 *      Cycling Speed & Cadence (CSC, 0x1816) and Cycling Power (0x1818)
 *      data over the same signal contract as BtleHub, allowing the full
 *      workout data pipeline to be exercised without any sensor hardware.
 *
 *   3. Offline UI screenshot: A labelled 1280x720 window is shown with
 *      an "OFFLINE MODE" badge, live BTLE metrics, a workout-loaded
 *      indicator, and a data-recording counter.  The screenshot is saved
 *      as build evidence and uploaded as a CI artefact.
 *
 *   4. Data integrity: Cadence, speed, and power data points are
 *      accumulated during the simulated session and written to a
 *      tab-separated activity file, confirming that workout data can be
 *      persisted locally with no network involvement.
 *
 * No network module is linked.  All data originates from SimulatorHub.
 *
 * Build:
 *   cmake --build build --target offline_mode_tests
 * Run headless (Linux CI):
 *   Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
 *   ../../build/tests/offline_mode_tests -v2
 * Run directly (Windows / macOS CI -- display is always available):
 *   .\build\tests\offline_mode_tests.exe -v2
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
#include <QTextStream>
#include <QDateTime>
#include <QSysInfo>
#include <QDir>
#include <QTimer>

#include "../../src/btle/simulator_hub.h"

// ---------------------------------------------------------------------------
// Compile-time platform tag used in screenshot filenames and window title
// ---------------------------------------------------------------------------
#if defined(Q_OS_WIN)
    static const QString kPlatformTag = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    static const QString kPlatformTag = QStringLiteral("macos");
#else
    static const QString kPlatformTag = QStringLiteral("linux");
#endif

// ---------------------------------------------------------------------------
// DataPoint – one second of cycling telemetry recorded during the session
// ---------------------------------------------------------------------------
struct DataPoint {
    int    cadence; // rpm
    double speed;   // km/h
    int    power;   // W
};

// ---------------------------------------------------------------------------
// OfflineModeWindow
//
// 1280×720 window that demonstrates the offline mode UI state:
//   • "OFFLINE MODE" badge in the header
//   • Network status label (no external calls)
//   • Live BTLE metrics (cadence, speed, power) from SimulatorHub
//   • Workout-file loaded indicator
//   • Data-recording counter (data points accumulated so far)
// ---------------------------------------------------------------------------
class OfflineModeWindow : public QWidget
{
    Q_OBJECT

public:
    explicit OfflineModeWindow(const QString &workoutName,
                               const QString &timestamp,
                               QWidget       *parent = nullptr)
        : QWidget(parent)
    {
        const QString osName   = QSysInfo::prettyProductName();
        const QString qtVer    = QString("Qt %1").arg(qVersion());
        const QString platform = kPlatformTag.toUpper();

        setWindowTitle(
            QString("MaximumTrainer -- Offline Mode [%1]").arg(platform));
        setFixedSize(1280, 720);

        setStyleSheet(
            "OfflineModeWindow { background-color: #0d1117; }"
            "QLabel { color: #c9d1d9;"
            "         font-family: 'DejaVu Sans', 'Segoe UI', sans-serif; }");

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(48, 32, 48, 32);
        root->setSpacing(0);

        // -- Header -----------------------------------------------------------
        auto *headerRow = new QHBoxLayout();

        auto *appTitle = new QLabel("MaximumTrainer", this);
        appTitle->setStyleSheet(
            "font-size: 28px; font-weight: bold; color: #58a6ff;");

        m_offlineBadge = new QLabel("[ OFFLINE MODE ]", this);
        m_offlineBadge->setStyleSheet(
            "font-size: 14px; color: #f0883e; background: #161b22;"
            "border: 1px solid #f0883e; border-radius: 4px;"
            "padding: 4px 12px;");
        m_offlineBadge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        headerRow->addWidget(appTitle,       1);
        headerRow->addWidget(m_offlineBadge, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
        root->addLayout(headerRow);
        root->addSpacing(6);

        // -- Meta row ---------------------------------------------------------
        auto *metaLabel = new QLabel(
            QString("Platform: %1  |  %2  |  %3  |  %4")
                .arg(platform, osName, qtVer, timestamp),
            this);
        metaLabel->setStyleSheet("font-size: 12px; color: #8b949e;");
        root->addWidget(metaLabel);
        root->addSpacing(16);

        auto *sep1 = new QFrame(this);
        sep1->setFrameShape(QFrame::HLine);
        sep1->setStyleSheet("color: #21262d;");
        root->addWidget(sep1);
        root->addSpacing(16);

        // -- Status row: network + BTLE ---------------------------------------
        auto *statusRow = new QHBoxLayout();

        auto *netLabel = new QLabel(
            "[--]  Network: OFFLINE  (no external API calls)", this);
        netLabel->setStyleSheet("font-size: 14px; color: #8b949e;");

        auto *btLabel = new QLabel(
            "[BT]  BTLE Simulator: ACTIVE  (CSC 0x1816 + Power 0x1818)",
            this);
        btLabel->setStyleSheet("font-size: 14px; color: #3fb950;");
        btLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        statusRow->addWidget(netLabel, 1);
        statusRow->addWidget(btLabel,  1, Qt::AlignRight);
        root->addLayout(statusRow);
        root->addSpacing(8);

        // -- Workout row ------------------------------------------------------
        auto *wkLabel = new QLabel(
            QString("[WK]  Workout: %1  [LOADED from local file system]")
                .arg(workoutName),
            this);
        wkLabel->setStyleSheet("font-size: 14px; color: #79c0ff;");
        root->addWidget(wkLabel);
        root->addSpacing(16);

        auto *sep2 = new QFrame(this);
        sep2->setFrameShape(QFrame::HLine);
        sep2->setStyleSheet("color: #21262d;");
        root->addWidget(sep2);
        root->addSpacing(16);

        // -- Sensor panel -----------------------------------------------------
        auto *panelFrame = new QFrame(this);
        panelFrame->setStyleSheet(
            "QFrame { background: #161b22; border: 1px solid #30363d;"
            "         border-radius: 8px; }");
        auto *panelLayout = new QVBoxLayout(panelFrame);
        panelLayout->setContentsMargins(32, 20, 32, 20);
        panelLayout->setSpacing(16);

        auto *panelTitle = new QLabel(
            "Live Sensor Data -- BTLE Simulator (offline)", panelFrame);
        panelTitle->setStyleSheet(
            "font-size: 13px; color: #8b949e; font-weight: bold;");
        panelLayout->addWidget(panelTitle);

        // Grid: Cadence | Speed | Power  (3 big metrics)
        auto *grid = new QGridLayout();
        grid->setVerticalSpacing(24);
        grid->setHorizontalSpacing(48);

        struct Metric { QLabel *icon; QLabel *name; QLabel *value; QLabel *unit; };

        auto makeMetric = [&](const QString &icon, const QString &name,
                               const QString &unit) -> Metric {
            auto *iconL = new QLabel(icon, panelFrame);
            auto *nameL = new QLabel(name, panelFrame);
            auto *valL  = new QLabel("--",  panelFrame);
            auto *unitL = new QLabel(unit,  panelFrame);
            iconL->setStyleSheet("font-size: 22px; color: #8b949e;");
            nameL->setStyleSheet("font-size: 14px; color: #8b949e;");
            valL->setStyleSheet(
                "font-size: 48px; font-weight: bold; color: #7ee787;"
                "min-width: 110px;");
            valL->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            unitL->setStyleSheet("font-size: 14px; color: #8b949e; min-width: 50px;");
            unitL->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            return {iconL, nameL, valL, unitL};
        };

        Metric cad = makeMetric("[CAD]", "Cadence",  "rpm");
        Metric spd = makeMetric("[SPD]", "Speed",    "km/h");
        Metric pwr = makeMetric("[PWR]", "Power",    "W");

        m_cadLabel = cad.value;
        m_spdLabel = spd.value;
        m_pwrLabel = pwr.value;

        auto addToGrid = [&](const Metric &m, int col) {
            grid->addWidget(m.icon,  0, col + 0, Qt::AlignCenter);
            grid->addWidget(m.name,  0, col + 1);
            grid->addWidget(m.value, 0, col + 2, Qt::AlignRight);
            grid->addWidget(m.unit,  0, col + 3);
        };

        addToGrid(cad, 0);
        addToGrid(spd, 4);
        addToGrid(pwr, 8);

        panelLayout->addLayout(grid);
        root->addWidget(panelFrame);
        root->addSpacing(16);

        auto *sep3 = new QFrame(this);
        sep3->setFrameShape(QFrame::HLine);
        sep3->setStyleSheet("color: #21262d;");
        root->addWidget(sep3);
        root->addSpacing(12);

        // -- Footer -----------------------------------------------------------
        auto *footerRow = new QHBoxLayout();

        m_recordingLabel = new QLabel("Recording: 0 data points", this);
        m_recordingLabel->setStyleSheet("font-size: 13px; color: #3fb950;");

        m_elapsedLabel = new QLabel("Elapsed: 0 s", this);
        m_elapsedLabel->setStyleSheet("font-size: 12px; color: #8b949e;");
        m_elapsedLabel->setAlignment(Qt::AlignCenter);

        m_artifactLabel = new QLabel(
            QString("Artefact: offline-mode-%1-%2.png")
                .arg(kPlatformTag, timestamp),
            this);
        m_artifactLabel->setStyleSheet("font-size: 12px; color: #8b949e;");
        m_artifactLabel->setAlignment(Qt::AlignRight);

        footerRow->addWidget(m_recordingLabel, 1);
        footerRow->addWidget(m_elapsedLabel,   1, Qt::AlignCenter);
        footerRow->addWidget(m_artifactLabel,  1, Qt::AlignRight);
        root->addLayout(footerRow);

        // -- Elapsed timer ----------------------------------------------------
        m_uiTimer = new QTimer(this);
        m_uiTimer->setInterval(1000);
        connect(m_uiTimer, &QTimer::timeout,
                this, &OfflineModeWindow::tick);
        m_uiTimer->start();
    }

    // Accessors used by the test to assert values
    int                     cadence()       const { return m_cadence; }
    double                  speed()         const { return m_speed;   }
    int                     power()         const { return m_power;   }
    int                     recordedCount() const { return m_recorded.size(); }
    const QVector<DataPoint> &recorded()    const { return m_recorded; }

public slots:
    void onCadence(int /*uid*/, int val) {
        m_cadence = val;
        m_cadLabel->setText(QString::number(val));
    }
    void onSpeed(int /*uid*/, double val) {
        m_speed = val;
        m_spdLabel->setText(QString::number(val, 'f', 1));
    }
    void onPower(int /*uid*/, int val) {
        m_power = val;
        m_pwrLabel->setText(QString::number(val));
        // Record a data point only when all three cycling channels carry
        // valid data (cadence and speed are populated before power in each
        // SimulatorHub tick, so this check is safe).
        if (m_cadence > 0 && m_speed > 0.01 && m_power > 0) {
            m_recorded.append({m_cadence, m_speed, m_power});
            m_recordingLabel->setText(
                QString("Recording: %1 data point(s)").arg(m_recorded.size()));
        }
    }

private slots:
    void tick() {
        m_elapsed++;
        m_elapsedLabel->setText(QString("Elapsed: %1 s").arg(m_elapsed));
    }

private:
    QLabel *m_cadLabel       = nullptr;
    QLabel *m_spdLabel       = nullptr;
    QLabel *m_pwrLabel       = nullptr;
    QLabel *m_recordingLabel = nullptr;
    QLabel *m_elapsedLabel   = nullptr;
    QLabel *m_artifactLabel  = nullptr;
    QLabel *m_offlineBadge   = nullptr;
    QTimer *m_uiTimer        = nullptr;

    int    m_cadence = 0;
    double m_speed   = 0.0;
    int    m_power   = 0;
    int    m_elapsed = 0;

    QVector<DataPoint> m_recorded;
};


// ---------------------------------------------------------------------------
// TstOfflineMode -- QTest class
// ---------------------------------------------------------------------------
class TstOfflineMode : public QObject
{
    Q_OBJECT

private slots:

    // -----------------------------------------------------------------------
    // testLocalWorkoutAccess
    //
    // Verifies that a workout XML file can be created on the local file
    // system and parsed correctly using only QFile + QXmlStreamReader --
    // with no network connection required.
    //
    // Acceptance criteria:
    //   • QTemporaryFile is created and writable.
    //   • Written content is a well-formed Workout XML document.
    //   • QXmlStreamReader finds the root <Workout> element without errors.
    //   • At least one <Interval> element with a <Duration> child is present.
    // -----------------------------------------------------------------------
    void testLocalWorkoutAccess()
    {
        // ── Create a minimal .workout XML in a temporary file ────────────────
        QTemporaryFile tmpFile;
        tmpFile.setFileTemplate(
            QDir::tempPath() + "/maximumtrainer_test_XXXXXX.workout");
        QVERIFY2(tmpFile.open(),
                 "Failed to create temporary workout file on local file system");

        {
            QXmlStreamWriter writer(&tmpFile);
            writer.setAutoFormatting(true);
            writer.writeStartDocument();
            writer.writeStartElement("Workout");

            writer.writeTextElement("Version", "2.02");
            writer.writeTextElement("Plan",    "Offline Test Plan");
            writer.writeTextElement("Author",  "MaximumTrainer CI");
            writer.writeTextElement("Description",
                "Minimal test workout for offline mode validation.");
            writer.writeTextElement("Type", "1");

            writer.writeStartElement("Intervals");

            // Warm-up interval
            writer.writeStartElement("Interval");
            writer.writeTextElement("Duration",       "00:05:00");
            writer.writeTextElement("DisplayMessage", "Warm up");
            writer.writeStartElement("Power");
            writer.writeTextElement("StepType", "0");
            writer.writeTextElement("Start",    "0.50");
            writer.writeTextElement("End",      "0.50");
            writer.writeEndElement(); // Power
            writer.writeEndElement(); // Interval

            // Main effort interval
            writer.writeStartElement("Interval");
            writer.writeTextElement("Duration",       "00:20:00");
            writer.writeTextElement("DisplayMessage", "Steady effort");
            writer.writeStartElement("Power");
            writer.writeTextElement("StepType", "0");
            writer.writeTextElement("Start",    "0.75");
            writer.writeTextElement("End",      "0.75");
            writer.writeEndElement(); // Power
            writer.writeEndElement(); // Interval

            writer.writeEndElement(); // Intervals
            writer.writeEndElement(); // Workout
            writer.writeEndDocument();
        }

        tmpFile.flush();
        tmpFile.seek(0);

        // ── Parse the file back with QXmlStreamReader ────────────────────────
        QXmlStreamReader xml(&tmpFile);

        bool foundWorkout  = false;
        int  intervalCount = 0;
        bool foundDuration = false;

        while (!xml.atEnd() && !xml.hasError()) {
            xml.readNext();
            if (!xml.isStartElement())
                continue;

            if (xml.name().toString() == QLatin1String("Workout"))
                foundWorkout = true;
            else if (xml.name().toString() == QLatin1String("Interval"))
                ++intervalCount;
            else if (xml.name().toString() == QLatin1String("Duration"))
                foundDuration = true;
        }

        QVERIFY2(!xml.hasError(),
                 qPrintable(QString("XML parse error: %1").arg(xml.errorString())));
        QVERIFY2(foundWorkout,
                 "Workout XML must contain a root <Workout> element");
        QVERIFY2(intervalCount >= 2,
                 qPrintable(QString("Expected >= 2 <Interval> elements, got %1")
                                .arg(intervalCount)));
        QVERIFY2(foundDuration,
                 "At least one <Interval> must contain a <Duration> element");

        qDebug().noquote()
            << "[LocalWorkoutAccess] PASS -- file written and parsed:"
            << tmpFile.fileName()
            << "| intervals:" << intervalCount;
    }

    // -----------------------------------------------------------------------
    // testBtleSimulatorCyclingData
    //
    // Verifies that SimulatorHub correctly broadcasts Cycling Speed & Cadence
    // (CSC, profile 0x1816) and Cycling Power (profile 0x1818) signals with
    // realistic, drifting values -- without any physical BLE hardware.
    //
    // Acceptance criteria:
    //   • All three cycling channels (cadence, speed, power) emit non-zero
    //     values within 5 seconds.
    //   • Cadence is in [80, 100] rpm.
    //   • Speed   is in [23.0, 33.0] km/h.
    //   • Power   is in [170, 260] W.
    // -----------------------------------------------------------------------
    void testBtleSimulatorCyclingData()
    {
        int    cadence = 0;
        double speed   = 0.0;
        int    power   = 0;

        SimulatorHub sim;
        connect(&sim, &SimulatorHub::signal_cadence,
                [&](int, int c)    { cadence = c; });
        connect(&sim, &SimulatorHub::signal_speed,
                [&](int, double s) { speed   = s; });
        connect(&sim, &SimulatorHub::signal_power,
                [&](int, int p)    { power   = p; });

        sim.start();

        // Poll until all three channels carry non-zero values (or 5 s timeout)
        const int kTimeoutMs = 5000;
        const int kPollMs    = 100;
        int       elapsed    = 0;
        while ((cadence == 0 || speed < 0.01 || power == 0)
               && elapsed < kTimeoutMs) {
            QTest::qWait(kPollMs);
            elapsed += kPollMs;
        }

        QVERIFY2(cadence >= 80 && cadence <= 100,
                 qPrintable(
                     QString("CSC cadence %1 rpm out of expected [80, 100] range")
                         .arg(cadence)));
        QVERIFY2(speed >= 23.0 && speed <= 33.0,
                 qPrintable(
                     QString("CSC speed %1 km/h out of expected [23.0, 33.0] range")
                         .arg(speed)));
        QVERIFY2(power >= 170 && power <= 260,
                 qPrintable(
                     QString("Power %1 W out of expected [170, 260] range")
                         .arg(power)));

        qDebug() << "[BtleSimCycling] PASS -- Cadence:" << cadence
                 << "Speed:" << speed << "Power:" << power;
    }

    // -----------------------------------------------------------------------
    // testOfflineModeScreenshot
    //
    // End-to-end offline mode test:
    //   1. Creates and shows the 1280×720 offline mode window.
    //   2. Starts SimulatorHub (BTLE simulation -- no hardware).
    //   3. Waits up to 10 seconds for all three cycling channels to populate.
    //   4. Lets values settle for 2 more seconds.
    //   5. Captures a labelled screenshot as visual build evidence.
    //   6. Writes accumulated data points to a tab-separated activity file,
    //      confirming offline data-persistence works without a network.
    //   7. Asserts all cycling channels are in realistic ranges.
    // -----------------------------------------------------------------------
    void testOfflineModeScreenshot()
    {
        // -- Build artefact filenames ----------------------------------------
        const QString timestamp = QDateTime::currentDateTimeUtc()
                                      .toString("yyyy-MM-ddTHH-mm-ssZ");
        const QString screenshotName =
            QString("offline-mode-%1-%2.png").arg(kPlatformTag, timestamp);
        const QString activityName =
            QString("activity-%1-%2.tsv").arg(kPlatformTag, timestamp);
        const QString outDir  = QCoreApplication::applicationDirPath();
        const QString imgPath = outDir + "/" + screenshotName;
        const QString tsvPath = outDir + "/" + activityName;

        // -- Create and show the offline mode window -------------------------
        OfflineModeWindow window("offline_test_session.workout", timestamp);
        window.show();
        QCoreApplication::processEvents();

        // -- Wire SimulatorHub -> window -------------------------------------
        SimulatorHub sim;
        connect(&sim, &SimulatorHub::signal_cadence,
                &window, &OfflineModeWindow::onCadence);
        connect(&sim, &SimulatorHub::signal_speed,
                &window, &OfflineModeWindow::onSpeed);
        connect(&sim, &SimulatorHub::signal_power,
                &window, &OfflineModeWindow::onPower);

        sim.start();

        // -- Wait up to 10 s for all channels to carry non-zero values -------
        const int kTimeoutMs = 10000;
        const int kPollMs    = 100;
        int       elapsed    = 0;
        while ((window.cadence() == 0 || window.speed() < 0.01
                || window.power() == 0)
               && elapsed < kTimeoutMs) {
            QTest::qWait(kPollMs);
            elapsed += kPollMs;
        }

        const bool timedOut = (window.cadence() == 0 || window.speed() < 0.01
                               || window.power() == 0);

        // Let values settle for 2 more seconds so the window looks fully
        // populated in the screenshot
        if (!timedOut)
            QTest::qWait(2000);

        QCoreApplication::processEvents();
        QTest::qWait(50);

        // -- Capture screenshot ----------------------------------------------
        QPixmap screenshot = window.grab();
        QVERIFY2(!screenshot.isNull(),
                 "Screenshot grab() returned a null pixmap");
        QVERIFY2(screenshot.width()  >= 1280,
                 "Screenshot width must be >= 1280 px");
        QVERIFY2(screenshot.height() >= 720,
                 "Screenshot height must be >= 720 px");

        const bool saved = screenshot.save(imgPath, "PNG");
        QVERIFY2(saved,
                 qPrintable(
                     QString("Failed to save screenshot to: %1").arg(imgPath)));
        qDebug().noquote() << "[Screenshot] Saved:" << imgPath;

        // -- Fail-fast assert immediately after screenshot -------------------
        // Checked here so that the screenshot (which captures the error state)
        // is always saved before the test aborts.  Data-integrity checks below
        // only run when the simulator was confirmed to be working.
        QVERIFY2(!timedOut,
                 "FAIL-FAST: SimulatorHub did not emit cycling data within "
                 "10 seconds. See screenshot in build artefacts.");

        // -- Write accumulated data points to TSV (offline data integrity) ---
        // Only reached when !timedOut, so recordedCount() >= 2 is guaranteed
        // by the simulator having run for at least 2 ticks.
        QVERIFY2(window.recordedCount() >= 2,
                 qPrintable(
                     QString("Expected >= 2 recorded data points, got %1")
                         .arg(window.recordedCount())));

        {
            QFile tsv(tsvPath);
            QVERIFY2(tsv.open(QIODevice::WriteOnly | QIODevice::Text),
                     qPrintable(QString("Could not open activity file for writing: %1")
                                    .arg(tsvPath)));

            QTextStream out(&tsv);
            out << "timestamp_s\tcadence_rpm\tspeed_kmh\tpower_W\n";
            const auto &pts = window.recorded();
            for (int i = 0; i < pts.size(); ++i) {
                out << (i + 1) << '\t'
                    << pts[i].cadence << '\t'
                    << QString::number(pts[i].speed, 'f', 2) << '\t'
                    << pts[i].power   << '\n';
            }
        }

        QVERIFY2(QFile::exists(tsvPath),
                 qPrintable(QString("Activity TSV file not found: %1").arg(tsvPath)));
        qDebug().noquote() << "[DataIntegrity] Activity file saved:"
                           << tsvPath
                           << "| data points:" << window.recordedCount();

        // -- Assert all cycling channels are realistic -----------------------
        QVERIFY2(window.cadence() >= 80 && window.cadence() <= 100,
                 qPrintable(
                     QString("Cadence %1 rpm out of expected [80, 100]")
                         .arg(window.cadence())));
        QVERIFY2(window.speed() >= 23.0 && window.speed() <= 33.0,
                 qPrintable(
                     QString("Speed %1 km/h out of expected [23.0, 33.0]")
                         .arg(window.speed())));
        QVERIFY2(window.power() >= 170 && window.power() <= 260,
                 qPrintable(
                     QString("Power %1 W out of expected [170, 260]")
                         .arg(window.power())));

        qDebug() << "[OfflineMode] PASS -- Cadence:" << window.cadence()
                 << "Speed:"   << window.speed()
                 << "Power:"   << window.power()
                 << "Recorded:" << window.recordedCount() << "pts";
    }
};

QTEST_MAIN(TstOfflineMode)
#include "tst_offline_mode.moc"
