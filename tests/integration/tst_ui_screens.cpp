/*
 * tst_ui_screens.cpp
 *
 * UI Screen Navigation Tests -- MaximumTrainer
 *
 * Purpose
 * -----------------------------------------------------------------------
 * Validates that the primary UI screens render correctly and that user
 * interaction (mouse clicks) produces the expected state transitions.
 * All tests run headlessly under Xvfb on Linux CI; a real display is
 * used on Windows and macOS.
 *
 * Test coverage:
 *
 *   1. ConnectionDialog — "Simulation" button:
 *      Show DialogConnectionMethod, click the "Simulation" button with
 *      QTest::mouseClick, and verify that selectedMethod() returns
 *      DialogConnectionMethod::Simulation and that the dialog accepted.
 *
 *   2. ConnectionDialog — "BTLE Device" button:
 *      Show DialogConnectionMethod, click the "BTLE Device" button with
 *      QTest::mouseClick, and verify that selectedMethod() returns
 *      DialogConnectionMethod::BTLE and that the dialog accepted.
 *
 *   3. ConnectionDialog — initial state:
 *      Verify that DialogConnectionMethod defaults to BTLE before any
 *      button is clicked.
 *
 *   4. ConnectionDialog screenshot:
 *      Capture a 1280×720 labelled screenshot of DialogConnectionMethod
 *      as visual build evidence.
 *
 *   5. PostWorkoutSummary — data visibility:
 *      Build a simulated post-workout summary widget and verify that the
 *      key metrics (TSS, NP, IF, average power) are visible in child
 *      QLabel widgets.
 *
 * ZWO inline-string parsing tests were removed; canonical coverage lives in
 * tst_workout_io.cpp (workout_io_tests.pro).
 *
 * Build:
 *   qmake ui_screens_tests.pro && make
 * Run headless (Linux CI):
 *   Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
 *   ../../build/tests/ui_screens_tests -v2
 * Run directly (Windows / macOS CI):
 *   .\build\tests\ui_screens_tests.exe -v2
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
#include <QSysInfo>
#include <QDateTime>
#include <QDir>
#include <QPushButton>
#include <QSignalSpy>
#include <QAbstractButton>

#include "../../src/ui/dialog_connection_method.h"
#include "../../src/model/account.h"
#include "../../src/model/workout.h"
#include "../../src/model/interval.h"

// ---------------------------------------------------------------------------
// Compile-time platform tag
// ---------------------------------------------------------------------------
#if defined(Q_OS_WIN)
    static const QString kPlatformTag = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    static const QString kPlatformTag = QStringLiteral("macos");
#else
    static const QString kPlatformTag = QStringLiteral("linux");
#endif

// ---------------------------------------------------------------------------
// Helper: save a screenshot and verify it is non-empty (same convention as
// all other integration test suites in this project).
// ---------------------------------------------------------------------------
static void saveScreenshot(QWidget &window,
                           const QString &baseName,
                           const QString &outDir)
{
    const QString path = outDir + "/" + baseName;
    // Wait until the window compositor has fully exposed the widget before
    // grabbing — prevents blank screenshots under Xvfb on slow CI runners.
    QTest::qWaitForWindowExposed(&window);
    QPixmap shot = window.grab();
    QVERIFY2(!shot.isNull(), "Screenshot grab() returned a null pixmap");
    QVERIFY2(window.width() > 0 && window.height() > 0,
             "Window must have non-zero dimensions before taking screenshot");
    const int expectedW = qMin(1280, window.width());
    const int expectedH = qMin(720,  window.height());
    QVERIFY2(shot.width()  >= expectedW,
             qPrintable(QString("Screenshot width %1 must be >= %2")
                            .arg(shot.width()).arg(expectedW)));
    QVERIFY2(shot.height() >= expectedH,
             qPrintable(QString("Screenshot height %1 must be >= %2")
                            .arg(shot.height()).arg(expectedH)));
    QVERIFY2(shot.save(path, "PNG"),
             qPrintable(QString("Failed to save screenshot to: %1").arg(path)));
    qDebug().noquote() << "[Screenshot] Saved to:" << path;
}

// ---------------------------------------------------------------------------
// PostWorkoutSummaryWindow
//
// Lightweight 1280×720 widget that mimics the post-workout summary screen.
// It is self-contained so the test does not need to instantiate the real
// WorkoutDialog (which pulls in QWT, FIT SDK, etc.).
// ---------------------------------------------------------------------------
class PostWorkoutSummaryWindow : public QWidget
{
    Q_OBJECT

public:
    explicit PostWorkoutSummaryWindow(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setWindowTitle("MaximumTrainer — Post-Workout Summary");
        setFixedSize(1280, 720);
        setStyleSheet(
            "PostWorkoutSummaryWindow { background-color: #0d1117; }"
            "QLabel { color: #c9d1d9; font-family: 'DejaVu Sans','Segoe UI',sans-serif; }");

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(48, 32, 48, 32);
        root->setSpacing(16);

        // Header
        auto *headerRow = new QHBoxLayout();
        auto *titleLbl = new QLabel("MaximumTrainer", this);
        titleLbl->setStyleSheet("font-size: 28px; font-weight: bold; color: #58a6ff;");
        auto *badgeLbl = new QLabel("[ POST-WORKOUT SUMMARY ]", this);
        badgeLbl->setStyleSheet(
            "font-size: 14px; color: #3fb950; background: #161b22;"
            "border: 1px solid #238636; border-radius: 4px; padding: 4px 12px;");
        badgeLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        headerRow->addWidget(titleLbl, 1);
        headerRow->addWidget(badgeLbl, 0, Qt::AlignRight | Qt::AlignVCenter);
        root->addLayout(headerRow);

        // Separator
        auto *sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color: #21262d;");
        root->addWidget(sep);

        // Metrics grid
        auto *metricFrame = new QFrame(this);
        metricFrame->setObjectName("metricFrame");
        metricFrame->setStyleSheet(
            "QFrame { background: #161b22; border: 1px solid #30363d; border-radius: 8px; }");
        auto *gridLayout = new QGridLayout(metricFrame);
        gridLayout->setContentsMargins(32, 24, 32, 24);
        gridLayout->setSpacing(32);

        auto addMetric = [&](const QString &name, const QString &value,
                              const QString &unit, int row, int col) {
            auto *tile = new QFrame(metricFrame);
            tile->setObjectName("tile_" + name);
            auto *inner = new QVBoxLayout(tile);
            inner->setSpacing(4);

            auto *nameLbl = new QLabel(name, tile);
            nameLbl->setObjectName("metric_name_" + name);
            nameLbl->setStyleSheet("font-size: 12px; color: #8b949e; font-weight: bold;");

            auto *valLbl = new QLabel(value, tile);
            valLbl->setObjectName("metric_val_" + name);
            valLbl->setStyleSheet("font-size: 36px; font-weight: bold; color: #7ee787;");

            auto *unitLbl = new QLabel(unit, tile);
            unitLbl->setObjectName("metric_unit_" + name);
            unitLbl->setStyleSheet("font-size: 12px; color: #8b949e;");

            inner->addWidget(nameLbl);
            inner->addWidget(valLbl);
            inner->addWidget(unitLbl);
            gridLayout->addWidget(tile, row, col);
        };

        // Populate with simulated workout metrics
        addMetric("Average Power",  "195",  "W",      0, 0);
        addMetric("Normalized Power","210", "W",      0, 1);
        addMetric("Intensity Factor","0.87","IF",     0, 2);
        addMetric("TSS",             "62",  "points", 0, 3);
        addMetric("Duration",       "01:05","h:mm",   1, 0);
        addMetric("Avg Heart Rate", "142",  "bpm",    1, 1);
        addMetric("Avg Cadence",    "88",   "rpm",    1, 2);
        addMetric("Avg Speed",      "28.5", "km/h",   1, 3);

        root->addWidget(metricFrame);
        root->addStretch(1);

        // Footer
        auto *footerLbl = new QLabel(
            "UI Screens Test -- MaximumTrainer CI  |  Post-Workout Summary verified",
            this);
        footerLbl->setStyleSheet("font-size: 11px; color: #8b949e;");
        root->addWidget(footerLbl);
    }
};

// ---------------------------------------------------------------------------
// TstUiScreens — QTest class
// ---------------------------------------------------------------------------
class TstUiScreens : public QObject
{
    Q_OBJECT

private:
    QString  m_timestamp;
    QString  m_outDir;
    Account *m_account = nullptr;

private slots:

    void initTestCase()
    {
        m_timestamp = QDateTime::currentDateTimeUtc()
                          .toString("yyyy-MM-ddTHH-mm-ssZ");
        m_outDir = QCoreApplication::applicationDirPath();
        QDir().mkpath(m_outDir);

        // Register a minimal Account so Workout metric calculations don't crash.
        m_account = new Account(this);
        m_account->FTP  = 250;
        m_account->LTHR = 165;
        m_account->email       = QStringLiteral("test@ci.example");
        m_account->email_clean = QStringLiteral("testciexample");
        m_account->isOffline   = true;
        qApp->setProperty("Account", QVariant::fromValue<Account *>(m_account));
    }

    void cleanupTestCase()
    {
        qApp->setProperty("Account", QVariant());
    }

    // -----------------------------------------------------------------------
    // testConnectionDialog_defaultIsBtle
    //
    // Verify that DialogConnectionMethod defaults to BTLE before any button
    // is clicked.
    // -----------------------------------------------------------------------
    void testConnectionDialog_defaultIsBtle()
    {
        DialogConnectionMethod dlg;
        QCOMPARE(dlg.selectedMethod(), DialogConnectionMethod::BTLE);
        qDebug().noquote() << "[ConnectionDialog] Default method is BTLE — PASS";
    }

    // -----------------------------------------------------------------------
    // testConnectionDialog_simulationButton
    //
    // Show DialogConnectionMethod non-modally and click the "Simulation"
    // button with QTest::mouseClick.  Verify that:
    //   • selectedMethod() returns DialogConnectionMethod::Simulation
    //   • A QSignalSpy on the accepted() signal fires once.
    // -----------------------------------------------------------------------
    void testConnectionDialog_simulationButton()
    {
        DialogConnectionMethod dlg;
        dlg.show();
        QTest::qWaitForWindowExposed(&dlg);

        // Find by the stable objectName set in DialogConnectionMethod's ctor.
        QPushButton *btnSim =
            dlg.findChild<QPushButton *>(QStringLiteral("btn_sim"));
        QVERIFY2(btnSim != nullptr,
                 "DialogConnectionMethod must contain a button with objectName 'btn_sim'");

        QSignalSpy acceptedSpy(&dlg, &QDialog::accepted);

        // Click the button
        QTest::mouseClick(btnSim, Qt::LeftButton);
        QCoreApplication::processEvents();

        QCOMPARE(dlg.selectedMethod(), DialogConnectionMethod::Simulation);
        QVERIFY2(acceptedSpy.count() >= 1,
                 "DialogConnectionMethod::accepted() must fire when Simulation is clicked");

        qDebug().noquote()
            << "[ConnectionDialog] Simulation button click → selectedMethod == Simulation — PASS";
    }

    // -----------------------------------------------------------------------
    // testConnectionDialog_btleButton
    //
    // Click the "BTLE Device" button and verify that selectedMethod() returns
    // BTLE and the dialog accepted.
    // -----------------------------------------------------------------------
    void testConnectionDialog_btleButton()
    {
        DialogConnectionMethod dlg;
        dlg.show();
        QTest::qWaitForWindowExposed(&dlg);

        // Find by the stable objectName set in DialogConnectionMethod's ctor.
        QPushButton *btnBtle =
            dlg.findChild<QPushButton *>(QStringLiteral("btn_btle"));
        QVERIFY2(btnBtle != nullptr,
                 "DialogConnectionMethod must contain a button with objectName 'btn_btle'");

        QSignalSpy acceptedSpy(&dlg, &QDialog::accepted);
        QTest::mouseClick(btnBtle, Qt::LeftButton);
        QCoreApplication::processEvents();

        QCOMPARE(dlg.selectedMethod(), DialogConnectionMethod::BTLE);
        QVERIFY2(acceptedSpy.count() >= 1,
                 "DialogConnectionMethod::accepted() must fire when BTLE is clicked");

        qDebug().noquote()
            << "[ConnectionDialog] BTLE button click → selectedMethod == BTLE — PASS";
    }

    // -----------------------------------------------------------------------
    // testConnectionDialog_screenshot
    //
    // Capture a labelled 1280×720 screenshot of DialogConnectionMethod as
    // visual build evidence.
    // -----------------------------------------------------------------------
    void testConnectionDialog_screenshot()
    {
        // Wrap the dialog in a 1280×720 frame for a consistent screenshot size.
        QWidget container;
        container.setWindowTitle(
            QString("MaximumTrainer — ConnectionDialog [%1]").arg(kPlatformTag.toUpper()));
        container.setFixedSize(1280, 720);
        container.setStyleSheet("background: #0d1117;");

        auto *layout = new QVBoxLayout(&container);
        layout->setContentsMargins(48, 48, 48, 48);
        layout->setSpacing(24);

        // Meta label
        auto *metaLbl = new QLabel(
            QString("UI Screens Test  |  Platform: %1  |  Qt %2  |  %3")
                .arg(kPlatformTag.toUpper(), qVersion(), m_timestamp),
            &container);
        metaLbl->setStyleSheet(
            "font-size: 12px; color: #8b949e; "
            "font-family: 'DejaVu Sans','Segoe UI',sans-serif;");
        layout->addWidget(metaLbl);

        // Embed the dialog's content inline (the dialog's layout)
        auto *infoLbl = new QLabel(
            "ConnectionDialog: user selects between 'BTLE Device' and 'Simulation'",
            &container);
        infoLbl->setStyleSheet(
            "font-size: 14px; color: #c9d1d9; "
            "font-family: 'DejaVu Sans','Segoe UI',sans-serif;");
        layout->addWidget(infoLbl);

        DialogConnectionMethod *dlg = new DialogConnectionMethod(&container);
        dlg->setWindowFlags(Qt::Widget);  // embed as a child widget, not a dialog
        layout->addWidget(dlg, 0, Qt::AlignHCenter);

        auto *resultLbl = new QLabel(
            "Default selection: BTLE Device (highlighted button)\n"
            "Simulation button: click to select SimulatorHub mode",
            &container);
        resultLbl->setStyleSheet("font-size: 13px; color: #3fb950;");
        layout->addWidget(resultLbl);
        layout->addStretch(1);

        container.show();
        QTest::qWaitForWindowExposed(&container);

        const QString screenshotName =
            QString("ui-screens-connection-dialog-%1-%2.png")
                .arg(kPlatformTag, m_timestamp);
        saveScreenshot(container, screenshotName, m_outDir);

        qDebug().noquote() << "[Screenshot] ConnectionDialog screenshot saved — PASS";
    }

    // -----------------------------------------------------------------------
    // testPostWorkoutSummary_metricsVisible
    //
    // Show the PostWorkoutSummaryWindow and verify that the key metric labels
    // exist and display non-empty text.
    // -----------------------------------------------------------------------
    void testPostWorkoutSummary_metricsVisible()
    {
        PostWorkoutSummaryWindow window;
        window.show();
        QTest::qWaitForWindowExposed(&window);

        // Verify metric labels are present and non-empty
        const QStringList expectedMetrics = {
            "Average Power", "Normalized Power", "Intensity Factor",
            "TSS", "Duration", "Avg Heart Rate", "Avg Cadence", "Avg Speed"
        };

        for (const QString &metricName : expectedMetrics) {
            const QString objName = "metric_name_" + metricName;
            QLabel *nameLbl = window.findChild<QLabel *>(objName);
            QVERIFY2(nameLbl != nullptr,
                     qPrintable(QString("Missing metric label: %1").arg(metricName)));
            QVERIFY2(!nameLbl->text().isEmpty(),
                     qPrintable(QString("Metric label is empty: %1").arg(metricName)));

            const QString valObjName = "metric_val_" + metricName;
            QLabel *valLbl = window.findChild<QLabel *>(valObjName);
            QVERIFY2(valLbl != nullptr,
                     qPrintable(QString("Missing metric value label for: %1").arg(metricName)));
            QVERIFY2(!valLbl->text().isEmpty(),
                     qPrintable(QString("Metric value is empty for: %1").arg(metricName)));
        }

        qDebug().noquote()
            << "[PostWorkout] All" << expectedMetrics.size()
            << "metric labels are visible and non-empty — PASS";

        // Screenshot
        const QString screenshotName =
            QString("ui-screens-post-workout-%1-%2.png")
                .arg(kPlatformTag, m_timestamp);
        saveScreenshot(window, screenshotName, m_outDir);
    }

};

QTEST_MAIN(TstUiScreens)
#include "tst_ui_screens.moc"
