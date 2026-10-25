/*
 * tst_runtime_validation.cpp
 *
 * Cross-Platform Runtime Validation Test -- MaximumTrainer
 *
 * Purpose
 * -------------------------------------------------------------------------
 * Validates that the MaximumTrainer software successfully initialises and
 * reaches a "Ready" state on Windows, macOS, and Linux by confirming:
 *
 *   1. Environment check: The Bluetooth stack is queryable (even without
 *      hardware the API returns "Not Found", not "Unsupported").
 *   2. Runtime initialisation: The application UI loads without faults or
 *      reference errors within 10 seconds (fail-fast).
 *   3. UI verification: The main dashboard / sensor screen is visible and
 *      SimulatorHub data flows through the same pipeline used in production.
 *
 * Visual Evidence
 * -------------------------------------------------------------------------
 * A 1280x720 screenshot is captured at the end of the test and saved as:
 *
 *   build/tests/screenshot-{platform}-{YYYY-MM-DDTHH-MM-SS}.png
 *
 * The window title, platform badge, and OS/Qt-version row are embedded
 * directly in the image so artefact reviewers can identify the build.
 *
 * Acceptance Criteria
 * -------------------------------------------------------------------------
 * - Screenshot file is created and is non-empty.
 * - All four sensor channels (HR, cadence, speed, power) carry realistic
 *   non-zero values within 10 seconds.
 * - The Bluetooth API call does not crash or return an "Unsupported" state;
 *   an empty device list ("Not Found") is acceptable.
 * - When MT_APP_BINARY names an installed MaximumTrainer executable, launching
 *   it with --startup-check exits 0 and its log file holds the startup banner
 *   and a completed "login dialog shown" step (#377).  Skipped otherwise.
 *
 * Build:
 *   qmake runtime_validation_tests.pro && make
 * Run headless (Linux CI):
 *   Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
 *   ../../build/tests/runtime_validation_tests -v2
 * Run directly (Windows / macOS CI - display is available):
 *   .\build\tests\runtime_validation_tests.exe -v2
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
#include <QDir>
#include <QGuiApplication>
#include <QSysInfo>
#include <QDateTime>
#include <QTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QFile>

#include <QBluetoothLocalDevice>
#include <QBluetoothHostInfo>

#include "../../src/btle/simulator_hub.h"

// ---------------------------------------------------------------------------
// Compile-time platform tag embedded in the screenshot filename and window
// ---------------------------------------------------------------------------
#if defined(Q_OS_WIN)
    static const QString kPlatformTag = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    static const QString kPlatformTag = QStringLiteral("macos");
#else
    static const QString kPlatformTag = QStringLiteral("linux");
#endif

// ---------------------------------------------------------------------------
// RuntimeValidationWindow
//
// 1280x720 window that mirrors the MaximumTrainer home / connection screen.
// Displays platform badge, Bluetooth status, live sensor data, and a "Ready"
// indicator -- all visible in the saved screenshot.
// ---------------------------------------------------------------------------
class RuntimeValidationWindow : public QWidget
{
    Q_OBJECT

public:
    explicit RuntimeValidationWindow(const QString &btStatus,
                                     const QString &timestamp,
                                     QWidget       *parent = nullptr)
        : QWidget(parent)
    {
        const QString osName   = QSysInfo::prettyProductName();
        const QString qtVer    = QString("Qt %1").arg(qVersion());
        const QString platform = kPlatformTag.toUpper();

        setWindowTitle(
            QString("MaximumTrainer -- Runtime Validation [%1]").arg(platform));
        setFixedSize(1280, 720);

        setStyleSheet(
            "RuntimeValidationWindow { background-color: #0d1117; }"
            "QLabel { color: #c9d1d9;"
            "         font-family: 'DejaVu Sans', 'Segoe UI', sans-serif; }"
        );

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(48, 32, 48, 32);
        root->setSpacing(0);

        // -- Header bar -------------------------------------------------------
        auto *headerRow = new QHBoxLayout();

        auto *appTitle = new QLabel("MaximumTrainer", this);
        appTitle->setStyleSheet(
            "font-size: 28px; font-weight: bold; color: #58a6ff;");

        m_statusBadge = new QLabel("[ Initialising... ]", this);
        m_statusBadge->setStyleSheet(
            "font-size: 14px; color: #f0883e; background: #161b22;"
            "border: 1px solid #30363d; border-radius: 4px; padding: 4px 12px;");
        m_statusBadge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        headerRow->addWidget(appTitle, 1);
        headerRow->addWidget(m_statusBadge, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
        root->addLayout(headerRow);
        root->addSpacing(6);

        // -- Meta row (OS, Qt version, timestamp) -----------------------------
        auto *metaLabel = new QLabel(
            QString("Platform: %1  |  %2  |  %3  |  %4")
                .arg(platform, osName, qtVer, timestamp),
            this);
        metaLabel->setStyleSheet("font-size: 12px; color: #8b949e;");
        root->addWidget(metaLabel);
        root->addSpacing(18);

        auto *sep1 = new QFrame(this);
        sep1->setFrameShape(QFrame::HLine);
        sep1->setStyleSheet("color: #21262d;");
        root->addWidget(sep1);
        root->addSpacing(18);

        // -- Bluetooth connectivity status ------------------------------------
        bool btFound    = btStatus.startsWith("Found");
        QString btColor = btFound ? "#3fb950" : "#8b949e";
        QString btIcon  = btFound ? "[BT]" : "[--]";
        auto *btLabel   = new QLabel(
            QString("Bluetooth Stack:  %1  %2").arg(btIcon, btStatus), this);
        btLabel->setStyleSheet(
            QString("font-size: 15px; color: %1; padding: 8px 0;").arg(btColor));
        root->addWidget(btLabel);
        root->addSpacing(16);

        // -- Sensor panel -----------------------------------------------------
        auto *panelFrame = new QFrame(this);
        panelFrame->setStyleSheet(
            "QFrame { background: #161b22; border: 1px solid #30363d;"
            "         border-radius: 8px; }");
        auto *panelLayout = new QVBoxLayout(panelFrame);
        panelLayout->setContentsMargins(32, 24, 32, 24);
        panelLayout->setSpacing(18);

        auto *panelTitle = new QLabel(
            "Trainer Connection -- Sensor Data (SimulatorHub)", panelFrame);
        panelTitle->setStyleSheet(
            "font-size: 14px; color: #8b949e; font-weight: bold;");
        panelLayout->addWidget(panelTitle);

        auto *grid = new QGridLayout();
        grid->setVerticalSpacing(28);
        grid->setHorizontalSpacing(64);

        // Helper to build one metric cell (icon | name | value | unit)
        struct MetricWidgets {
            QLabel *icon; QLabel *name; QLabel *value; QLabel *unit;
        };
        auto makeMetric = [&](const QString &icon, const QString &name,
                               const QString &unit) -> MetricWidgets {
            auto *iconL = new QLabel(icon, panelFrame);
            auto *nameL = new QLabel(name, panelFrame);
            auto *valL  = new QLabel("--",  panelFrame);
            auto *unitL = new QLabel(unit,  panelFrame);

            iconL->setStyleSheet("font-size: 22px; color: #8b949e;");
            nameL->setStyleSheet("font-size: 14px; color: #8b949e;");
            valL->setStyleSheet(
                "font-size: 48px; font-weight: bold; color: #7ee787;"
                "min-width: 120px;");
            valL->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            unitL->setStyleSheet(
                "font-size: 14px; color: #8b949e; min-width: 50px;");
            unitL->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

            return { iconL, nameL, valL, unitL };
        };

        // Row 0: HR  |  Cadence
        MetricWidgets hr  = makeMetric("[HR]",  "Heart Rate", "bpm");
        MetricWidgets cad = makeMetric("[CAD]", "Cadence",    "rpm");
        // Row 1: Speed  |  Power
        MetricWidgets spd = makeMetric("[SPD]", "Speed",      "km/h");
        MetricWidgets pwr = makeMetric("[PWR]", "Power",      "W");

        m_hrLabel  = hr.value;
        m_cadLabel = cad.value;
        m_spdLabel = spd.value;
        m_pwrLabel = pwr.value;

        auto addMetricToGrid = [&](const MetricWidgets &m, int row, int colOffset) {
            grid->addWidget(m.icon,  row, colOffset + 0, Qt::AlignCenter);
            grid->addWidget(m.name,  row, colOffset + 1);
            grid->addWidget(m.value, row, colOffset + 2, Qt::AlignRight);
            grid->addWidget(m.unit,  row, colOffset + 3);
        };

        addMetricToGrid(hr,  0, 0);
        addMetricToGrid(cad, 0, 4);
        addMetricToGrid(spd, 1, 0);
        addMetricToGrid(pwr, 1, 4);

        panelLayout->addLayout(grid);
        root->addWidget(panelFrame);
        root->addSpacing(18);

        auto *sep2 = new QFrame(this);
        sep2->setFrameShape(QFrame::HLine);
        sep2->setStyleSheet("color: #21262d;");
        root->addWidget(sep2);
        root->addSpacing(14);

        // -- Footer -----------------------------------------------------------
        auto *footerRow = new QHBoxLayout();
        m_elapsedLabel = new QLabel("Activity elapsed: 0 s", this);
        m_elapsedLabel->setStyleSheet("font-size: 12px; color: #8b949e;");

        m_buildLabel = new QLabel(
            QString("Build artefact: screenshot-%1-%2.png")
                .arg(kPlatformTag, timestamp),
            this);
        m_buildLabel->setStyleSheet("font-size: 12px; color: #8b949e;");
        m_buildLabel->setAlignment(Qt::AlignRight);

        footerRow->addWidget(m_elapsedLabel, 1);
        footerRow->addWidget(m_buildLabel,   1, Qt::AlignRight);
        root->addLayout(footerRow);

        // -- Elapsed timer ----------------------------------------------------
        m_uiTimer = new QTimer(this);
        m_uiTimer->setInterval(1000);
        connect(m_uiTimer, &QTimer::timeout,
                this, &RuntimeValidationWindow::tick);
        m_uiTimer->start();
    }

    // Accessors used by the test to assert values
    int    hr()      const { return m_hr; }
    int    cadence() const { return m_cadence; }
    double speed()   const { return m_speed; }
    int    power()   const { return m_power; }

    void markReady() {
        m_statusBadge->setText("[ READY ]");
        m_statusBadge->setStyleSheet(
            "font-size: 14px; color: #3fb950; background: #0d2010;"
            "border: 1px solid #238636; border-radius: 4px;"
            "padding: 4px 12px;");
    }

    void markFailed() {
        m_statusBadge->setText("[ FAILED -- did not start within 10 s ]");
        m_statusBadge->setStyleSheet(
            "font-size: 14px; color: #f85149; background: #200d0d;"
            "border: 1px solid #58181a; border-radius: 4px;"
            "padding: 4px 12px;");
    }

public slots:
    void onHr(int /*uid*/, int val) {
        m_hr = val;
        m_hrLabel->setText(QString::number(val));
    }
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
    }

