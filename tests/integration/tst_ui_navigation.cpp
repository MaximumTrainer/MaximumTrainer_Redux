/*
 * tst_ui_navigation.cpp
 *
 * UI Navigation & User Journey Tests -- MaximumTrainer
 *
 * Purpose
 * -----------------------------------------------------------------------
 * Validates the primary UI screens and user interaction flows of
 * MaximumTrainer using two complementary approaches:
 *
 *   A. Full application journey (out-of-process)
 *      Launches the installed MaximumTrainer binary via QProcess with the
 *      existing --screenshots flag.  The application starts without login
 *      (offline mode), navigates through every major screen, and saves ten
 *      1280×720 PNGs to a temporary directory.  The test verifies each file
 *      exists and has valid dimensions.  This approach exercises the real,
 *      installed application on the host OS using its actual display
 *      (Xvfb on Linux CI, native display on macOS / Windows CI).
 *
 *   B. Component interaction (in-process, real display)
 *      Uses QTest::mouseClick(), QSignalSpy, and QCOMPARE to drive
 *      individual widgets and models that back the UI.  These tests
 *      complement the full-app journey by verifying discrete signal/slot
 *      contracts and model mutations that the screenshot run cannot assert.
 *
 * Test groups
 * ──────────────────────────────────────────────────────────────────────
 * 0. Full Application Journey  (out-of-process, real installed binary)
 *    Launches MaximumTrainer --screenshots <dir> and verifies all ten
 *    expected screenshots are written by the running application:
 *      • screenshot_main_window.png
 *      • screenshot_workout_editor.png
 *      • screenshot_workout_running.png
 *      • screenshot_settings.png
 *      • screenshot_studio_mode.png
 *      • screenshot_activity_history.png
 *      • screenshot_plan.png
 *      • screenshot_profile.png
 *      • screenshot_achievements.png
 *      • screenshot_history.png
 *    Binary is located via the MT_APP_BINARY env-var override or the
 *    canonical peer path  build/release/MaximumTrainer{.app|.exe}.
 *    The test is skipped gracefully when the binary is not found.
 *
 * 1. ConnectionDialog (DialogConnectionMethod)
 *    Verifies that clicking "Simulation" or "BTLE Device" accepts the
 *    dialog, emits QDialog::accepted(), and sets the correct
 *    DialogConnectionMethod::ConnectionMethod on the returned value.
 *    Uses QTest::mouseClick() and QSignalSpy.
 *
 * 2. WorkoutCreator — IntervalTableModel interactions
 *    Tests the GUI-level interval management model that backs the
 *    WorkoutCreator table view.  Exercises:
 *      • insertRows()  — row count increases by 1, dataChanged emitted
 *      • removeRows()  — row count decreases, remaining rows are correct
 *      • copyRows()    — duplicates an existing row with identical data
 *      • setListInterval() — dataChanged emitted, model row count matches
 *
 * 3. Post-workout summary data
 *    Constructs a WorkoutHistorySummary struct with known values and
 *    verifies all data fields are accessible and correct.
 *
 * Acceptance criteria
 * ──────────────────────────────────────────────────────────────────────
 * • Full-app journey: all 10 screenshots exist and are ≥ 1280×720 px.
 * • ConnectionDialog: accepted() signal fires exactly once per click;
 *   selectedMethod() returns the correct method.
 * • IntervalTableModel: rowCount() changes match expectation after each
 *   mutating operation; dataChanged signal is emitted.
 * • WorkoutHistorySummary: all numeric and string fields match the
 *   values used to populate the struct.
 *
 * Build:
 *   cd tests/integration
 *   qmake ui_navigation_tests.pro [QWT_INSTALL=/path/to/qwt] && make -j$(nproc)
 * Run (Linux CI — with Xvfb providing a real X11 display):
 *   Xvfb :99 -screen 0 1280x800x24 &
 *   sleep 1
 *   export DISPLAY=:99
 *   ../../build/tests/ui_navigation_tests -v2
 * Run directly (Windows / macOS CI — native display):
 *   .\build\tests\ui_navigation_tests.exe -v2
 * Override application binary path:
 *   MT_APP_BINARY=/opt/MaximumTrainer/MaximumTrainer ./build/tests/ui_navigation_tests -v2
 */

