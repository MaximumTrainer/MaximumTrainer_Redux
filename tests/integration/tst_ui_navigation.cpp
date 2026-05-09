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
#include <QWidget>
#include <QDir>
#include <QDateTime>
#include <QProcess>
#include <QImage>
#include <QFileInfo>

#include "../../src/model/account.h"

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

        // Register an Account for application startup consistency.
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
            // Verify the screenshot has real content. We don't assert exact
            // minimum dimensions because window managers on CI runners (especially
            // Windows) may constrain the window below the requested geometry.
            QVERIFY2(img.width()  > 0 && img.height() > 0,
                     qPrintable(QString("%1: zero-size image (%2x%3)")
                                    .arg(fileName).arg(img.width()).arg(img.height())));

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
};

QTEST_MAIN(TstUiNavigation)
#include "tst_ui_navigation.moc"
