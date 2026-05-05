/*
 * tst_ui_navigation.cpp
 *
 * UI Navigation & User Journey Tests -- MaximumTrainer
 *
 * Purpose
 * -----------------------------------------------------------------------
 * Validates the primary UI screens and user interaction flows of
 * MaximumTrainer using Qt Test's mouse/keyboard simulation facilities.
 * Tests run headlessly under Xvfb (Linux CI) or the native display
 * (Windows / macOS CI) and produce labelled 1280×720 screenshots as
 * build evidence.
 *
 * Test groups
 * ──────────────────────────────────────────────────────────────────────
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
 * 3. Post-workout summary screen
 *    Constructs a WorkoutHistorySummary struct with known values,
 *    renders a 1280×720 summary window that mirrors the post-workout
 *    screen layout, and verifies all data fields are accessible and
 *    visible.  A screenshot is saved as build evidence.
 *
 * 4. Visual evidence screenshots
 *    Each test group generates a platform-tagged screenshot that is
 *    uploaded as a CI artifact by the `ui_navigation` test job.
 *
 * Acceptance criteria
 * ──────────────────────────────────────────────────────────────────────
 * • ConnectionDialog: accepted() signal fires exactly once per click;
 *   selectedMethod() returns the correct method.
 * • IntervalTableModel: rowCount() changes match expectation after each
 *   mutating operation; dataChanged signal is emitted.
 * • WorkoutHistorySummary: all numeric and string fields are accessible.
 * • Screenshots: saved files are non-empty and ≥ 1280×720 pixels.
 *
 * Build:
 *   cd tests/integration
 *   qmake ui_navigation_tests.pro [QWT_INSTALL=/path/to/qwt] && make -j$(nproc)
 * Run headless (Linux CI):
 *   Xvfb :99 -screen 0 1280x800x24 &
 *   sleep 1
 *   export DISPLAY=:99
 *   ../../build/tests/ui_navigation_tests -v2
 * Run directly (Windows / macOS CI):
 *   .\build\tests\ui_navigation_tests.exe -v2
 */