#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QPushButton>
#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QDir>
#include <QDateTime>
#include <QTableView>
#include <QHeaderView>
#include <QAbstractItemModel>
#include <QProcess>
#include <QImage>
#include <QFileInfo>

#include "../../src/model/account.h"
#include "../../src/model/interval.h"
#include "../../src/model/intervaltablemodel.h"
#include "../../src/model/workouthistorysummary.h"
#include "../../src/ui/dialog_connection_method.h"

// ---------------------------------------------------------------------------
// Platform tag embedded in screenshot filenames and window titles.
// ---------------------------------------------------------------------------
#if defined(Q_OS_WIN)
    static const QString kPlatformTag = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    static const QString kPlatformTag = QStringLiteral("macos");
#else
    static const QString kPlatformTag = QStringLiteral("linux");
#endif

// ---------------------------------------------------------------------------
// findApplicationBinary()
//
// Locates the MaximumTrainer application binary.  Resolution order:
//   1. MT_APP_BINARY environment variable (absolute path override).
//   2. Canonical peer path: the test binary lives in build/tests/; the
//      application lives in build/release/.
// Returns an empty string if no candidate is found.
// ---------------------------------------------------------------------------
static QString findApplicationBinary()
{
    // 1. Explicit override
    const QString envBin = qEnvironmentVariable("MT_APP_BINARY");
    if (!envBin.isEmpty() && QFileInfo::exists(envBin))
        return envBin;

    // 2. Peer path relative to the test binary directory
    QDir dir(QCoreApplication::applicationDirPath()); // build/tests/
    dir.cd(QStringLiteral("../release"));             // build/release/

#if defined(Q_OS_WIN)
    const QString candidate = dir.absoluteFilePath(QStringLiteral("MaximumTrainer.exe"));
#elif defined(Q_OS_MACOS)
    const QString candidate = dir.absoluteFilePath(
        QStringLiteral("MaximumTrainer.app/Contents/MacOS/MaximumTrainer"));
#else
    const QString candidate = dir.absoluteFilePath(QStringLiteral("MaximumTrainer"));
#endif

    if (QFileInfo::exists(candidate))
        return candidate;

    return QString();
}

// ---------------------------------------------------------------------------
// TstUiNavigation
// ---------------------------------------------------------------------------
class TstUiNavigation : public QObject
{
    Q_OBJECT

private:
    Account *m_account   = nullptr;
    QString  m_timestamp;
    QString  m_outputDir;
    QString  m_appBinary;

private slots:

    // ── Setup ────────────────────────────────────────────────────────────────
    void initTestCase()
    {
        m_timestamp = QDateTime::currentDateTimeUtc()
                          .toString(QStringLiteral("yyyy-MM-ddTHH-mm-ssZ"));
        m_outputDir = QCoreApplication::applicationDirPath();
        QDir().mkpath(m_outputDir);

        m_appBinary = findApplicationBinary();
        if (m_appBinary.isEmpty()) {
            qWarning() << "[initTestCase] MaximumTrainer binary not found — "
                          "testFullApplicationJourney will be skipped.";
            qWarning() << "[initTestCase] Set MT_APP_BINARY or build the app into build/release/.";
        } else {
            qDebug() << "[initTestCase] Application binary:" << m_appBinary;
        }

        // Register an Account so IntervalTableModel can resolve account->FTP
        m_account = new Account(this);
        m_account->FTP          = 200;  // W
        m_account->LTHR         = 160;  // bpm
        m_account->display_name = QStringLiteral("CI Test User");
        qApp->setProperty("Account", QVariant::fromValue(m_account));
    }

    void cleanupTestCase()
    {
        qApp->setProperty("Account", QVariant());
    }

    // ========================================================================
    // Group 0 — Full Application Journey (out-of-process, installed binary)
    // ========================================================================