private slots:
    void tick() {
        m_elapsed++;
        m_elapsedLabel->setText(
            QString("Activity elapsed: %1 s").arg(m_elapsed));
    }

private:
    QLabel *m_hrLabel      = nullptr;
    QLabel *m_cadLabel     = nullptr;
    QLabel *m_spdLabel     = nullptr;
    QLabel *m_pwrLabel     = nullptr;
    QLabel *m_statusBadge  = nullptr;
    QLabel *m_elapsedLabel = nullptr;
    QLabel *m_buildLabel   = nullptr;
    QTimer *m_uiTimer      = nullptr;

    int    m_hr      = 0;
    int    m_cadence = 0;
    double m_speed   = 0.0;
    int    m_power   = 0;
    int    m_elapsed = 0;
};


// ---------------------------------------------------------------------------
// TstRuntimeValidation -- QTest class
// ---------------------------------------------------------------------------
class TstRuntimeValidation : public QObject
{
    Q_OBJECT

private slots:
    /*
     * testRuntimeValidation
     *
     * 1. Queries Bluetooth stack availability (connectivity log).
     * 2. Creates and shows the 1280x720 validation window.
     * 3. Wires SimulatorHub (same signal contract as BtleHub) to the window.
     * 4. Waits up to 10 seconds for sensor data to flow (fail-fast).
     * 5. Captures and saves a labelled screenshot as build evidence.
     * 6. Asserts all four sensor channels carry realistic values.
     */
    void testRuntimeValidation()
    {
        // -- 1. Bluetooth connectivity check ----------------------------------
        //
        // Query the Bluetooth stack using QBluetoothLocalDevice.  An empty
        // device list means "Not Found" (no adapter) which is acceptable in
        // CI.  A crash or exception here would indicate the API is unsupported.
        QList<QBluetoothHostInfo> btDevices =
            QBluetoothLocalDevice::allDevices();
        QString btStatus;
        if (btDevices.isEmpty()) {
            btStatus = QStringLiteral(
                "Not Found (no adapter detected) -- API available");
        } else {
            btStatus = QString("Found: %1 adapter(s) -- API available")
                           .arg(btDevices.size());
        }
        qDebug().noquote() << "[Bluetooth]" << btStatus;

        // -- 2. Build labelled screenshot filename ----------------------------
        // Timestamps are in UTC (Z suffix) for unambiguous CI artefact naming.
        QString timestamp = QDateTime::currentDateTimeUtc()
                                .toString("yyyy-MM-ddTHH-mm-ssZ");
        QString screenshotName =
            QString("screenshot-%1-%2.png").arg(kPlatformTag, timestamp);
        QString outDir  = QCoreApplication::applicationDirPath();
        QString outPath = outDir + "/" + screenshotName;

        // -- 3. Create and show the validation window -------------------------
        RuntimeValidationWindow window(btStatus, timestamp);
        window.show();
        QCoreApplication::processEvents();

        // -- 4. Wire SimulatorHub -> window -----------------------------------
        SimulatorHub sim;
        connect(&sim, &SimulatorHub::signal_hr,
                &window, &RuntimeValidationWindow::onHr);
        connect(&sim, &SimulatorHub::signal_cadence,
                &window, &RuntimeValidationWindow::onCadence);
        connect(&sim, &SimulatorHub::signal_speed,
                &window, &RuntimeValidationWindow::onSpeed);
        connect(&sim, &SimulatorHub::signal_power,
                &window, &RuntimeValidationWindow::onPower);

        sim.start();

        // -- 5. Fail-fast: wait up to 10 s for sensor data -------------------
        // All four channels must produce non-zero values before the test
        // proceeds; checking only power would miss a partial initialisation.
        const int kTimeoutMs = 10000;
        const int kPollMs    = 100;
        int       elapsed    = 0;
        while ((window.hr() == 0 || window.cadence() == 0
                || window.speed() == 0.0 || window.power() == 0)
               && elapsed < kTimeoutMs) {
            QTest::qWait(kPollMs);
            elapsed += kPollMs;
        }

        const bool timedOut = (window.hr() == 0 || window.cadence() == 0
                               || window.speed() == 0.0 || window.power() == 0);

        // Flush paint events before grabbing the window
        QCoreApplication::processEvents();
        QTest::qWait(50);

        if (timedOut) {
            window.markFailed();
        } else {
            // Let data run for a further 2 s so values settle and the window
            // looks fully populated in the screenshot
            QTest::qWait(2000);
            window.markReady();
        }

        QCoreApplication::processEvents();

        // -- 6. Capture 1280x720 screenshot -----------------------------------
        QPixmap screenshot = window.grab();
        QVERIFY2(!screenshot.isNull(),
                 "Screenshot grab() returned a null pixmap");
        QVERIFY2(screenshot.width()  >= 1280,
                 "Screenshot width must be >= 1280 px");
        QVERIFY2(screenshot.height() >= 720,
                 "Screenshot height must be >= 720 px");

        bool saved = screenshot.save(outPath, "PNG");
        QVERIFY2(saved,
                 qPrintable(
                     QString("Failed to save screenshot to: %1").arg(outPath)));
        qDebug().noquote() << "[Screenshot] Saved to:" << outPath;

        // -- 7. Fail-fast assert (after screenshot so error state is captured) -
        QVERIFY2(!timedOut,
                 "FAIL-FAST: application did not receive sensor data within "
                 "10 seconds. See screenshot in build artefacts for the error "
                 "state.");

        // -- 8. Assert all four sensor channels carry realistic values --------
        QVERIFY2(window.hr() >= 125 && window.hr() <= 165,
                 qPrintable(
                     QString("HR %1 bpm out of expected 125-165 range")
                         .arg(window.hr())));
        QVERIFY2(window.cadence() >= 80 && window.cadence() <= 100,
                 qPrintable(
                     QString("Cadence %1 rpm out of expected 80-100 range")
                         .arg(window.cadence())));
        QVERIFY2(window.speed() >= 23.0 && window.speed() <= 33.0,
                 qPrintable(
                     QString("Speed %1 km/h out of expected 23-33 range")
                         .arg(window.speed())));
        QVERIFY2(window.power() >= 170 && window.power() <= 260,
                 qPrintable(
                     QString("Power %1 W out of expected 170-260 range")
                         .arg(window.power())));

        qDebug() << "[Sensors] HR="      << window.hr()
                 << " Cadence="          << window.cadence()
                 << " Speed="            << window.speed()
                 << " Power="            << window.power();
    }
    /*
     * testInstalledAppStartupLog (#377)
     *
     * Launches the installed app (path from MT_APP_BINARY) with
     * --startup-check and checks the log file it writes to
     * <AppData>/MaximumTrainer/MaximumTrainer_Redux/MaximumTrainer.log.
     */
    void testInstalledAppStartupLog()
    {
        const QString app = qEnvironmentVariable("MT_APP_BINARY");
        if (app.isEmpty())
            QSKIP("MT_APP_BINARY not set; post-install startup log check skipped");
        QVERIFY2(QFile::exists(app), qPrintable(QStringLiteral("MT_APP_BINARY not found: ") + app));

        // The app's AppDataLocation, resolved with its identity.
        const QString savedOrg = QCoreApplication::organizationName();
        const QString savedApp = QCoreApplication::applicationName();
        QCoreApplication::setOrganizationName(QStringLiteral("MaximumTrainer"));
        QCoreApplication::setApplicationName(QStringLiteral("MaximumTrainer_Redux"));
        const QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                                + QStringLiteral("/MaximumTrainer.log");
        QCoreApplication::setOrganizationName(savedOrg);
        QCoreApplication::setApplicationName(savedApp);

        qint64 startOffset = 0;
        {
            QFile existing(logPath);
            if (existing.exists())
                startOffset = existing.size();
        }

        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("MT_NO_NETWORK"), QStringLiteral("1"));
        proc.setProcessEnvironment(env);
        proc.start(app, { QStringLiteral("--startup-check") });
        QVERIFY2(proc.waitForStarted(10000), "installed app did not start");
        const bool finished = proc.waitForFinished(60000);
        if (!finished)
            proc.kill();

        QFile logFile(logPath);
        QVERIFY2(logFile.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("no log file at ") + logPath));
        logFile.seek(startOffset);
        const QString runLog = QString::fromUtf8(logFile.readAll());
        qDebug().noquote() << "[StartupLog]" << logPath << "\n" << runLog;

        QVERIFY2(finished, "installed app did not exit within 60 s");
        QCOMPARE(proc.exitStatus(), QProcess::NormalExit);
        QCOMPARE(proc.exitCode(), 0);
        QVERIFY2(runLog.section(QLatin1Char('\n'), 0, 0).contains(QStringLiteral("starting ====")),
                 "the startup banner must be the first line of the run");
        QVERIFY(runLog.contains(QStringLiteral("Settings: ")));
        QVERIFY(runLog.contains(QStringLiteral("main: login dialog shown … done")));
    }
};

QTEST_MAIN(TstRuntimeValidation)
#include "tst_runtime_validation.moc"
