/*
 * tst_btle_integration.cpp
 *
 * BTLE Integration Test – MaximumTrainer
 *
 * Purpose
 * ──────────────────────────────────────────────────────────────────────────
 * Demonstrates that the BTLE sensor layer works end-to-end:
 *
 *   1. SimulatorHub starts and emits HR / cadence / speed / power every 1 s.
 *   2. A Qt window renders the live values (the same data pipeline that feeds
 *      WorkoutDialog during a real activity).
 *   3. After 3 seconds the test grabs a screenshot, saves it as a PNG and
 *      asserts that all four channels carry realistic non-zero values.
 *
 * The screenshot is uploaded as a CI artifact and serves as visual evidence
 * of a "running activity" with Bluetooth sensor data flowing.
 *
 * Build:
 *   qmake btle_integration_tests.pro && make
 * Run headless (Linux CI):
 *   Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
 *   ../../build/tests/btle_integration_tests -v2
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

#include "../../src/btle/simulator_hub.h"

// ─────────────────────────────────────────────────────────────────────────────
// SensorDisplayWidget – shows live BTLE metrics from SimulatorHub
// ─────────────────────────────────────────────────────────────────────────────
class SensorDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SensorDisplayWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setWindowTitle("MaximumTrainer – BTLE Activity Running");
        setFixedSize(520, 380);

        // Dark background mimicking the workout screen
        setStyleSheet(
            "SensorDisplayWidget { background-color: #1c1c2e; }"
            "QLabel { color: #e8e8e8; font-family: 'DejaVu Sans', sans-serif; }"
        );

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(24, 20, 24, 20);
        root->setSpacing(10);

        // ── Title bar ────────────────────────────────────────────────────────
        auto *titleLabel = new QLabel("MaximumTrainer", this);
        titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #4fc3f7;");
        titleLabel->setAlignment(Qt::AlignCenter);
        root->addWidget(titleLabel);

        auto *subLabel = new QLabel("BTLE Activity Running", this);
        subLabel->setStyleSheet("font-size: 12px; color: #78909c;");
        subLabel->setAlignment(Qt::AlignCenter);
        root->addWidget(subLabel);

        // ── Separator ────────────────────────────────────────────────────────
        auto *sep1 = new QFrame(this);
        sep1->setFrameShape(QFrame::HLine);
        sep1->setStyleSheet("color: #2a2a4a;");
        root->addWidget(sep1);

        // ── Sensor status badge ───────────────────────────────────────────────
        m_statusLabel = new QLabel("⬤  Bluetooth Simulator: ACTIVE", this);
        m_statusLabel->setStyleSheet("font-size: 13px; color: #69f0ae; padding: 4px 0;");
        m_statusLabel->setAlignment(Qt::AlignCenter);
        root->addWidget(m_statusLabel);

        // ── Metrics grid ─────────────────────────────────────────────────────
        auto *grid = new QGridLayout();
        grid->setVerticalSpacing(16);
        grid->setHorizontalSpacing(20);

        auto makeRow = [&](int row, const QString &icon, const QString &name,
                           const QString &unit, QLabel **valueOut) {
            auto *icon_l  = new QLabel(icon, this);
            auto *name_l  = new QLabel(name, this);
            auto *val_l   = new QLabel("—", this);
            auto *unit_l  = new QLabel(unit, this);

            icon_l->setStyleSheet("font-size: 22px;");
            name_l->setStyleSheet("font-size: 14px; color: #90a4ae;");
            val_l->setStyleSheet("font-size: 32px; font-weight: bold; color: #a5d6a7; min-width: 80px;");
            val_l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            unit_l->setStyleSheet("font-size: 14px; color: #607d8b;");

            grid->addWidget(icon_l,  row, 0, Qt::AlignCenter);
            grid->addWidget(name_l,  row, 1);
            grid->addWidget(val_l,   row, 2, Qt::AlignRight);
            grid->addWidget(unit_l,  row, 3);

            *valueOut = val_l;
        };

        makeRow(0, "♥",  "Heart Rate",  "bpm",  &m_hrLabel);
        makeRow(1, "↺",  "Cadence",     "rpm",  &m_cadLabel);
        makeRow(2, "▶",  "Speed",       "km/h", &m_spdLabel);
        makeRow(3, "⚡", "Power",       "W",    &m_pwrLabel);

        root->addLayout(grid);

        // ── Separator ────────────────────────────────────────────────────────
        auto *sep2 = new QFrame(this);
        sep2->setFrameShape(QFrame::HLine);
        sep2->setStyleSheet("color: #2a2a4a;");
        root->addWidget(sep2);

        // ── Timer ─────────────────────────────────────────────────────────────
        m_elapsedLabel = new QLabel("Activity elapsed: 0 s", this);
        m_elapsedLabel->setStyleSheet("font-size: 12px; color: #78909c;");
        m_elapsedLabel->setAlignment(Qt::AlignCenter);
        root->addWidget(m_elapsedLabel);

        // UI clock (updates every second to show elapsed time)
        m_uiTimer = new QTimer(this);
        m_uiTimer->setInterval(1000);
        connect(m_uiTimer, &QTimer::timeout, this, &SensorDisplayWidget::tick);
        m_uiTimer->start();
    }

    // Accessors used by the test to assert values
    int    hr()      const { return m_hr; }
    int    cadence() const { return m_cadence; }
    double speed()   const { return m_speed; }
    int    power()   const { return m_power; }

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
        m_elapsedLabel->setText(QString("Activity elapsed: %1 s").arg(m_elapsed));
    }

private:
    QLabel *m_hrLabel      = nullptr;
    QLabel *m_cadLabel     = nullptr;
    QLabel *m_spdLabel     = nullptr;
    QLabel *m_pwrLabel     = nullptr;
    QLabel *m_statusLabel  = nullptr;
    QLabel *m_elapsedLabel = nullptr;
    QTimer *m_uiTimer      = nullptr;

    int    m_hr      = 0;
    int    m_cadence = 0;
    double m_speed   = 0.0;
    int    m_power   = 0;
    int    m_elapsed = 0;
};


// ─────────────────────────────────────────────────────────────────────────────
// TstBtleIntegration – QTest class
// ─────────────────────────────────────────────────────────────────────────────
class TstBtleIntegration : public QObject
{
    Q_OBJECT

private slots:
    /*
     * testBtleActivityRunning
     *
     * End-to-end integration test:
     *   - Starts SimulatorHub (same signals as BtleHub used in production)
     *   - Displays a live sensor window (same data path as WorkoutDialog)
     *   - Waits 3 seconds for 3 update cycles
     *   - Takes a screenshot and saves it as PNG evidence
     *   - Asserts all four channels carry realistic values
     */
    void testBtleActivityRunning()
    {
        // ── Create the live display window ───────────────────────────────────
        SensorDisplayWidget window;
        window.show();
        QCoreApplication::processEvents();

        // ── Wire SimulatorHub → display ──────────────────────────────────────
        SimulatorHub sim;
        connect(&sim, &SimulatorHub::signal_hr,      &window, &SensorDisplayWidget::onHr);
        connect(&sim, &SimulatorHub::signal_cadence, &window, &SensorDisplayWidget::onCadence);
        connect(&sim, &SimulatorHub::signal_speed,   &window, &SensorDisplayWidget::onSpeed);
        connect(&sim, &SimulatorHub::signal_power,   &window, &SensorDisplayWidget::onPower);

        // ── Start simulation and let it run for 3 ticks ──────────────────────
        sim.start();
        // Wait (event-driven) for 3+ hub ticks before asserting sensor values
        {
            QSignalSpy powerSpy(&sim, &SimulatorHub::signal_power);
            while (powerSpy.count() < 3)
                QVERIFY2(powerSpy.wait(3000),
                         "SimulatorHub did not emit signal_power within 3 s of start");
        }

        // ── Grab screenshot ──────────────────────────────────────────────────
        // Ensure all pending paint events are flushed
        QCoreApplication::processEvents();
        QTest::qWait(50);

        QPixmap screenshot = window.grab();
        QVERIFY2(!screenshot.isNull(), "Screenshot grab returned null pixmap");

        // Resolve output path relative to test binary location
        QString outDir = QCoreApplication::applicationDirPath();
        QString outPath = outDir + "/btle_integration_screenshot.png";
        bool saved = screenshot.save(outPath, "PNG");
        QVERIFY2(saved, qPrintable(QString("Failed to save screenshot to: %1").arg(outPath)));

        qDebug() << "Screenshot saved to:" << outPath;

        // ── Assert sensor data is flowing with realistic values ───────────────
        QVERIFY2(window.hr() >= 125 && window.hr() <= 165,
                 qPrintable(QString("HR %1 bpm out of expected 125–165 range").arg(window.hr())));

        QVERIFY2(window.cadence() >= 80 && window.cadence() <= 100,
                 qPrintable(QString("Cadence %1 rpm out of expected 80–100 range").arg(window.cadence())));

        QVERIFY2(window.speed() >= 23.0 && window.speed() <= 33.0,
                 qPrintable(QString("Speed %1 km/h out of expected 23–33 range").arg(window.speed())));

        QVERIFY2(window.power() >= 170 && window.power() <= 260,
                 qPrintable(QString("Power %1 W out of expected 170–260 range").arg(window.power())));

        qDebug() << "PASS: HR=" << window.hr()
                 << "Cadence=" << window.cadence()
                 << "Speed=" << window.speed()
                 << "Power=" << window.power();
    }

    // =========================================================================
    // testSimulatorHubStopSuppressesSignals
    //
    // Verifies that calling stopDecodingMsg() causes SimulatorHub to cease
    // emitting signals.  This models the sensor-disconnect flow used by
    // WorkoutDialog when a BLE device is disconnected mid-activity.
    // =========================================================================
    void testSimulatorHubStopSuppressesSignals()
    {
        SimulatorHub hub;
        hub.start();

        // Confirm signals are flowing — wait for at least 1 emission
        QSignalSpy startSpy(&hub, &SimulatorHub::signal_power);
        QVERIFY2(startSpy.wait(3000), "SimulatorHub did not emit any signal_power within 3 s");

        // Stop the hub
        hub.stopDecodingMsg();

        // Open a new spy AFTER stop.  QSignalSpy::wait() blocks the event loop
        // for up to 1.5 s.  If the hub has truly stopped, wait() times out and
        // returns false — which is the expected (passing) outcome.
        QSignalSpy afterStopSpy(&hub, &SimulatorHub::signal_power);
        const bool emittedAfterStop = afterStopSpy.wait(1500);
        QVERIFY2(!emittedAfterStop,
                 qPrintable(QString("signal_power emitted %1 time(s) after stopDecodingMsg() — "
                                    "hub should be silent")
                                .arg(afterStopSpy.count())));
    }

QTEST_MAIN(TstBtleIntegration)
#include "tst_btle_integration.moc"