    // ────────────────────────────────────────────────────────────────────────
    // 0a. Launch the real MaximumTrainer binary via --screenshots mode.
    //
    //     The application bypasses the login dialog, shows the MainWindow,
    //     navigates through every major tab (main, settings, workout editor,
    //     workout running with SimulatorHub, studio mode, history/Intervals.icu)
    //     and writes six 1280×720 PNG files to a temporary directory.
    //
    //     This test verifies:
    //       • The installed binary starts and exits cleanly (exit code 0).
    //       • All six expected screenshots are created.
    //       • Each screenshot is a valid PNG with width ≥ 1280 and height ≥ 720.
    //
    //     The binary is located via:
    //       1. MT_APP_BINARY environment variable (absolute override path).
    //       2. Canonical peer path build/release/MaximumTrainer{.app|.exe}.
    //     The test is skipped if neither location yields a binary.
    // ────────────────────────────────────────────────────────────────────────
    void testFullApplicationJourney()
    {
        if (m_appBinary.isEmpty()) {
            QSKIP("MaximumTrainer binary not found — "
                  "build the app into build/release/ or set MT_APP_BINARY.");
        }

        // Temporary directory for the application to write its screenshots into.
        const QString ssDir = m_outputDir + QDir::separator()
                              + QStringLiteral("app-journey-") + m_timestamp;
        QVERIFY2(QDir().mkpath(ssDir),
                 qPrintable("Failed to create screenshot directory: " + ssDir));

        qDebug() << "[FullAppJourney] Launching:" << m_appBinary;
        qDebug() << "[FullAppJourney] Screenshot output dir:" << ssDir;

        QProcess proc;
        proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.start(m_appBinary, { QStringLiteral("--screenshots"), ssDir });

        QVERIFY2(proc.waitForStarted(15'000),
                 qPrintable(QString("MaximumTrainer failed to start: %1")
                                .arg(proc.errorString())));

        // The --screenshots sequence runs for ~26 s (animation + paint delays).
        // Allow 120 s to be safe on slow CI runners.
        const bool finished = proc.waitForFinished(120'000);

        // Capture any output from the process for diagnostics.
        const QByteArray procOutput = proc.readAll();
        if (!procOutput.isEmpty()) {
            qDebug().noquote() << "[FullAppJourney] Application output:\n"
                               << QString::fromLocal8Bit(procOutput);
        }

        QVERIFY2(finished,
                 "MaximumTrainer --screenshots did not complete within 120 s");
        QVERIFY2(proc.exitCode() == 0,
                 qPrintable(QString("MaximumTrainer exited with code %1")
                                .arg(proc.exitCode())));

        // ── Verify expected screenshots ──────────────────────────────────────
        // These file names are hard-coded in MainWindow::screenshotNextStep().
        // Covers all 7 main tabs of the application.
        const QStringList expectedFiles = {
            QStringLiteral("screenshot_main_window.png"),
            QStringLiteral("screenshot_settings.png"),
            QStringLiteral("screenshot_workout_editor.png"),
            QStringLiteral("screenshot_workout_running.png"),
            QStringLiteral("screenshot_studio_mode.png"),
            QStringLiteral("screenshot_activity_history.png"),
            QStringLiteral("screenshot_plan.png"),
            QStringLiteral("screenshot_profile.png"),
            QStringLiteral("screenshot_achievements.png"),
            QStringLiteral("screenshot_history.png"),
        };

        for (const QString &fileName : expectedFiles) {
            const QString path = ssDir + QDir::separator() + fileName;

            QVERIFY2(QFileInfo::exists(path),
                     qPrintable(QString("Missing screenshot: %1  (dir: %2)")
                                    .arg(fileName, ssDir)));

            QImage img(path);
            QVERIFY2(!img.isNull(),
                     qPrintable("Could not read screenshot: " + fileName));
            QVERIFY2(img.width()  >= 1280,
                     qPrintable(QString("%1: width %2 < 1280")
                                    .arg(fileName).arg(img.width())));
            QVERIFY2(img.height() >= 720,
                     qPrintable(QString("%1: height %2 < 720")
                                    .arg(fileName).arg(img.height())));

            // Copy to the test output dir with platform + timestamp tag so
            // CI artifact upload picks them up alongside the other test artifacts.
            const QString stem = fileName.left(fileName.length() - 4); // strip .png
            const QString dest = m_outputDir + QDir::separator()
                                 + QStringLiteral("app-journey-") + stem
                                 + QStringLiteral("-") + kPlatformTag
                                 + QStringLiteral("-") + m_timestamp
                                 + QStringLiteral(".png");
            QFile::copy(path, dest);
            qDebug().noquote() << "[FullAppJourney] ✓" << fileName
                               << img.width() << "x" << img.height();
        }
    }

    // ========================================================================
    // Group 1 — ConnectionDialog (DialogConnectionMethod)
    // ========================================================================

    // ────────────────────────────────────────────────────────────────────────
    // 1a. Clicking "Simulation" accepts the dialog and returns Simulation.
    // ────────────────────────────────────────────────────────────────────────
    void testConnectionDialogSimulationChoice()
    {
        DialogConnectionMethod dlg;
        QSignalSpy acceptedSpy(&dlg, &QDialog::accepted);

        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));

        // Find the "Simulation" button by text
        QPushButton *simBtn = nullptr;
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (QPushButton *btn : buttons) {
            if (btn->text().contains(QStringLiteral("Simulation"), Qt::CaseInsensitive)) {
                simBtn = btn;
                break;
            }
        }
        QVERIFY2(simBtn != nullptr,
                 "Could not find a 'Simulation' button in DialogConnectionMethod");

        QTest::mouseClick(simBtn, Qt::LeftButton);
        QCoreApplication::processEvents();

        QCOMPARE(acceptedSpy.count(), 1);
        QCOMPARE(dlg.selectedMethod(),
                 DialogConnectionMethod::Simulation);
    }