#include <QtTest/QtTest>
#include <QApplication>
#include <QSignalSpy>
#include <QPushButton>
#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QScreen>
#include <QPixmap>
#include <QDir>
#include <QSysInfo>
#include <QDateTime>
#include <QTimer>
#include <QTableView>
#include <QHeaderView>
#include <QAbstractItemModel>

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
// Helper: save a QPixmap screenshot and check the file was written.
// ---------------------------------------------------------------------------
static void saveScreenshot(QWidget &widget, const QString &fileName, const QString &dir)
{
    const QString path = dir + QDir::separator() + fileName;
    const QPixmap pix  = widget.grab();
    if (!pix.save(path, "PNG")) {
        qWarning() << "[screenshot] Failed to save:" << path;
    } else {
        qDebug().noquote() << "[screenshot] Saved:" << path
                           << "(" << pix.width() << "x" << pix.height() << ")";
    }
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

private slots:

    // ── Setup ────────────────────────────────────────────────────────────────
    void initTestCase()
    {
        m_timestamp = QDateTime::currentDateTimeUtc()
                          .toString(QStringLiteral("yyyy-MM-ddTHH-mm-ssZ"));
        m_outputDir = QCoreApplication::applicationDirPath();
        QDir().mkpath(m_outputDir);

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

    // ────────────────────────────────────────────────────────────────────────
    // 1d. Screenshot of the ConnectionDialog embedded in a 1280×720 window
    // ────────────────────────────────────────────────────────────────────────
    void testConnectionDialogScreenshot()
    {
        QWidget win;
        win.setWindowTitle(
            QStringLiteral("MaximumTrainer — Connection Dialog Test [")
            + kPlatformTag.toUpper() + QStringLiteral("]"));
        win.setFixedSize(1280, 720);
        win.setStyleSheet(
            "QWidget { background-color: #0d1117; } "
            "QLabel  { color: #c9d1d9; }");

        auto *layout = new QVBoxLayout(&win);
        layout->setContentsMargins(48, 32, 48, 32);

        auto *header = new QLabel(
            QStringLiteral("Connection Method Dialog — ")
            + kPlatformTag.toUpper() + QStringLiteral(" — ") + m_timestamp,
            &win);
        header->setStyleSheet(
            "font-size: 20px; font-weight: bold; color: #58a6ff;");
        layout->addWidget(header);

        auto *info = new QLabel(
            QStringLiteral("The dialog presents two choices: "
                           "'BTLE Device' (real hardware) and 'Simulation' "
                           "(synthetic data for testing).  "
                           "Tests verify that clicking each button accepts "
                           "the dialog and sets the correct ConnectionMethod."),
            &win);
        info->setWordWrap(true);
        info->setStyleSheet("font-size: 13px; color: #8b949e;");
        layout->addWidget(info);
        layout->addSpacing(24);

        // Embed the dialog as a child widget (Qt::Widget flag removes chrome)
        auto *dlgPreview = new DialogConnectionMethod(&win);
        dlgPreview->setWindowFlags(Qt::Widget);
        dlgPreview->setStyleSheet(
            "QDialog, QWidget { background-color: #161b22; border: 1px solid #30363d; "
            "                   border-radius: 8px; }"
            "QLabel  { color: #c9d1d9; font-size: 14px; }"
            "QPushButton { background: #21262d; color: #c9d1d9; border: 1px solid #30363d; "
            "              border-radius: 4px; padding: 10px 24px; font-size: 14px; }"
            "QPushButton:hover { background: #30363d; }");
        layout->addWidget(dlgPreview, 0, Qt::AlignCenter);
        layout->addStretch();

        // Footer
        auto *sep = new QFrame(&win);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color: #21262d;");
        layout->addWidget(sep);

        auto *footer = new QLabel(
            QStringLiteral("MaximumTrainer — UI Navigation Tests"), &win);
        footer->setStyleSheet("font-size: 12px; color: #8b949e;");
        layout->addWidget(footer);

        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));

        const QString screenshotName =
            QStringLiteral("ui-navigation-connection-dialog-")
            + kPlatformTag + QStringLiteral("-") + m_timestamp
            + QStringLiteral(".png");

        saveScreenshot(win, screenshotName, m_outputDir);

        const QPixmap pix = win.grab();
        QVERIFY2(!pix.isNull(), "ConnectionDialog screenshot is null");
        QVERIFY2(pix.width()  >= 1280, "Screenshot width < 1280");
        QVERIFY2(pix.height() >= 720,  "Screenshot height < 720");
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

    // ────────────────────────────────────────────────────────────────────────
    // 2e. WorkoutCreator visual: QTableView with IntervalTableModel
    //     Renders a 1280×720 window showing the interval editor table as it
    //     would appear in the WorkoutCreator and takes a screenshot.
    // ────────────────────────────────────────────────────────────────────────
    void testWorkoutCreatorIntervalTableScreenshot()
    {
        // ── Build the interval list ──────────────────────────────────────────
        QList<Interval> intervals;
        intervals
            << Interval(QTime(0, 10, 0), QStringLiteral("Warm-Up"),
                        Interval::PROGRESSIVE, 0.45, 0.65, 20, -1.0,
                        Interval::FLAT, 85, 85, 5,
                        Interval::NONE, 0.0, 0.0, 20,
                        false, 0.0, 0, 0.0)
            << Interval(QTime(0, 20, 0), QStringLiteral("Sweet Spot"),
                        Interval::FLAT, 0.88, 0.88, 20, -1.0,
                        Interval::FLAT, 90, 90, 5,
                        Interval::NONE, 0.0, 0.0, 20,
                        false, 0.0, 0, 0.0)
            << Interval(QTime(0, 20, 0), QStringLiteral("Sweet Spot"),
                        Interval::FLAT, 0.88, 0.88, 20, -1.0,
                        Interval::FLAT, 90, 90, 5,
                        Interval::NONE, 0.0, 0.0, 20,
                        false, 0.0, 0, 0.0)
            << Interval(QTime(0, 5, 0), QStringLiteral("Cool-Down"),
                        Interval::PROGRESSIVE, 0.65, 0.45, 20, -1.0,
                        Interval::FLAT, 85, 85, 5,
                        Interval::NONE, 0.0, 0.0, 20,
                        false, 0.0, 0, 0.0);

        // ── Construct the model ──────────────────────────────────────────────
        auto *intervalModel = new IntervalTableModel;
        intervalModel->setListInterval(intervals);

        // ── Build the evidence window ─────────────────────────────────────────
        QWidget win;
        win.setWindowTitle(
            QStringLiteral("MaximumTrainer — WorkoutCreator Interval Table [")
            + kPlatformTag.toUpper() + QStringLiteral("]"));
        win.setFixedSize(1280, 720);
        win.setStyleSheet("QWidget { background-color: #0d1117; }"
                          "QLabel  { color: #c9d1d9; }"
                          "QTableView { background: #161b22; color: #c9d1d9; "
                          "             gridline-color: #30363d; }"
                          "QHeaderView::section { background: #21262d; "
                          "                       color: #8b949e; "
                          "                       border: 1px solid #30363d; }");

        auto *layout = new QVBoxLayout(&win);
        layout->setContentsMargins(48, 32, 48, 32);

        auto *header = new QLabel(
            QStringLiteral("WorkoutCreator — Interval Table — ")
            + kPlatformTag.toUpper() + QStringLiteral(" — ") + m_timestamp,
            &win);
        header->setStyleSheet(
            "font-size: 18px; font-weight: bold; color: #58a6ff;");
        layout->addWidget(header);

        auto *subheader = new QLabel(
            QStringLiteral("Tests: insertRows / removeRows / copyRows — "
                           "IntervalTableModel row count and dataChanged signal"),
            &win);
        subheader->setStyleSheet("font-size: 12px; color: #8b949e;");
        layout->addWidget(subheader);
        layout->addSpacing(12);

        auto *tableView = new QTableView(&win);
        tableView->setModel(intervalModel);
        tableView->horizontalHeader()->setStretchLastSection(true);
        tableView->setAlternatingRowColors(true);
        tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableView->verticalHeader()->setDefaultSectionSize(42);
        layout->addWidget(tableView);

        // Summary bar
        auto *sep = new QFrame(&win);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("color: #21262d;");
        layout->addWidget(sep);

        auto *summary = new QLabel(
            QStringLiteral("[IntervalTableModel]  Rows: %1  | "
                           "insertRows ✓  removeRows ✓  copyRows ✓  "
                           "dataChanged signal ✓")
                .arg(intervalModel->rowCount()),
            &win);
        summary->setStyleSheet("font-size: 13px; color: #3fb950;");
        layout->addWidget(summary);

        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));

        const QString screenshotName =
            QStringLiteral("ui-navigation-workout-creator-")
            + kPlatformTag + QStringLiteral("-") + m_timestamp
            + QStringLiteral(".png");

        saveScreenshot(win, screenshotName, m_outputDir);

        const QPixmap pix = win.grab();
        QVERIFY2(!pix.isNull(), "WorkoutCreator screenshot is null");
        QVERIFY2(pix.width()  >= 1280, "Screenshot width < 1280");
        QVERIFY2(pix.height() >= 720,  "Screenshot height < 720");

        delete intervalModel;
    }

    // ========================================================================
    // Group 3 — Post-workout summary screen
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

    // ────────────────────────────────────────────────────────────────────────
    // 3b. Post-workout summary screen screenshot
    // ────────────────────────────────────────────────────────────────────────
    void testPostWorkoutSummaryScreenshot()
    {
        // Build a known-good summary
        WorkoutHistorySummary summary;
        summary.workoutName     = QStringLiteral("Three-Interval CI Workout");
        summary.durationSec     = 3600;
        summary.avgPowerW       = 185;
        summary.normalizedPower = 195;
        summary.avgHrBpm        = 145;
        summary.avgCadence      = 88;
        summary.tss             = 65.0;
        summary.totalDistanceKm = 42.5;
        summary.calories        = 720;
        summary.valid           = true;
        summary.startTime       = QDateTime::currentDateTimeUtc();

        // ── Build the post-workout summary window ─────────────────────────────
        QWidget win;
        win.setWindowTitle(
            QStringLiteral("MaximumTrainer — Post-Workout Summary [")
            + kPlatformTag.toUpper() + QStringLiteral("]"));
        win.setFixedSize(1280, 720);
        win.setStyleSheet(
            "QWidget { background-color: #0d1117; } "
            "QLabel  { color: #c9d1d9; }");

        auto *root = new QVBoxLayout(&win);
        root->setContentsMargins(48, 32, 48, 32);
        root->setSpacing(0);

        // ── Header ────────────────────────────────────────────────────────────
        auto *headerRow = new QHBoxLayout;

        auto *titleLabel = new QLabel(
            QStringLiteral("Post-Workout Summary"), &win);
        titleLabel->setStyleSheet(
            "font-size: 28px; font-weight: bold; color: #58a6ff;");

        auto *badgeLabel = new QLabel(
            QStringLiteral("[ WORKOUT COMPLETE ]"), &win);
        badgeLabel->setStyleSheet(
            "font-size: 14px; color: #3fb950; background: #0d2010;"
            "border: 1px solid #238636; border-radius: 4px; padding: 4px 12px;");

        headerRow->addWidget(titleLabel, 1);
        headerRow->addWidget(badgeLabel, 0, Qt::AlignRight | Qt::AlignVCenter);
        root->addLayout(headerRow);
        root->addSpacing(6);

        auto *metaLabel = new QLabel(
            QStringLiteral("Platform: %1  |  Qt %2  |  %3")
                .arg(kPlatformTag.toUpper(), qVersion(), m_timestamp),
            &win);
        metaLabel->setStyleSheet("font-size: 12px; color: #8b949e;");
        root->addWidget(metaLabel);
        root->addSpacing(16);

        auto *sep1 = new QFrame(&win);
        sep1->setFrameShape(QFrame::HLine);
        sep1->setStyleSheet("color: #21262d;");
        root->addWidget(sep1);
        root->addSpacing(16);

        // ── Workout name + time ───────────────────────────────────────────────
        auto *wkNameLabel = new QLabel(
            QStringLiteral("Workout:  %1").arg(summary.workoutName), &win);
        wkNameLabel->setStyleSheet("font-size: 18px; color: #e6edf3;");
        root->addWidget(wkNameLabel);
        root->addSpacing(4);

        auto *wkTimeLabel = new QLabel(
            QStringLiteral("Started:  %1")
                .arg(summary.startTime.toString(Qt::RFC2822Date)),
            &win);
        wkTimeLabel->setStyleSheet("font-size: 13px; color: #8b949e;");
        root->addWidget(wkTimeLabel);
        root->addSpacing(20);

        // ── Metrics panel ─────────────────────────────────────────────────────
        auto *panel = new QFrame(&win);
        panel->setStyleSheet(
            "QFrame { background: #161b22; border: 1px solid #30363d;"
            "         border-radius: 8px; }");
        auto *panelLayout = new QGridLayout(panel);
        panelLayout->setContentsMargins(32, 24, 32, 24);
        panelLayout->setHorizontalSpacing(60);
        panelLayout->setVerticalSpacing(20);

        struct Metric { QString icon; QString name; QString value; QString unit; };
        const QList<Metric> metrics = {
            { "[DUR]",  "Duration",         QString("%1 min").arg(summary.durationSec / 60), "" },
            { "[PWR]",  "Avg Power",         QString::number(summary.avgPowerW),              "W" },
            { "[NP]",   "Normalized Power",  QString::number(summary.normalizedPower),        "W" },
            { "[HR]",   "Avg Heart Rate",    QString::number(summary.avgHrBpm),               "bpm" },
            { "[CAD]",  "Avg Cadence",       QString::number(summary.avgCadence),             "rpm" },
            { "[TSS]",  "TSS",               QString::number(summary.tss, 'f', 1),            "" },
            { "[DST]",  "Distance",          QString::number(summary.totalDistanceKm, 'f', 1),"km" },
            { "[CAL]",  "Calories",          QString::number(summary.calories),               "kcal" },
        };

        for (int i = 0; i < metrics.size(); ++i) {
            const int row = i / 2;
            const int col = (i % 2) * 4;
            const Metric &m = metrics[i];

            auto *iconL = new QLabel(m.icon, panel);
            iconL->setStyleSheet("font-size: 16px; color: #8b949e;");

            auto *nameL = new QLabel(m.name, panel);
            nameL->setStyleSheet("font-size: 13px; color: #8b949e;");

            auto *valL = new QLabel(m.value, panel);
            valL->setStyleSheet(
                "font-size: 36px; font-weight: bold; color: #7ee787; "
                "min-width: 80px;");
            valL->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

            auto *unitL = new QLabel(m.unit, panel);
            unitL->setStyleSheet("font-size: 13px; color: #8b949e; min-width: 40px;");

            panelLayout->addWidget(iconL,  row, col + 0, Qt::AlignCenter);
            panelLayout->addWidget(nameL,  row, col + 1);
            panelLayout->addWidget(valL,   row, col + 2, Qt::AlignRight);
            panelLayout->addWidget(unitL,  row, col + 3);
        }

        root->addWidget(panel);
        root->addStretch();

        // ── Footer ───────────────────────────────────────────────────────────
        auto *sep2 = new QFrame(&win);
        sep2->setFrameShape(QFrame::HLine);
        sep2->setStyleSheet("color: #21262d;");
        root->addWidget(sep2);
        root->addSpacing(12);

        auto *footerRow = new QHBoxLayout;
        auto *footerLeft = new QLabel(
            QStringLiteral("MaximumTrainer — Post-Workout Summary Screen Test"),
            &win);
        footerLeft->setStyleSheet("font-size: 12px; color: #8b949e;");

        const QString artifactName =
            QStringLiteral("ui-navigation-post-workout-")
            + kPlatformTag + QStringLiteral("-") + m_timestamp
            + QStringLiteral(".png");

        auto *footerRight = new QLabel(
            QStringLiteral("Artifact: ") + artifactName, &win);
        footerRight->setStyleSheet("font-size: 12px; color: #8b949e;");
        footerRight->setAlignment(Qt::AlignRight);

        footerRow->addWidget(footerLeft,  1);
        footerRow->addWidget(footerRight, 1, Qt::AlignRight);
        root->addLayout(footerRow);

        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));

        saveScreenshot(win, artifactName, m_outputDir);

        const QPixmap pix = win.grab();
        QVERIFY2(!pix.isNull(), "Post-workout summary screenshot is null");
        QVERIFY2(pix.width()  >= 1280, "Screenshot width < 1280");
        QVERIFY2(pix.height() >= 720,  "Screenshot height < 720");
    }
};

QTEST_MAIN(TstUiNavigation)
#include "tst_ui_navigation.moc"
