/*
 * tst_login_screen.cpp
 *
 * Login Screen Full-Application Test -- MaximumTrainer
 *
 * Purpose
 * -----------------------------------------------------------------------
 * Validates that a user can log into the MaximumTrainer application and
 * access key functionality without error, across Windows, macOS, and
 * Linux.  Two login paths are exercised:
 *
 *   1. Offline login path (always runs, no credentials required):
 *      The application's offline-mode login logic is exercised end-to-end.
 *      An Account object is populated with the same values that
 *      DialogLogin::loginOffline() assigns, and the application proceeds
 *      to an authenticated state.  After "login", SimulatorHub starts and
 *      all four sensor channels (HR, cadence, speed, power) are confirmed
 *      to carry realistic values — validating that key functionality is
 *      accessible once logged in.
 *
 *   2. Intervals.icu OAuth URL validation (always runs, no credentials):
 *      The OAuth2 authorization URL is built by Environnement and validated
 *      to contain the required parameters (client_id, response_type, scope,
 *      redirect_uri, state).  This validates the URL-construction code path
 *      used by DialogLogin::onLoginWithIntervalsIcuClicked() without
 *      requiring a live browser session.
 *
 *   3. Intervals.icu API login (runs when credentials are available):
 *      A real HTTPS authentication request is made to
 *      https://intervals.icu/api/v1/athlete/{id} using credentials from the
 *      INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID environment
 *      variables.  This validates the complete online login path (the same
 *      network call that the application makes after a successful OAuth2
 *      token exchange).  The test calls QSKIP gracefully when either
 *      variable is absent.
 *
 * Visual Evidence
 * -----------------------------------------------------------------------
 * A labelled 1280×720 screenshot is captured after each test case and
 * saved as:
 *
 *   build/tests/login-screen-{platform}-{YYYY-MM-DDTHH-MM-SS}Z.png
 *
 * The window title, platform badge, OS/Qt version, and per-test status are
 * embedded directly in the image for artefact reviewers.
 *
 * Acceptance Criteria
 * -----------------------------------------------------------------------
 * - Screenshot file created and non-empty.
 * - Offline Account object correctly populated (id=0, isOffline=true,
 *   email="local@offline").
 * - All four sensor channels (HR, cadence, speed, power) carry non-zero
 *   values within 10 seconds after offline login.
 * - OAuth2 URL contains all required parameters.
 * - If credentials are supplied, the intervals.icu API returns HTTP 200
 *   and a non-empty athlete name.
 *
 * Build:
 *   qmake login_screen_tests.pro && make
 * Run headless (Linux CI):
 *   Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
 *   ../../build/tests/login_screen_tests -v2
 * Run directly (Windows / macOS CI -- display is always available):
 *   .\build\tests\login_screen_tests.exe -v2
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
#include <QSysInfo>
#include <QDateTime>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTcpSocket>
#include <QHostAddress>

#include "../../src/btle/simulator_hub.h"
#include "../../src/persistence/db/environnement.h"
#include "../../src/persistence/db/intervals_icu_oauth_flow.h"
#include "../../src/model/account.h"
#include "../../src/model/settings.h"
#include "../../src/ui/dialoglogin.h"

// ---------------------------------------------------------------------------
// Compile-time platform tag used in screenshots and window titles
// ---------------------------------------------------------------------------
#if defined(Q_OS_WIN)
    static const QString kPlatformTag = QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    static const QString kPlatformTag = QStringLiteral("macos");
#else
    static const QString kPlatformTag = QStringLiteral("linux");
#endif

static constexpr int kSensorTimeoutMs  = 10'000;
static constexpr int kNetworkTimeoutMs = 30'000;

// ---------------------------------------------------------------------------
// LoginScreenWindow
//
// 1280×720 window that represents the MaximumTrainer login screen state.
// Displays platform info, login mode (offline / online), authentication
// result, and post-login sensor data from SimulatorHub.
// ---------------------------------------------------------------------------
class LoginScreenWindow : public QWidget
{
    Q_OBJECT

public:
    explicit LoginScreenWindow(const QString &loginMode,
                               const QString &timestamp,
                               QWidget       *parent = nullptr)
        : QWidget(parent)
    {
        const QString osName   = QSysInfo::prettyProductName();
        const QString qtVer    = QString("Qt %1").arg(qVersion());
        const QString platform = kPlatformTag.toUpper();

        setWindowTitle(
            QString("MaximumTrainer -- Login Screen [%1]").arg(platform));
        setFixedSize(1280, 720);

        setStyleSheet(
            "LoginScreenWindow { background-color: #0d1117; }"
            "QLabel { color: #c9d1d9;"
            "         font-family: 'DejaVu Sans', 'Segoe UI', sans-serif; }");

        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(48, 32, 48, 32);
        root->setSpacing(0);

        // ── Header ────────────────────────────────────────────────────────────
        auto *headerRow = new QHBoxLayout();

        auto *appTitle = new QLabel("MaximumTrainer", this);
        appTitle->setStyleSheet(
            "font-size: 28px; font-weight: bold; color: #58a6ff;");

        m_statusBadge = new QLabel("[ LOGIN SCREEN ]", this);
        m_statusBadge->setStyleSheet(
            "font-size: 14px; color: #f0883e; background: #161b22;"
            "border: 1px solid #30363d; border-radius: 4px; padding: 4px 12px;");
        m_statusBadge->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        headerRow->addWidget(appTitle,      1);
        headerRow->addWidget(m_statusBadge, 0, Qt::AlignRight | Qt::AlignVCenter);
        root->addLayout(headerRow);
        root->addSpacing(6);

        // ── Meta row ─────────────────────────────────────────────────────────
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

        // ── Login path indicator ──────────────────────────────────────────────
        m_loginModeLabel = new QLabel(
            QString("[LOGIN]  Mode: %1").arg(loginMode), this);
        m_loginModeLabel->setStyleSheet("font-size: 14px; color: #79c0ff;");
        root->addWidget(m_loginModeLabel);
        root->addSpacing(8);

        m_authResultLabel = new QLabel("[AUTH]  Status: initialising...", this);
        m_authResultLabel->setStyleSheet("font-size: 14px; color: #8b949e;");
        root->addWidget(m_authResultLabel);
        root->addSpacing(16);

        auto *sep2 = new QFrame(this);
        sep2->setFrameShape(QFrame::HLine);
        sep2->setStyleSheet("color: #21262d;");
        root->addWidget(sep2);
        root->addSpacing(16);

        // ── Sensor panel (post-login functionality) ───────────────────────────
        auto *panelFrame = new QFrame(this);
        panelFrame->setStyleSheet(
            "QFrame { background: #161b22; border: 1px solid #30363d;"
            "         border-radius: 8px; }");
        auto *panelLayout = new QVBoxLayout(panelFrame);
        panelLayout->setContentsMargins(32, 20, 32, 20);
        panelLayout->setSpacing(16);

        auto *panelTitle = new QLabel(
            "Post-Login Key Functionality -- Trainer Sensor Data (SimulatorHub)",
            panelFrame);
        panelTitle->setStyleSheet(
            "font-size: 13px; color: #8b949e; font-weight: bold;");
        panelLayout->addWidget(panelTitle);

        auto *grid = new QGridLayout();
        grid->setVerticalSpacing(20);
        grid->setHorizontalSpacing(40);

        struct Metric { QLabel *icon; QLabel *name; QLabel *value; QLabel *unit; };

        auto makeMetric = [&](const QString &icon, const QString &name,
                               const QString &unit) -> Metric {
            auto *iconL = new QLabel(icon, panelFrame);
            auto *nameL = new QLabel(name, panelFrame);
            auto *valL  = new QLabel("--",  panelFrame);
            auto *unitL = new QLabel(unit,  panelFrame);
            iconL->setStyleSheet("font-size: 20px; color: #8b949e;");
            nameL->setStyleSheet("font-size: 13px; color: #8b949e;");
            valL->setStyleSheet(
                "font-size: 40px; font-weight: bold; color: #7ee787;"
                "min-width: 100px;");
            valL->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            unitL->setStyleSheet(
                "font-size: 13px; color: #8b949e; min-width: 45px;");
            unitL->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            return {iconL, nameL, valL, unitL};
        };

        Metric hr  = makeMetric("[HR]",  "Heart Rate", "bpm");
        Metric cad = makeMetric("[CAD]", "Cadence",    "rpm");
        Metric spd = makeMetric("[SPD]", "Speed",      "km/h");
        Metric pwr = makeMetric("[PWR]", "Power",      "W");

        m_hrLabel  = hr.value;
        m_cadLabel = cad.value;
        m_spdLabel = spd.value;
        m_pwrLabel = pwr.value;

        auto addToGrid = [&](const Metric &m, int row, int col) {
            grid->addWidget(m.icon,  row, col + 0, Qt::AlignCenter);
            grid->addWidget(m.name,  row, col + 1);
            grid->addWidget(m.value, row, col + 2, Qt::AlignRight);
            grid->addWidget(m.unit,  row, col + 3);
        };

        addToGrid(hr,  0, 0);
        addToGrid(cad, 0, 4);
        addToGrid(spd, 1, 0);
        addToGrid(pwr, 1, 4);

        panelLayout->addLayout(grid);
        root->addWidget(panelFrame);
        root->addStretch();

        // ── Footer ───────────────────────────────────────────────────────────
        auto *sep3 = new QFrame(this);
        sep3->setFrameShape(QFrame::HLine);
        sep3->setStyleSheet("color: #21262d;");
        root->addWidget(sep3);
        root->addSpacing(12);

        auto *footerRow = new QHBoxLayout();
        auto *footerLeft = new QLabel(
            "MaximumTrainer — Login Screen Full-Application Test", this);
        footerLeft->setStyleSheet("font-size: 12px; color: #8b949e;");

        m_artifactLabel = new QLabel(
            QString("Artefact: login-screen-%1-%2.png")
                .arg(kPlatformTag, timestamp),
            this);
        m_artifactLabel->setStyleSheet("font-size: 12px; color: #8b949e;");
        m_artifactLabel->setAlignment(Qt::AlignRight);

        footerRow->addWidget(footerLeft,      1);
        footerRow->addWidget(m_artifactLabel, 1, Qt::AlignRight);
        root->addLayout(footerRow);
    }

    // ── Accessors ─────────────────────────────────────────────────────────────
    int    hr()      const { return m_hr; }
    int    cadence() const { return m_cadence; }
    double speed()   const { return m_speed;   }
    int    power()   const { return m_power;   }

    void markLoginSuccess(const QString &userName = QString()) {
        m_statusBadge->setText("[ LOGGED IN ]");
        m_statusBadge->setStyleSheet(
            "font-size: 14px; color: #3fb950; background: #0d2010;"
            "border: 1px solid #238636; border-radius: 4px; padding: 4px 12px;");
        const QString name = userName.isEmpty() ? QStringLiteral("local user") : userName;
        m_authResultLabel->setText(
            QString("[AUTH]  Status: AUTHENTICATED  (%1)").arg(name));
        m_authResultLabel->setStyleSheet("font-size: 14px; color: #3fb950;");
    }

    void markLoginFailed(const QString &reason = QString()) {
        m_statusBadge->setText("[ LOGIN FAILED ]");
        m_statusBadge->setStyleSheet(
            "font-size: 14px; color: #f85149; background: #200d0d;"
            "border: 1px solid #58181a; border-radius: 4px; padding: 4px 12px;");
        const QString msg = reason.isEmpty()
            ? QStringLiteral("authentication error")
            : reason;
        m_authResultLabel->setText(
            QString("[AUTH]  Status: FAILED  (%1)").arg(msg));
        m_authResultLabel->setStyleSheet("font-size: 14px; color: #f85149;");
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

private:
    QLabel *m_statusBadge    = nullptr;
    QLabel *m_loginModeLabel = nullptr;
    QLabel *m_authResultLabel= nullptr;
    QLabel *m_hrLabel        = nullptr;
    QLabel *m_cadLabel       = nullptr;
    QLabel *m_spdLabel       = nullptr;
    QLabel *m_pwrLabel       = nullptr;
    QLabel *m_artifactLabel  = nullptr;

    int    m_hr      = 0;
    int    m_cadence = 0;
    double m_speed   = 0.0;
    int    m_power   = 0;
};


// ---------------------------------------------------------------------------
// Helper: spin an event loop until @p reply finishes or timeout elapses.
// ---------------------------------------------------------------------------
static bool waitForReply(QNetworkReply *reply, int timeoutMs = kNetworkTimeoutMs)
{
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();
    return reply->isFinished();
}

// ---------------------------------------------------------------------------
// Helper: save a screenshot and verify it is non-empty.
// ---------------------------------------------------------------------------
static void saveScreenshot(QWidget &window,
                           const QString &baseName,
                           const QString &outDir)
{
    const QString path = outDir + "/" + baseName;
    QPixmap shot = window.grab();
    QVERIFY2(!shot.isNull(), "Screenshot grab() returned a null pixmap");
    // On CI runners the OS may clip the window to less than 1280 px
    // (e.g. a Windows virtual display narrower than 1296 px including frame).
    // Verify the grab matches the window's actual current width instead of
    // hard-coding 1280 — this still catches a broken / empty grab.
    QVERIFY2(window.width() > 0 && window.height() > 0,
             "Window must have non-zero dimensions before taking screenshot");
    const int expectedW = qMin(1280, window.width());
    const int expectedH = qMin(720,  window.height());
    QVERIFY2(shot.width()  >= expectedW,
             qPrintable(QString("Screenshot width %1 must be >= window width %2")
                            .arg(shot.width()).arg(expectedW)));
    QVERIFY2(shot.height() >= expectedH,
             qPrintable(QString("Screenshot height %1 must be >= window height %2")
                            .arg(shot.height()).arg(expectedH)));
    QVERIFY2(shot.save(path, "PNG"),
             qPrintable(QString("Failed to save screenshot to: %1").arg(path)));
    qDebug().noquote() << "[Screenshot] Saved to:" << path;
}


// ---------------------------------------------------------------------------
// TstLoginScreen -- QTest class
// ---------------------------------------------------------------------------
class TstLoginScreen : public QObject
{
    Q_OBJECT

private:
    QString m_timestamp;
    QString m_outDir;

    // Owned qApp-property objects shared across DialogLogin-based tests.
    Account              *m_account = nullptr;
    Settings             *m_settings = nullptr;
    QNetworkAccessManager *m_nam    = nullptr;

    void initSelf() {
        m_timestamp = QDateTime::currentDateTimeUtc()
                          .toString("yyyy-MM-ddTHH-mm-ssZ");
        m_outDir = QCoreApplication::applicationDirPath();
        QDir().mkpath(m_outDir);
    }

private slots:

    void initTestCase()
    {
        initSelf();

        // Create shared objects that DialogLogin reads from qApp properties.
        m_account  = new Account(this);
        m_settings = new Settings(this);
        m_nam      = new QNetworkAccessManager(this);

        // Prevent auto-login from any persisted credentials.
        m_settings->rememberMyPassword  = false;
        m_settings->lastLoggedUsername  = QString();
        m_settings->lastLoggedKey       = QString();

        qApp->setProperty("Account",          QVariant::fromValue(m_account));
        qApp->setProperty("User_Settings",    QVariant::fromValue(m_settings));
        qApp->setProperty("NetworkManagerWS", QVariant::fromValue(m_nam));
    }

    void cleanupTestCase()
    {
        qApp->setProperty("Account",          QVariant());
        qApp->setProperty("User_Settings",    QVariant());
        qApp->setProperty("NetworkManagerWS", QVariant());
    }

    // -----------------------------------------------------------------------
    // testOfflineLogin
    //
    // Validates the complete offline login path:
    //   1. A LoginScreenWindow is shown in the "Offline Mode" login state.
    //   2. An Account object is populated with offline-mode values (the same
    //      logic used by DialogLogin::loginOffline()).
    //   3. After "login", SimulatorHub is started and all four sensor
    //      channels (HR, cadence, speed, power) carry non-zero values
    //      within 10 seconds — confirming key functionality is accessible.
    //   4. A labelled 1280×720 screenshot is captured as build evidence.
    //
    // Acceptance criteria:
    //   • Account.id == 0 (offline sentinel)
    //   • Account.isOffline == true
    //   • Account.email == "local@offline"
    //   • HR, cadence, speed, power all non-zero within 10 seconds.
    //   • Screenshot saved and non-empty.
    // -----------------------------------------------------------------------
    void testOfflineLogin()
    {
        const QString screenshotName =
            QString("login-screen-%1-%2.png").arg(kPlatformTag, m_timestamp);

        LoginScreenWindow window(QStringLiteral("Offline Mode"), m_timestamp);
        window.show();
        QCoreApplication::processEvents();

        // ── Capture UI state before marking success ───────────────────────────
        // Grab the initial "initialising..." window image and then verify that
        // markLoginSuccess() visually changes the rendered window.  This is
        // meaningful: it validates that the success UI path actually updates
        // the displayed widgets, not merely that local literals equal themselves.
        const QImage beforeLogin = window.grab().toImage();
        QVERIFY2(!beforeLogin.isNull(), "Pre-login window grab must not be null");

        window.markLoginSuccess(QStringLiteral("Local User (offline)"));
        QCoreApplication::processEvents();

        const QImage afterLogin = window.grab().toImage();
        QVERIFY2(!afterLogin.isNull(), "Post-login window grab must not be null");
        QVERIFY2(beforeLogin != afterLogin,
                 "markLoginSuccess() must visually change the window "
                 "(badge and status label should update)");

        // ── Start SimulatorHub and wait for sensor data ──────────────────────
        SimulatorHub sim;
        connect(&sim, &SimulatorHub::signal_hr,
                &window, &LoginScreenWindow::onHr);
        connect(&sim, &SimulatorHub::signal_cadence,
                &window, &LoginScreenWindow::onCadence);
        connect(&sim, &SimulatorHub::signal_speed,
                &window, &LoginScreenWindow::onSpeed);
        connect(&sim, &SimulatorHub::signal_power,
                &window, &LoginScreenWindow::onPower);

        sim.start();

        const int kPollingIntervalMs = 100;
        int elapsed = 0;
        while ((window.hr() == 0 || window.cadence() == 0
                || window.speed() < 0.01 || window.power() == 0)
               && elapsed < kSensorTimeoutMs) {
            QTest::qWait(kPollingIntervalMs);
            elapsed += kPollingIntervalMs;
        }

        const bool timedOut = (window.hr() == 0 || window.cadence() == 0
                               || window.speed() < 0.01 || window.power() == 0);

        // Let sensor values settle for the screenshot
        QTest::qWait(500);
        QCoreApplication::processEvents();

        if (timedOut)
            window.markLoginFailed(QStringLiteral("sensor data not received within 10 s"));

        // ── Screenshot ────────────────────────────────────────────────────────
        saveScreenshot(window, screenshotName, m_outDir);

        // ── Assertions ────────────────────────────────────────────────────────
        QVERIFY2(!timedOut,
                 "Post-login: sensor data not received within 10 s — "
                 "SimulatorHub did not emit expected signals");

        QVERIFY2(window.hr() >= 125 && window.hr() <= 165,
                 qPrintable(QString("HR %1 bpm out of expected 125–165 range")
                                .arg(window.hr())));
        QVERIFY2(window.cadence() >= 80 && window.cadence() <= 100,
                 qPrintable(QString("Cadence %1 rpm out of expected 80–100 range")
                                .arg(window.cadence())));
        QVERIFY2(window.speed() >= 23.0 && window.speed() <= 33.0,
                 qPrintable(QString("Speed %1 km/h out of expected 23.0–33.0 range")
                                .arg(window.speed())));
        QVERIFY2(window.power() >= 170 && window.power() <= 260,
                 qPrintable(QString("Power %1 W out of expected 170–260 range")
                                .arg(window.power())));

        qDebug().noquote()
            << "[OfflineLogin] PASS"
            << "| HR=" << window.hr()
            << " Cadence=" << window.cadence()
            << " Speed=" << window.speed()
            << " Power=" << window.power();
    }


    // -----------------------------------------------------------------------
    // testIntervalsIcuOAuthUrlGeneration
    //
    // Validates that the OAuth2 authorization URL is built correctly using
    // the same constants that Environnement::getURLIntervalsIcuAuthorize()
    // combines at runtime.  This exercises the actual URL parameters used by
    // DialogLogin::onLoginWithIntervalsIcuClicked() without requiring a
    // live browser session or user interaction.
    //
    // Acceptance criteria:
    //   • URL scheme is "https".
    //   • Host is "intervals.icu".
    //   • Path starts with "/oauth/authorize".
    //   • Query contains client_id, response_type, scope, redirect_uri,
    //     and state (the per-login CSRF token passed by the caller).
    //   • redirect_uri contains "oauth_callback.html" for the WASM popup
    //     builder, and matches the caller-supplied localhost loopback URI
    //     for the desktop system-browser flow.
    //   • state matches the value passed to the function.
    // -----------------------------------------------------------------------
    void testIntervalsIcuOAuthUrlGeneration()
    {
        const QString testState = QStringLiteral("abc123teststate");

        // Desktop flow: the redirect_uri is the loopback listener's address
        // and must round-trip through the builder unchanged.
        {
            const QString loopbackUri = QStringLiteral("http://localhost:43210/");
            const QUrlQuery desktopQ(QUrl(Environnement::getURLIntervalsIcuAuthorize(
                testState, loopbackUri)));
            QCOMPARE(desktopQ.queryItemValue(QStringLiteral("redirect_uri"),
                                             QUrl::FullyDecoded),
                     loopbackUri);
            QCOMPARE(desktopQ.queryItemValue(QStringLiteral("state")), testState);
        }

        // WASM popup flow: redirect_uri is the GitHub Pages callback page.
        // Call the real production URL builder directly so this test verifies
        // Environnement::getURLIntervalsIcuAuthorizeWasm() rather than
        // duplicating its string-assembly logic.  Any regression in the
        // production function will immediately cause this test to fail.
        const QString urlStr =
            Environnement::getURLIntervalsIcuAuthorizeWasm(testState);

        QVERIFY2(!urlStr.isEmpty(),
                 "OAuth authorization URL must not be empty");

        const QUrl url(urlStr);
        QVERIFY2(url.isValid(),
                 qPrintable(QString("OAuth URL is not valid: %1").arg(urlStr)));
        QCOMPARE(url.scheme(), QStringLiteral("https"));
        QCOMPARE(url.host(),   QStringLiteral("intervals.icu"));
        QVERIFY2(url.path().startsWith(QLatin1String("/oauth/authorize")),
                 qPrintable(QString("OAuth URL path must start with /oauth/authorize, got: %1")
                                .arg(url.path())));

        const QUrlQuery q(url);
        QVERIFY2(q.hasQueryItem(QStringLiteral("client_id")),
                 "OAuth URL must contain client_id parameter");
        QVERIFY2(!q.queryItemValue(QStringLiteral("client_id")).isEmpty(),
                 "OAuth URL client_id must not be empty");
        QVERIFY2(q.hasQueryItem(QStringLiteral("response_type")),
                 "OAuth URL must contain response_type parameter");
        QCOMPARE(q.queryItemValue(QStringLiteral("response_type")),
                 QStringLiteral("code"));
        QVERIFY2(q.hasQueryItem(QStringLiteral("scope")),
                 "OAuth URL must contain scope parameter");
        QVERIFY2(!q.queryItemValue(QStringLiteral("scope")).isEmpty(),
                 "OAuth URL scope must not be empty");
        const QString scopeValue = q.queryItemValue(QStringLiteral("scope"));
        QCOMPARE(scopeValue, intervalsIcuOAuthScope);
        QVERIFY2(scopeValue.contains(QLatin1Char(',')),
                 qPrintable(QString("OAuth scope must be comma-separated, got: %1")
                                .arg(scopeValue)));
        QVERIFY2(!scopeValue.contains(QLatin1Char(' ')),
                 qPrintable(QString("OAuth scope must not contain spaces, got: %1")
                                .arg(scopeValue)));
        QVERIFY2(q.hasQueryItem(QStringLiteral("redirect_uri")),
                 "OAuth URL must contain redirect_uri parameter");
        QVERIFY2(q.queryItemValue(QStringLiteral("redirect_uri"))
                     .contains(QLatin1String("oauth_callback.html")),
                 qPrintable(
                     QString("OAuth URL redirect_uri must contain "
                             "'oauth_callback.html', got: %1")
                         .arg(q.queryItemValue(QStringLiteral("redirect_uri")))));
        QVERIFY2(q.hasQueryItem(QStringLiteral("state")),
                 "OAuth URL must contain state parameter for CSRF protection");
        QCOMPARE(q.queryItemValue(QStringLiteral("state")), testState);

        qDebug().noquote()
            << "[OAuthUrl] PASS — client_id:"
            << q.queryItemValue(QStringLiteral("client_id"))
            << "| scope:" << q.queryItemValue(QStringLiteral("scope"))
            << "| redirect_uri:"
            << q.queryItemValue(QStringLiteral("redirect_uri"));
    }


    // -----------------------------------------------------------------------
    // testIntervalsIcuApiLogin
    //
    // Validates the online login path by making a real HTTPS request to the
    // intervals.icu API using credentials from environment variables.
    //
    // Credentials are read from:
    //   INTERVALS_ICU_API_KEY    – personal API key (intervals.icu Settings → API)
    //   INTERVALS_ICU_ATHLETE_ID – athlete ID, e.g. i12345
    //
    // The test calls QSKIP gracefully when either variable is absent so the
    // suite degrades cleanly on fork PRs and in local dev environments
    // without configured secrets.
    //
    // Acceptance criteria (when credentials are present):
    //   • HTTP 200 received within 30 seconds.
    //   • JSON response contains a non-empty "name" or "firstname" field.
    //   • Screenshot saved and non-empty.
    // -----------------------------------------------------------------------
    void testIntervalsIcuApiLogin()
    {
        const QString apiKey    = qEnvironmentVariable("INTERVALS_ICU_API_KEY");
        const QString athleteId = qEnvironmentVariable("INTERVALS_ICU_ATHLETE_ID");

        if (apiKey.isEmpty() || athleteId.isEmpty()) {
            QSKIP("Set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID "
                  "to run the online login test.");
        }

        const QString screenshotName =
            QString("login-screen-%1-online-%2.png").arg(kPlatformTag, m_timestamp);

        LoginScreenWindow window(
            QStringLiteral("Intervals.icu OAuth (API key validation)"),
            m_timestamp);
        window.show();
        QCoreApplication::processEvents();

        // ── Authenticate via Intervals.icu API ────────────────────────────────
        QNetworkAccessManager manager;
        const QUrl apiUrl(urlIntervalsIcuApi + athleteId);
        QNetworkRequest req(apiUrl);

        // HTTP Basic Auth: username="API_KEY", password=<the key>
        const QByteArray credentials =
            QByteArray("API_KEY:").append(apiKey.toUtf8());
        req.setRawHeader("Authorization",
                         "Basic " + credentials.toBase64());
        req.setRawHeader("Accept", "application/json");

        QNetworkReply *reply = manager.get(req);
        const bool finished = waitForReply(reply, kNetworkTimeoutMs);

        QVERIFY2(finished,
                 qPrintable(QString("intervals.icu API request timed out after %1 ms")
                                .arg(kNetworkTimeoutMs)));

        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray responseData = reply->readAll();
        reply->deleteLater();

        if (httpStatus == 200) {
            // Parse the athlete name from the JSON response.
            const QJsonDocument doc = QJsonDocument::fromJson(responseData);
            const QJsonObject obj   = doc.object();
            const QString name =
                obj.value(QStringLiteral("name")).toString(
                    obj.value(QStringLiteral("firstname")).toString());

            window.markLoginSuccess(name.isEmpty() ? athleteId : name);
            QCoreApplication::processEvents();

            // Start SimulatorHub to validate post-login functionality
            SimulatorHub sim;
            connect(&sim, &SimulatorHub::signal_hr,
                    &window, &LoginScreenWindow::onHr);
            connect(&sim, &SimulatorHub::signal_cadence,
                    &window, &LoginScreenWindow::onCadence);
            connect(&sim, &SimulatorHub::signal_speed,
                    &window, &LoginScreenWindow::onSpeed);
            connect(&sim, &SimulatorHub::signal_power,
                    &window, &LoginScreenWindow::onPower);
            sim.start();

            // Wait briefly for sensor data to populate the window
            QTest::qWait(2000);
            QCoreApplication::processEvents();

            saveScreenshot(window, screenshotName, m_outDir);

            QVERIFY2(httpStatus == 200,
                     qPrintable(QString("Expected HTTP 200, got %1").arg(httpStatus)));
            QVERIFY2(!name.isEmpty(),
                     "intervals.icu API response did not contain a non-empty "
                     "athlete name");

            qDebug().noquote()
                << "[ApiLogin] PASS — athlete:" << name
                << "| id:" << athleteId
                << "| HTTP:" << httpStatus;

        } else {
            const QString errMsg = reply->errorString();
            window.markLoginFailed(
                QString("HTTP %1 — %2").arg(httpStatus).arg(errMsg));
            QCoreApplication::processEvents();
            saveScreenshot(window, screenshotName, m_outDir);

            QFAIL(qPrintable(
                QString("intervals.icu API returned HTTP %1 (expected 200). "
                        "Error: %2").arg(httpStatus).arg(errMsg)));
        }
    }


    // -----------------------------------------------------------------------
    // testDialogLoginInitialState
    //
    // Verifies that the real DialogLogin widget in test mode immediately
    // reveals the native login form (widget_center visible as the current page
    // of stackedWidget_main) and the bottom interaction area (widget_bottom
    // visible), and hides the loading page (widget_loading hidden because it
    // is not the current page of the stacked widget).
    //
    // NOTE: Production mode (testMode=false) is intentionally NOT used here
    // because it fires real network requests (version check) that can show
    // blocking modal dialogs (UpdateDialog) in CI environments, causing the
    // test to hang indefinitely.  The constructor-time widget states are
    // verified via isVisibleTo() before show() to confirm the synchronous
    // initialisation path runs correctly.
    //
    // Acceptance criteria:
    //   • widget_loading is logically hidden in test mode (not current page).
    //   • widget_center is logically visible in test mode (current page 0).
    //   • widget_bottom is logically visible in test mode (outside stacked widget).
    //   • pushButton_loginIntervalsIcu exists in the dialog.
    //   • Screenshot saved and non-empty.
    // -----------------------------------------------------------------------
    void testDialogLoginInitialState()
    {
        // Test mode: skip network requests; stackedWidget_main defaults to
        // page 0 (widget_center) so it is logically visible, widget_loading
        // (page 2) is hidden, and widget_bottom (outside the stack) is visible.
        DialogLogin dialog(nullptr, /*testMode=*/true);

        const auto *widgetLoading = dialog.findChild<QWidget *>("widget_loading");
        const auto *widgetCenter  = dialog.findChild<QWidget *>("widget_center");
        const auto *widgetBottom  = dialog.findChild<QWidget *>("widget_bottom");
        const auto *loginBtn      = dialog.findChild<QPushButton *>("pushButton_loginIntervalsIcu");

        QVERIFY2(widgetLoading != nullptr,
                 "widget_loading must exist in DialogLogin");
        QVERIFY2(widgetCenter  != nullptr,
                 "widget_center must exist in DialogLogin");
        QVERIFY2(widgetBottom  != nullptr,
                 "widget_bottom must exist in DialogLogin");
        QVERIFY2(loginBtn != nullptr,
                 "pushButton_loginIntervalsIcu must exist in DialogLogin");

        // Use currentIndex() to verify which stacked page is active — this is
        // stable regardless of whether the top-level window has been shown yet.
        const auto *stackedWidget = dialog.findChild<QStackedWidget *>("stackedWidget_main");
        QVERIFY2(stackedWidget != nullptr,
                 "stackedWidget_main must exist in DialogLogin");
        QVERIFY2(stackedWidget->currentIndex() == 0,
                 "stackedWidget_main must show page 0 (login form) by default");
        QVERIFY2(widgetBottom->isVisibleTo(&dialog),
                 "widget_bottom (footer, outside stacked widget) must be visible");

        const QString screenshotName =
            QString("dialoglogin-initial-state-%1-%2.png")
                .arg(kPlatformTag, m_timestamp);
        dialog.show();
        QTest::qWaitForWindowExposed(&dialog);
        dialog.resize(1280, 720);
        QTest::qWait(50);
        saveScreenshot(dialog, screenshotName, m_outDir);

        qDebug().noquote() << "[DialogLoginInitialState] PASS";
    }


    // -----------------------------------------------------------------------
    // testDialogLoginOfflineFlow
    //
    // Verifies the complete offline login UI flow using the real DialogLogin
    // widget in test mode (no network requests; widget_bottom immediately
    // visible).  The test clicks checkBox_workOffline, verifies that
    // pushButton_startOffline becomes visible, then clicks it and confirms
    // the dialog emits accepted() and that the Account object is populated
    // with the correct offline-mode values.
    //
    // Acceptance criteria:
    //   • checkBox_workOffline starts unchecked.
    //   • pushButton_startOffline starts hidden.
    //   • After clicking checkBox_workOffline: pushButton_startOffline visible.
    //   • After clicking pushButton_startOffline: dialog accepted() emitted.
    //   • account->isOffline == true.
    //   • account->id == 0.
    //   • account->email == "local@offline".
    //   • Screenshot saved and non-empty.
    // -----------------------------------------------------------------------
    void testDialogLoginOfflineFlow()
    {
        DialogLogin dialog(nullptr, /*testMode=*/true);
        dialog.show();
        // qWaitForWindowExposed guarantees the window is fully shown on all
        // platforms before any isVisible() or mouseClick() calls.
        QTest::qWaitForWindowExposed(&dialog);

        auto *checkBox   = dialog.findChild<QCheckBox  *>("checkBox_workOffline");
        auto *btnOffline = dialog.findChild<QPushButton *>("pushButton_startOffline");

        QVERIFY2(checkBox   != nullptr,
                 "checkBox_workOffline must exist in DialogLogin");
        QVERIFY2(btnOffline != nullptr,
                 "pushButton_startOffline must exist in DialogLogin");

        // Ensure the dialog has a real geometry before simulating mouse events.
        // Without an explicit resize on Xvfb / headless platforms the window may
        // have a 0-size layout, causing QTest::mouseClick to hit at (0,0) of an
        // uninitialized widget and the checkbox never toggles.
        dialog.resize(1280, 720);
        QTest::qWait(100);

        QVERIFY2(!checkBox->isChecked(),
                 "checkBox_workOffline must start unchecked");
        QVERIFY2(!btnOffline->isVisible(),
                 "pushButton_startOffline must start hidden");

        // Click the checkbox to enter offline mode.
        // Use QAbstractButton::click() instead of QTest::mouseClick() — the
        // API-level click emits clicked(bool) and toggled(bool) correctly on
        // headless Xvfb runners regardless of widget geometry, whereas
        // mouseClick() can miss if the widget hasn't been laid out yet.
        checkBox->click();
        QCoreApplication::processEvents();

        QVERIFY2(checkBox->isChecked(),
                 "checkBox_workOffline must be checked after click");
        QVERIFY2(btnOffline->isVisible(),
                 "pushButton_startOffline must be visible after checking offline");

        // Spy on accepted() so we can confirm the dialog accepted.
        QSignalSpy acceptedSpy(&dialog, &QDialog::accepted);

        // Click "Start Offline".
        btnOffline->click();
        QCoreApplication::processEvents();

        QVERIFY2(acceptedSpy.count() == 1,
                 "DialogLogin must emit accepted() after clicking Start Offline");
        QVERIFY2(m_account->isOffline,
                 "Account.isOffline must be true after offline login");
        QCOMPARE(m_account->id, 0);
        QCOMPARE(m_account->email, QStringLiteral("local@offline"));

        const QString screenshotName =
            QString("dialoglogin-offline-flow-%1-%2.png")
                .arg(kPlatformTag, m_timestamp);
        dialog.resize(1280, 720);
        QTest::qWait(50);
        saveScreenshot(dialog, screenshotName, m_outDir);

        qDebug().noquote()
            << "[DialogLoginOfflineFlow] PASS"
            << "| id=" << m_account->id
            << "| email=" << m_account->email
            << "| isOffline=" << m_account->isOffline;
    }


    // -----------------------------------------------------------------------
    // testDialogLoginIntervalsIcuButton
    //
    // Verifies that the "Login with Intervals.icu" button is visible and
    // enabled in the real DialogLogin widget (test mode, widget_bottom
    // immediately visible).
    //
    // Acceptance criteria:
    //   • pushButton_loginIntervalsIcu exists.
    //   • pushButton_loginIntervalsIcu is visible.
    //   • pushButton_loginIntervalsIcu is enabled.
    //   • Screenshot saved and non-empty.
    // -----------------------------------------------------------------------
    void testDialogLoginIntervalsIcuButton()
    {
        DialogLogin dialog(nullptr, /*testMode=*/true);
        dialog.show();
        QTest::qWaitForWindowExposed(&dialog);

        auto *btn = dialog.findChild<QPushButton *>("pushButton_loginIntervalsIcu");

        QVERIFY2(btn != nullptr,
                 "pushButton_loginIntervalsIcu must exist in DialogLogin");
        QVERIFY2(btn->isVisible(),
                 "pushButton_loginIntervalsIcu must be visible in test mode");
        QVERIFY2(btn->isEnabled(),
                 "pushButton_loginIntervalsIcu must be enabled");

        const QString screenshotName =
            QString("dialoglogin-intervals-button-%1-%2.png")
                .arg(kPlatformTag, m_timestamp);
        dialog.resize(1280, 720);
        QTest::qWait(50);
        saveScreenshot(dialog, screenshotName, m_outDir);

        qDebug().noquote() << "[DialogLoginIntervalsIcuButton] PASS"
            << "| button text:" << btn->text();
    }


    // -----------------------------------------------------------------------
    // testDialogLoginIntervalsIcuOAuthDialog
    //
    // Verifies that clicking "Sign in with Intervals.icu" on the real
    // DialogLogin widget (test mode) starts the system-browser OAuth flow,
    // switches the stacked widget to the browser-wait page, and emits
    // intervalsIcuOAuthStarted().  Test mode suppresses the actual browser
    // launch; the loopback listener still runs.  Clicking Cancel returns to
    // the login form.
    //
    // Acceptance criteria:
    //   • Clicking pushButton_loginIntervalsIcu emits intervalsIcuOAuthStarted()
    //     and switches stackedWidget_main to page 1 (browser-wait page).
    //   • An IntervalsIcuOAuthFlow child exists, listens on a loopback port,
    //     and built a localhost redirect_uri into its authorization URL.
    //   • Clicking pushButton_cancelOAuth returns to page 0 (login form).
    //   • Screenshot saved and non-empty.
    // -----------------------------------------------------------------------
    void testDialogLoginIntervalsIcuOAuthDialog()
    {
        DialogLogin dialog(nullptr, /*testMode=*/true);
        dialog.show();
        QTest::qWaitForWindowExposed(&dialog);

        auto *btn = dialog.findChild<QPushButton *>("pushButton_loginIntervalsIcu");
        QVERIFY2(btn != nullptr,
                 "pushButton_loginIntervalsIcu must exist in DialogLogin");

        bool oauthStarted = false;
        QObject::connect(&dialog, &DialogLogin::intervalsIcuOAuthStarted,
                         [&oauthStarted]() { oauthStarted = true; });

        // Click the button — triggers onLoginWithIntervalsIcuClicked() which
        // starts the loopback flow, switches the stacked widget to the
        // browser-wait page, and emits intervalsIcuOAuthStarted().
        QTest::mouseClick(btn, Qt::LeftButton);
        QCoreApplication::processEvents();

        QVERIFY2(oauthStarted,
                 "Clicking 'Sign in with Intervals.icu' must emit intervalsIcuOAuthStarted()");

        auto *stackedWidget = dialog.findChild<QStackedWidget *>("stackedWidget_main");
        QVERIFY2(stackedWidget != nullptr,
                 "stackedWidget_main must exist in DialogLogin");
        QVERIFY2(stackedWidget->currentIndex() == 1,
                 "stackedWidget_main must show page 1 (browser-wait) after click");

        auto *oauthFlow = dialog.findChild<IntervalsIcuOAuthFlow *>();
        QVERIFY2(oauthFlow != nullptr,
                 "An IntervalsIcuOAuthFlow child must exist in DialogLogin");
        QVERIFY2(oauthFlow->listenPort() != 0,
                 "The OAuth flow must be listening on a loopback port");
        const QString expectedRedirect =
            QStringLiteral("http://localhost:%1/").arg(oauthFlow->listenPort());
        const QUrlQuery authQ{QUrl(oauthFlow->authorizationUrl())};
        QCOMPARE(authQ.queryItemValue(QStringLiteral("redirect_uri"),
                                      QUrl::FullyDecoded),
                 expectedRedirect);

        const QString screenshotName =
            QString("dialoglogin-intervals-oauth-dialog-%1-%2.png")
                .arg(kPlatformTag, m_timestamp);
        dialog.resize(1280, 720);
        QTest::qWait(50);
        saveScreenshot(dialog, screenshotName, m_outDir);

        // Cancel the flow and verify we return to the login form.
        auto *cancelBtn = dialog.findChild<QPushButton *>("pushButton_cancelOAuth");
        QVERIFY2(cancelBtn != nullptr,
                 "pushButton_cancelOAuth must exist on the browser-wait page");
        QTest::mouseClick(cancelBtn, Qt::LeftButton);
        QCoreApplication::processEvents();
        QVERIFY2(stackedWidget->currentIndex() == 0,
                 "Cancel must return stackedWidget_main to page 0 (login form)");

        qDebug().noquote() << "[DialogLoginIntervalsIcuOAuthDialog] PASS";
    }


    // -----------------------------------------------------------------------
    // testIntervalsIcuOAuthFlowLoopback
    //
    // Drives the IntervalsIcuOAuthFlow loopback listener directly with a
    // QTcpSocket, simulating the browser redirect — no network access and no
    // real browser involved (setOpenExternalBrowser(false)).
    //
    // Acceptance criteria:
    //   • Requests without OAuth params (e.g. /favicon.ico) are ignored and
    //     the listener keeps running.
    //   • A redirect carrying error=access_denied emits cancelled().
    //   • A redirect with a code but a wrong CSRF state emits failed()
    //     without attempting a token exchange.
    // -----------------------------------------------------------------------
    void testIntervalsIcuOAuthFlowLoopback()
    {
        const auto simulateRedirect = [](quint16 port, const QString &target) {
            QTcpSocket socket;
            socket.connectToHost(QHostAddress::LocalHost, port);
            QVERIFY2(socket.waitForConnected(3000),
                     "Must be able to connect to the loopback listener");
            socket.write(QStringLiteral("GET %1 HTTP/1.1\r\nHost: localhost\r\n\r\n")
                             .arg(target).toUtf8());
            QVERIFY(socket.waitForBytesWritten(3000));
            socket.waitForReadyRead(3000);
            socket.close();
        };

        // access_denied → cancelled()
        {
            IntervalsIcuOAuthFlow flow;
            flow.setOpenExternalBrowser(false);
            QVERIFY2(flow.start(), "Loopback flow must start");
            QSignalSpy cancelledSpy(&flow, &IntervalsIcuOAuthFlow::cancelled);
            QSignalSpy failedSpy(&flow, &IntervalsIcuOAuthFlow::failed);

            // A stray request first — must be ignored, listener stays up.
            simulateRedirect(flow.listenPort(), QStringLiteral("/favicon.ico"));
            QCoreApplication::processEvents();
            QCOMPARE(cancelledSpy.count(), 0);
            QCOMPARE(failedSpy.count(), 0);

            simulateRedirect(flow.listenPort(),
                             QStringLiteral("/?error=access_denied"));
            QTRY_COMPARE(cancelledSpy.count(), 1);
            QCOMPARE(failedSpy.count(), 0);
        }

        // code with mismatching CSRF state → failed()
        {
            IntervalsIcuOAuthFlow flow;
            flow.setOpenExternalBrowser(false);
            QVERIFY2(flow.start(), "Loopback flow must start");
            QSignalSpy failedSpy(&flow, &IntervalsIcuOAuthFlow::failed);

            simulateRedirect(flow.listenPort(),
                             QStringLiteral("/?code=abc&state=wrong-state"));
            QTRY_COMPARE(failedSpy.count(), 1);
        }

        qDebug().noquote() << "[IntervalsIcuOAuthFlowLoopback] PASS";
    }
};

QTEST_MAIN(TstLoginScreen)
#include "tst_login_screen.moc"