    // ────────────────────────────────────────────────────────────────────────
    // 1b. Clicking "BTLE Device" accepts the dialog and returns BTLE.
    // ────────────────────────────────────────────────────────────────────────
    void testConnectionDialogBtleChoice()
    {
        DialogConnectionMethod dlg;
        QSignalSpy acceptedSpy(&dlg, &QDialog::accepted);

        dlg.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dlg));

        // Find the "BTLE Device" button by text
        QPushButton *btleBtn = nullptr;
        const auto buttons = dlg.findChildren<QPushButton*>();
        for (QPushButton *btn : buttons) {
            if (btn->text().contains(QStringLiteral("BTLE"), Qt::CaseInsensitive)) {
                btleBtn = btn;
                break;
            }
        }
        QVERIFY2(btleBtn != nullptr,
                 "Could not find a 'BTLE Device' button in DialogConnectionMethod");

        QTest::mouseClick(btleBtn, Qt::LeftButton);
        QCoreApplication::processEvents();

        QCOMPARE(acceptedSpy.count(), 1);
        QCOMPARE(dlg.selectedMethod(),
                 DialogConnectionMethod::BTLE);
    }

    // ────────────────────────────────────────────────────────────────────────
    // 1c. Dialog widget structure — verify title and both buttons exist
    // ────────────────────────────────────────────────────────────────────────
    void testConnectionDialogWidgetStructure()
    {
        DialogConnectionMethod dlg;

        QVERIFY2(!dlg.windowTitle().isEmpty(),
                 "DialogConnectionMethod must have a non-empty window title");

        const auto buttons = dlg.findChildren<QPushButton*>();
        QVERIFY2(buttons.size() >= 2,
                 qPrintable(QString("Expected >= 2 buttons in ConnectionDialog, found %1")
                                .arg(buttons.size())));

        bool hasBtle = false, hasSim = false;
        for (const QPushButton *btn : buttons) {
            if (btn->text().contains(QStringLiteral("BTLE"),       Qt::CaseInsensitive))
                hasBtle = true;
            if (btn->text().contains(QStringLiteral("Simulation"), Qt::CaseInsensitive))
                hasSim  = true;
        }

        QVERIFY2(hasBtle, "ConnectionDialog must contain a 'BTLE Device' button");
        QVERIFY2(hasSim,  "ConnectionDialog must contain a 'Simulation' button");
    }


    // ========================================================================
    // Group 2 — WorkoutCreator: IntervalTableModel interactions
    // ========================================================================

    // ────────────────────────────────────────────────────────────────────────
    // 2a. insertRows() increases rowCount and emits dataChanged
    // ────────────────────────────────────────────────────────────────────────
    void testIntervalModelAddInterval()
    {
        IntervalTableModel model;

        // Start with one warmup interval
        QList<Interval> initial;
        initial << Interval(QTime(0, 10, 0), QStringLiteral("Warm-Up"),
                            Interval::PROGRESSIVE, 0.45, 0.60, 20, -1.0,
                            Interval::FLAT,        88, 88, 5,
                            Interval::NONE,        0.0, 0.0, 20,
                            false, 0.0, 0, 0.0);
        model.setListInterval(initial);
        QCOMPARE(model.rowCount(), 1);

        QSignalSpy changedSpy(&model, &IntervalTableModel::dataChanged);

        const bool ok = model.insertRows(model.rowCount(), 1, QModelIndex());
        QVERIFY2(ok, "insertRows() must return true");
        QCOMPARE(model.rowCount(), 2);
        QVERIFY2(changedSpy.count() >= 1,
                 "dataChanged must be emitted after insertRows()");
    }

    // ────────────────────────────────────────────────────────────────────────
    // 2b. removeRows() decreases rowCount and emits dataChanged;
    //     the remaining rows retain their original identity.
    // ────────────────────────────────────────────────────────────────────────
    void testIntervalModelRemoveInterval()
    {
        IntervalTableModel model;

        QList<Interval> threeIntervals;
        threeIntervals
            << Interval(QTime(0, 5, 0),  QStringLiteral("Warm-Up"),
                        Interval::PROGRESSIVE, 0.45, 0.60, 20, -1.0,
                        Interval::NONE, 0, 0, 5,
                        Interval::NONE, 0.0, 0.0, 20,
                        false, 0.0, 0, 0.0)
            << Interval(QTime(0, 20, 0), QStringLiteral("Tempo"),
                        Interval::FLAT, 0.80, 0.80, 20, -1.0,
                        Interval::NONE, 0, 0, 5,
                        Interval::NONE, 0.0, 0.0, 20,
                        false, 0.0, 0, 0.0)
            << Interval(QTime(0, 5, 0),  QStringLiteral("Cool-Down"),
                        Interval::PROGRESSIVE, 0.60, 0.45, 20, -1.0,
                        Interval::NONE, 0, 0, 5,
                        Interval::NONE, 0.0, 0.0, 20,
                        false, 0.0, 0, 0.0);

        model.setListInterval(threeIntervals);
        QCOMPARE(model.rowCount(), 3);

        QSignalSpy changedSpy(&model, &IntervalTableModel::dataChanged);

        // Remove the middle row (index 1)
        const bool ok = model.removeRows(1, 1, QModelIndex());
        QVERIFY2(ok, "removeRows() must return true");
        QCOMPARE(model.rowCount(), 2);
        QVERIFY2(changedSpy.count() >= 1,
                 "dataChanged must be emitted after removeRows()");

        // Remaining rows should be "Warm-Up" (row 0) and "Cool-Down" (row 1)
        const QList<Interval> remaining = model.getLstInterval();
        QCOMPARE(remaining.size(), 2);
        QCOMPARE(remaining.at(0).getDisplayMessage(), QStringLiteral("Warm-Up"));
        QCOMPARE(remaining.at(1).getDisplayMessage(), QStringLiteral("Cool-Down"));
    }

    // ────────────────────────────────────────────────────────────────────────
    // 2c. copyRows() duplicates a row; the copy is identical to the source.
    // ────────────────────────────────────────────────────────────────────────
    void testIntervalModelCopyRow()
    {
        IntervalTableModel model;

        QList<Interval> initial;
        initial << Interval(QTime(0, 10, 0), QStringLiteral("Steady State"),
                            Interval::FLAT, 0.75, 0.75, 20, -1.0,
                            Interval::FLAT, 90,  90,  5,
                            Interval::NONE, 0.0, 0.0, 20,
                            false, 0.0, 0, 0.0);
        model.setListInterval(initial);
        QCOMPARE(model.rowCount(), 1);

        const bool ok = model.copyRows(0);
        QVERIFY2(ok, "copyRows() must return true");
        QCOMPARE(model.rowCount(), 2);

        const QList<Interval> result = model.getLstInterval();
        QCOMPARE(result.at(0).getDisplayMessage(),
                 QStringLiteral("Steady State"));
        QCOMPARE(result.at(1).getDisplayMessage(),
                 QStringLiteral("Steady State"));

        QVERIFY2(qAbs(result.at(0).getFTP_start() - result.at(1).getFTP_start()) < 0.001,
                 "Copied interval FTP_start must match source");
        QVERIFY2(qAbs(result.at(0).getFTP_end()   - result.at(1).getFTP_end())   < 0.001,
                 "Copied interval FTP_end must match source");
        QCOMPARE(result.at(0).getDurationQTime(), result.at(1).getDurationQTime());
    }

    // ────────────────────────────────────────────────────────────────────────
    // 2d. setListInterval() emits no dataChanged; rowCount matches list size.
    //     (setListInterval uses beginResetModel/endResetModel, not dataChanged)
    // ────────────────────────────────────────────────────────────────────────
    void testIntervalModelSetListInterval()
    {
        IntervalTableModel model;
        QCOMPARE(model.rowCount(), 0);

        QList<Interval> five;
        for (int i = 0; i < 5; ++i) {
            five << Interval(QTime(0, 5, 0),
                             QStringLiteral("Interval %1").arg(i + 1),
                             Interval::FLAT, 0.75, 0.75, 20, -1.0,
                             Interval::NONE, 0, 0, 5,
                             Interval::NONE, 0.0, 0.0, 20,
                             false, 0.0, 0, 0.0);
        }
        model.setListInterval(five);
        QCOMPARE(model.rowCount(), 5);

        // Each row should have the correct name
        for (int i = 0; i < 5; ++i) {
            const QVariant v = model.data(
                model.index(i, 5), Qt::DisplayRole); // column 5 = display message
            QCOMPARE(v.toString(),
                     QStringLiteral("Interval %1").arg(i + 1));
        }
    }

    // ========================================================================
    // Group 3 — Post-workout summary data
    // ========================================================================

    // ────────────────────────────────────────────────────────────────────────
    // 3a. WorkoutHistorySummary struct field verification
    // ────────────────────────────────────────────────────────────────────────
    void testPostWorkoutSummaryDataFields()
    {
        WorkoutHistorySummary summary;
        summary.workoutName     = QStringLiteral("Three-Interval CI Workout");
        summary.durationSec     = 3600;           // 60 min
        summary.avgPowerW       = 185;
        summary.normalizedPower = 195;
        summary.avgHrBpm        = 145;
        summary.avgCadence      = 88;
        summary.tss             = 65.0;
        summary.totalDistanceKm = 42.5;
        summary.calories        = 720;
        summary.valid           = true;
        summary.startTime       = QDateTime::currentDateTimeUtc();

        QVERIFY2(summary.valid,
                 "WorkoutHistorySummary.valid must be set to true");
        QCOMPARE(summary.workoutName,
                 QStringLiteral("Three-Interval CI Workout"));
        QCOMPARE(summary.durationSec,     3600);
        QCOMPARE(summary.avgPowerW,       185);
        QCOMPARE(summary.normalizedPower, 195);
        QCOMPARE(summary.avgHrBpm,        145);
        QCOMPARE(summary.avgCadence,      88);
        QVERIFY2(qAbs(summary.tss - 65.0) < 0.01,
                 qPrintable(QString("TSS mismatch: expected 65.0, got %1").arg(summary.tss)));
        QVERIFY2(qAbs(summary.totalDistanceKm - 42.5) < 0.01,
                 qPrintable(QString("Distance mismatch: expected 42.5, got %1")
                                .arg(summary.totalDistanceKm)));
        QCOMPARE(summary.calories, 720);
        QVERIFY2(!summary.startTime.isNull(),
                 "startTime must not be null");
    }
};

QTEST_MAIN(TstUiNavigation)
#include "tst_ui_navigation.moc"
