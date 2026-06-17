#include "dialoglogin.h"
#include "ui_dialoglogin.h"

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <QUuid>

#include "logger.h"

#include "updatedialog.h"
#include "versiondao.h"
#include "environnement.h"
#include "util.h"
#include "xmlutil.h"
#include "intervalsicudao.h"
#include "extrequest.h"
#include "credential_store.h"
#include "intervals_icu_oauth_flow.h"

// ─────────────────────────────────────────────────────────────────────────────
// WASM: Emscripten bridge for the OAuth popup flow.
//
// Two C functions are exported to JavaScript:
//   mt_wasm_oauth_code_received_impl(code, state) — called by the message
//     listener when oauth_callback.html posts the authorization code back.
//   mt_trigger_wasm_oauth_login_impl() — Playwright test hook; triggers the
//     OAuth popup exactly as if the user clicked "Sign in with Intervals.icu".
//
// Two EM_JS functions are compiled into the module:
//   js_openOAuthPopup(authUrl) — opens the authorization popup and registers
//     the message listener that calls mt_wasm_oauth_code_received_impl.
//   js_exposeWasmOAuthBridge() — sets window.mt_wasmOAuthReady and
//     window.mt_triggerOAuthLogin() for Playwright and other test harnesses.
// ─────────────────────────────────────────────────────────────────────────────
#ifdef GC_WASM_BUILD
#include <emscripten.h>
#include <QPointer>

static QPointer<DialogLogin> g_loginDialog;

extern "C" {

EMSCRIPTEN_KEEPALIVE
void mt_wasm_oauth_code_received_impl(const char *code, const char *state)
{
    if (!g_loginDialog) return;
    QMetaObject::invokeMethod(g_loginDialog.data(), "onWasmOAuthCodeReceived",
                              Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromUtf8(code)),
                              Q_ARG(QString, QString::fromUtf8(state)));
}

EMSCRIPTEN_KEEPALIVE
void mt_trigger_wasm_oauth_login_impl()
{
    if (!g_loginDialog) return;
    QMetaObject::invokeMethod(g_loginDialog.data(), "onWasmOAuthLoginClicked",
                              Qt::QueuedConnection);
}

} // extern "C"

// Open the Intervals.icu OAuth authorization URL in a browser popup and
// register a message listener to receive the code from oauth_callback.html.
EM_JS(void, js_openOAuthPopup, (const char *authUrlCStr), {
    var authUrl = UTF8ToString(authUrlCStr);

    // Remove any stale listener from a previous login attempt.
    if (window._mtOAuthMsgListener) {
        window.removeEventListener('message', window._mtOAuthMsgListener);
        window._mtOAuthMsgListener = null;
    }

    // Expected origin of the oauth_callback.html page (same GitHub Pages host).
    var expectedOrigin = 'https://maximumtrainer.github.io';

    window._mtOAuthMsgListener = function onOAuthMessage(event) {
        // Reject messages from unexpected origins.
        if (event.origin !== expectedOrigin && event.origin !== window.location.origin)
            return;
        if (!event.data || typeof event.data !== 'object') return;
        if (!Object.prototype.hasOwnProperty.call(event.data, 'mt_oauth_code')) return;

        window.removeEventListener('message', window._mtOAuthMsgListener);
        window._mtOAuthMsgListener = null;

        var code  = String(event.data.mt_oauth_code  || '');
        var state = String(event.data.mt_oauth_state || '');
        var err   = String(event.data.mt_oauth_error || '');

        if (err) {
            // Forward error to C++ as an empty code so the state-mismatch / empty
            // code paths in onWasmOAuthCodeReceived() invoke onOAuthFailed().
            code = '';
        }

        // Marshal code and state strings into Emscripten heap and call C++.
        var enc      = new TextEncoder();
        var codeBuf  = enc.encode(code  + '\0');
        var stateBuf = enc.encode(state + '\0');
        var codePtr  = _malloc(codeBuf.length);
        var statePtr = _malloc(stateBuf.length);
        HEAPU8.set(codeBuf,  codePtr);
        HEAPU8.set(stateBuf, statePtr);
        Module._mt_wasm_oauth_code_received_impl(codePtr, statePtr);
        _free(codePtr);
        _free(statePtr);
    };
    // Register the listener BEFORE opening the popup so that an immediate
    // redirect (e.g., when the user is already logged in at intervals.icu)
    // does not race with listener registration.
    window.addEventListener('message', window._mtOAuthMsgListener);

    var popup = window.open(authUrl, 'mt_oauth_login',
                            'width=600,height=720,menubar=no,toolbar=no,resizable=yes');
    if (!popup) {
        // Popup was blocked.  Remove the listener and send the real OAuth
        // state with an empty code so that the state check in C++ passes and
        // we reach the "empty authorization code" → onOAuthFailed() branch.
        // Sending empty state would incorrectly trigger the CSRF-warning path.
        window.removeEventListener('message', window._mtOAuthMsgListener);
        window._mtOAuthMsgListener = null;

        var stateParam = '';
        try {
            stateParam = new URL(authUrl).searchParams.get('state') || '';
        } catch (e) {}

        var enc      = new TextEncoder();
        var codeBuf  = enc.encode('\0');
        var stateBuf = enc.encode(stateParam + '\0');
        var codePtr  = _malloc(codeBuf.length);
        var statePtr = _malloc(stateBuf.length);
        HEAPU8.set(codeBuf,  codePtr);
        HEAPU8.set(stateBuf, statePtr);
        Module._mt_wasm_oauth_code_received_impl(codePtr, statePtr);
        _free(codePtr);
        _free(statePtr);
    }
});

// Expose the WASM OAuth bridge to JavaScript for testing.
// window.mt_wasmOAuthReady — boolean, true once the login dialog is set up.
// window.mt_triggerOAuthLogin() — triggers the OAuth login popup (Playwright hook).
EM_JS(void, js_exposeWasmOAuthBridge, (), {
    window.mt_wasmOAuthReady = true;

    window.mt_triggerOAuthLogin = function() {
        Module._mt_trigger_wasm_oauth_login_impl();
    };
});
#endif // GC_WASM_BUILD





void DialogLogin::changeEvent(QEvent *event) {

    if (event->type() == QEvent::LanguageChange)
        ui->label_version->setText(Environnement::getVersion() );

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
DialogLogin::DialogLogin(QWidget *parent, bool testMode)
    : QDialog(parent), ui(new Ui::DialogLogin)
{
    ui->setupUi(this);

#ifndef Q_OS_WASM
    // Qt WASM platform plugin does not support setWindowFlags/setParent;
    // omit this call to avoid the "This plugin does not support setParent!" warning.
    this->setWindowFlags(Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
#endif

    gotUpdateDialog              = false;
    replyIntervalsIcuAthlete     = nullptr;
    replyIntervalsIcuSettings    = nullptr;
    replyVersion                 = nullptr;
    m_silentAuthReply            = nullptr;
    m_tokenRefreshReply          = nullptr;
    m_intervalsIcuTimeout        = nullptr;
    m_versionTimeout             = nullptr;
    m_silentAuthTimeout          = nullptr;
    m_pendingIntervalsIcuReplies = 0;
    m_silentAuthCancelled        = false;

    // Loading animation (lives in the footer widget_bottom — always visible)
    movie = new QMovie(":/image/icon/loading", QByteArray(), this);
    movie->setPaused(false);
    ui->label_loading->setMovie(movie);
    movie->setSpeed(150);
    movie->start();

    this->account  = qApp->property("Account").value<Account*>();
    this->settings = qApp->property("User_Settings").value<Settings*>();

    ui->label_version->setText(Environnement::getVersion());

    // Restore the last "work offline" choice so users who always run offline
    // do not have to re-tick the box every launch.
#ifndef Q_OS_WASM
    {
        QSettings s;
        const bool rememberOffline = s.value(QStringLiteral("login/workOffline"), false).toBool();
        ui->checkBox_workOffline->setChecked(rememberOffline);
        ui->pushButton_startOffline->setVisible(rememberOffline);
    }
#endif

    m_testMode = testMode;

    // Browser-wait page (page 1) controls — desktop system-browser OAuth flow.
    connect(ui->pushButton_cancelOAuth, &QPushButton::clicked,
            this, &DialogLogin::onOAuthCancelRequested);
    connect(ui->pushButton_reopenBrowser, &QPushButton::clicked, this, [this]() {
        if (m_oauthFlow)
            QDesktopServices::openUrl(QUrl(m_oauthFlow->authorizationUrl()));
    });

    // Primary sign-in button
    connect(ui->pushButton_loginIntervalsIcu, &QPushButton::clicked,
            this, &DialogLogin::onLoginWithIntervalsIcuClicked);

    // Account-switching links
    connect(ui->pushButton_switchAccount, &QPushButton::clicked,
            this, &DialogLogin::onSwitchAccountClicked);
    connect(ui->pushButton_useDifferentAccount, &QPushButton::clicked,
            this, &DialogLogin::onUseDifferentAccountClicked);

    // Returning-user pill: show when a previous athlete ID is stored
    ui->label_sessionExpired->setVisible(false);
    if (!account->intervals_icu_athlete_id.isEmpty()) {
        ui->widget_returningUser->setVisible(true);
        ui->label_returningPill->setText(
            tr("Signed in as %1").arg(account->intervals_icu_athlete_id));
        ui->label_welcomeHeading->setText(tr("Welcome back"));
    } else {
        ui->widget_returningUser->setVisible(false);
        ui->label_welcomeHeading->setText(tr("Sign in with Intervals.icu"));
    }

#ifdef Q_OS_WASM
    // ── WASM: OAuth popup flow ─────────────────────────────────────────────
    // The system-browser loopback flow (IntervalsIcuOAuthFlow) is desktop-only.
    // On WASM the "Sign in with Intervals.icu" button is rewired to open a
    // browser popup to the authorization URL; the popup redirects to
    // oauth_callback.html which posts the code back via window.opener.postMessage.
    // Offline mode is not available on WASM.
    ui->checkBox_workOffline->setVisible(false);
    ui->pushButton_startOffline->setVisible(false);
    ui->line_separator->setVisible(false);

    // Rewire the Intervals.icu button to the WASM popup handler.
    disconnect(ui->pushButton_loginIntervalsIcu, &QPushButton::clicked,
               this, &DialogLogin::onLoginWithIntervalsIcuClicked);
    connect(ui->pushButton_loginIntervalsIcu, &QPushButton::clicked,
            this, &DialogLogin::onWasmOAuthLoginClicked);

#ifdef GC_WASM_BUILD
    g_loginDialog = this;
    js_exposeWasmOAuthBridge();
#endif
#endif // Q_OS_WASM

    if (testMode) {
        // In test mode skip all network requests.  stackedWidget_main defaults
        // to page 0 (widget_center) from Qt Designer, so widget_center is
        // already "logically visible" and widget_loading is hidden.
        ui->label_loading->setVisible(false);
        return;
    }

    // Check for app updates in the background; never block the login UI.
    // WASM users cannot download a desktop binary, so skip the version check.
    // Source/dev builds report version "0.0.0" (APP_VERSION is only populated
    // from git describe in CI release builds), which would always look outdated
    // and pop the update dialog on every launch — skip the check for them.
#ifndef Q_OS_WASM
    const bool isDevBuild = Environnement::getVersion().remove(QRegularExpression("^[vV]")) == "0.0.0";
    replyVersion = isDevBuild ? nullptr : VersionDAO::getVersion();
    if (replyVersion) {
        connect(replyVersion, &QNetworkReply::finished,
                this, &DialogLogin::slotFinishedGetVersion);
        m_versionTimeout = new QTimer(this);
        m_versionTimeout->setSingleShot(true);
        connect(m_versionTimeout, &QTimer::timeout,
                this, &DialogLogin::onVersionTimeout);
        m_versionTimeout->start(10000);
    }
#endif // Q_OS_WASM

    // Silent session restore: if stored tokens exist, try to validate them
    // against the API before showing the login form.
    const QString storedToken    = account->intervals_icu_access_token;
    const QString storedAthleteId = account->intervals_icu_athlete_id;
    if (!storedToken.isEmpty() && !storedAthleteId.isEmpty()) {
        ui->label_process->setText(tr("Reconnecting to Intervals.icu..."));
        ui->stackedWidget_main->setCurrentIndex(2); // loading/reconnecting page
        m_silentAuthReply = IntervalsIcuDAO::getAthleteBearer(storedAthleteId, storedToken);
        if (m_silentAuthReply) {
            connect(m_silentAuthReply, &QNetworkReply::finished,
                    this, &DialogLogin::onSilentAuthFinished);
            m_silentAuthTimeout = new QTimer(this);
            m_silentAuthTimeout->setSingleShot(true);
            connect(m_silentAuthTimeout, &QTimer::timeout,
                    this, &DialogLogin::onSilentAuthTimeout);
            m_silentAuthTimeout->start(10000);
        } else {
            // Could not create request — fall back to the login form silently
            showLoginForm(false);
        }
    }
    // else: no stored session — stay on page 0 (login form)
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onVersionTimeout() {
    if (!replyVersion) return;
    LOG_WARN("DialogLogin", QStringLiteral("Version check timed out after 10 s – aborting"));
    replyVersion->abort();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::slotFinishedGetVersion() {

    m_versionTimeout->stop();

    if (!replyVersion) return;

    const QNetworkReply::NetworkError netError = replyVersion->error();

    if (netError == QNetworkReply::NoError) {
        const QByteArray arrayData = replyVersion->readAll();
        LOG_DEBUG("DialogLogin", QStringLiteral("Version check response: ") + QString::fromUtf8(arrayData));

        const QString latestVersion = Util::parseJsonObjectVersion(QString::fromUtf8(arrayData));
        LOG_INFO("DialogLogin", QStringLiteral("Current: ") + Environnement::getVersion()
                 + QStringLiteral("  Latest: ") + latestVersion);

        if (!this->isVisible()) {
            // Login already completed (dialog accepted, pending deletion) before
            // the version reply arrived — don't pop an update prompt over the app.
            LOG_INFO("DialogLogin", QStringLiteral("Version check returned after login – skipping update dialog"));
        } else if (!latestVersion.isEmpty() && Util::isVersionNewer(Environnement::getVersion(), latestVersion)) {
            LOG_INFO("DialogLogin", QStringLiteral("Update available – showing dialog"));
            m_updateDialogOpen = true;
            UpdateDialog up(latestVersion, this);
            const int updateChoice = up.exec();   // nested event loop
            m_updateDialogOpen = false;
            if (updateChoice == QDialog::Accepted) {
                gotUpdateDialog = true;
            } else {
                LOG_INFO("DialogLogin", QStringLiteral("User declined update – proceeding to login"));
            }
        }
    } else {
        LOG_WARN("DialogLogin", QStringLiteral("Version check failed – ") + replyVersion->errorString());
    }

    replyVersion->deleteLater();
    replyVersion = nullptr;

    if (gotUpdateDialog) {
        return QDialog::reject();
    }

    // Silent auto-login may have completed while the update dialog's nested loop
    // was running; its completeLogin() was deferred to avoid deleting this dialog
    // from under that loop. Now that the dialog is closed, finish entering the app.
    if (m_loginCompletePending) {
        m_loginCompletePending = false;
        completeLogin();
    }
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Offline mode
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DialogLogin::on_checkBox_workOffline_clicked(bool checked)
{
    ui->pushButton_startOffline->setVisible(checked);

    // Remember the choice so it is pre-selected on the next launch.
    QSettings s;
    s.setValue(QStringLiteral("login/workOffline"), checked);
}



void DialogLogin::on_pushButton_startOffline_clicked()
{
    loginOffline();
}



void DialogLogin::loginOffline()
{
    account->isOffline        = true;
    account->id               = 0;
    account->email            = QStringLiteral("local@offline");
    account->email_clean      = QStringLiteral("offline_user");
    account->display_name     = tr("Local User");
    account->first_name       = tr("Local");
    account->last_name        = tr("User");
    account->subscription_type_id = 1;

    XmlUtil::parseLocalSaveFile(account);

    LOG_INFO("DialogLogin", QStringLiteral("Offline login accepted – running as LocalUser"));
    this->accept();
}


// ─────────────────────────────────────────────────────────────────────────────
// Silent session restore
// ─────────────────────────────────────────────────────────────────────────────

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onSilentAuthTimeout()
{
    LOG_WARN("DialogLogin", QStringLiteral("Silent auth timed out – showing login form"));
    m_silentAuthCancelled = true;
    if (m_silentAuthReply) {
        auto *r = m_silentAuthReply;
        m_silentAuthReply = nullptr;
        r->abort(); r->deleteLater();
    }
    if (m_tokenRefreshReply) {
        auto *r = m_tokenRefreshReply;
        m_tokenRefreshReply = nullptr;
        r->abort(); r->deleteLater();
    }
    showLoginForm(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onSilentAuthFinished()
{
    if (!m_silentAuthReply) return;
    if (m_silentAuthCancelled) {
        m_silentAuthReply->deleteLater();
        m_silentAuthReply = nullptr;
        return;
    }

    if (m_silentAuthTimeout) m_silentAuthTimeout->stop();

    const QNetworkReply::NetworkError netError = m_silentAuthReply->error();
    const int httpStatus = m_silentAuthReply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();

    m_silentAuthReply->deleteLater();
    m_silentAuthReply = nullptr;

    const bool isHttp401 = (httpStatus == 401)
                        || (netError == QNetworkReply::AuthenticationRequiredError);

    if (netError == QNetworkReply::NoError && httpStatus >= 200 && httpStatus < 300) {
        // Token is valid → fetch full profile and log in
        LOG_INFO("DialogLogin", QStringLiteral("Silent auth OK – restoring session"));
        m_loggingInViaIntervalsIcu = true;
        fetchIntervalsIcuDataOAuth();

    } else if (isHttp401 && !account->intervals_icu_refresh_token.isEmpty()) {
        // Token expired → try to refresh it
        LOG_INFO("DialogLogin", QStringLiteral("Silent auth 401 – attempting token refresh"));
        m_tokenRefreshReply = ExtRequest::intervalsIcuOAuthRefresh(
            account->intervals_icu_refresh_token);
        if (m_tokenRefreshReply) {
            connect(m_tokenRefreshReply, &QNetworkReply::finished,
                    this, &DialogLogin::onTokenRefreshFinished);
        } else {
            showLoginForm(false);
        }

    } else if (isHttp401) {
        // Expired and no refresh token available
        LOG_INFO("DialogLogin",
                 QStringLiteral("Silent auth 401 – no refresh token, clearing credentials"));
        clearTokens();
        showLoginForm(true); // show "session expired" notice

    } else {
        // Network error or unexpected status → fail silently
        LOG_WARN("DialogLogin",
                 QStringLiteral("Silent auth network error (%1) – showing login form")
                 .arg(netError));
        showLoginForm(false);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onTokenRefreshFinished()
{
    if (!m_tokenRefreshReply) return;
    if (m_silentAuthCancelled) {
        m_tokenRefreshReply->deleteLater();
        m_tokenRefreshReply = nullptr;
        return;
    }

    const QNetworkReply::NetworkError netError = m_tokenRefreshReply->error();
    const QByteArray data = m_tokenRefreshReply->readAll();
    m_tokenRefreshReply->deleteLater();
    m_tokenRefreshReply = nullptr;

    if (netError == QNetworkReply::NoError && !data.isEmpty()) {
        Util::parseJsonIntervalsIcuOAuthToken(QString::fromUtf8(data));
        if (!account->intervals_icu_access_token.isEmpty()) {
            account->saveIntervalsIcuCredentials();
            LOG_INFO("DialogLogin", QStringLiteral("Token refresh succeeded – restoring session"));
            m_loggingInViaIntervalsIcu = true;
            fetchIntervalsIcuDataOAuth();
            return;
        }
    }

    LOG_WARN("DialogLogin", QStringLiteral("Token refresh failed – clearing credentials"));
    clearTokens();
    showLoginForm(true); // show "session expired" notice
}


// ─────────────────────────────────────────────────────────────────────────────
// Intervals.icu OAuth2 login
// ─────────────────────────────────────────────────────────────────────────────

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Opens the Intervals.icu login in the system browser (loopback redirect)
/// and switches to the browser-wait page (page 1).
void DialogLogin::onLoginWithIntervalsIcuClicked()
{
    LOG_INFO("DialogLogin", QStringLiteral("User clicked 'Sign in with Intervals.icu'"));

    resetOAuthFlow();
    m_oauthFlow = new IntervalsIcuOAuthFlow(this);
    m_oauthFlow->setOpenExternalBrowser(!m_testMode);
    connect(m_oauthFlow, &IntervalsIcuOAuthFlow::succeeded,
            this, &DialogLogin::onOAuthSucceeded);
    connect(m_oauthFlow, &IntervalsIcuOAuthFlow::failed,
            this, &DialogLogin::onOAuthFailed);
    connect(m_oauthFlow, &IntervalsIcuOAuthFlow::cancelled,
            this, &DialogLogin::onOAuthCancelRequested);

    if (!m_oauthFlow->start()) {
        m_oauthFlow->deleteLater();
        m_oauthFlow = nullptr;
        QMessageBox::warning(
            this,
            tr("Intervals.icu Login Failed"),
            tr("Could not start the local sign-in listener. Please try again."));
        return;
    }

    ui->label_sessionExpired->setVisible(false);
    ui->stackedWidget_main->setCurrentIndex(1); // browser-wait page

    emit intervalsIcuOAuthStarted();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onOAuthSucceeded()
{
    LOG_INFO("DialogLogin", QStringLiteral("Intervals.icu OAuth2 authorization successful"));
    resetOAuthFlow();
    m_loggingInViaIntervalsIcu = true;
    fetchIntervalsIcuDataOAuth(); // switches stacked widget to page 2 (loading)
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onOAuthFailed()
{
    LOG_WARN("DialogLogin", QStringLiteral("Intervals.icu OAuth2 authorization denied or failed"));
    resetOAuthFlow();
    showLoginForm(false);
#ifndef Q_OS_WASM
    QMessageBox::warning(
        this,
        tr("Intervals.icu Login Failed"),
        tr("Intervals.icu authorization was denied or did not complete.\n\n"
           "Please click \"Sign in with Intervals.icu\" to try again."));
#else
    ui->pushButton_loginIntervalsIcu->setEnabled(true);
    ui->label_sessionExpired->setText(tr("Authorization failed — please try again."));
    ui->label_sessionExpired->setVisible(true);
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onOAuthCancelRequested()
{
    resetOAuthFlow();
    showLoginForm(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::resetOAuthFlow()
{
    if (!m_oauthFlow)
        return;
    m_oauthFlow->abort();
    m_oauthFlow->deleteLater();
    m_oauthFlow = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onSwitchAccountClicked()
{
    m_silentAuthCancelled = true;
    if (m_silentAuthTimeout) m_silentAuthTimeout->stop();
    if (m_silentAuthReply) {
        auto *r = m_silentAuthReply;
        m_silentAuthReply = nullptr;
        r->abort(); r->deleteLater();
    }
    if (m_tokenRefreshReply) {
        auto *r = m_tokenRefreshReply;
        m_tokenRefreshReply = nullptr;
        r->abort(); r->deleteLater();
    }

    // Cancel any in-flight profile-fetch started by fetchIntervalsIcuDataOAuth().
    m_loggingInViaIntervalsIcu = false;
    m_pendingIntervalsIcuReplies = 0;
    if (m_intervalsIcuTimeout) m_intervalsIcuTimeout->stop();
    if (replyIntervalsIcuAthlete) {
        auto *r = replyIntervalsIcuAthlete;
        replyIntervalsIcuAthlete = nullptr;
        r->abort(); r->deleteLater();
    }
    if (replyIntervalsIcuSettings) {
        auto *r = replyIntervalsIcuSettings;
        replyIntervalsIcuSettings = nullptr;
        r->abort(); r->deleteLater();
    }

    clearTokens();
    account->intervals_icu_athlete_id.clear();
    account->saveIntervalsIcuCredentials(); // persist cleared athlete_id and tokens
    settings->lastLoggedUsername.clear();
    settings->saveGeneralSettings();

    resetOAuthFlow();

    ui->widget_returningUser->setVisible(false);
    ui->label_welcomeHeading->setText(tr("Sign in with Intervals.icu"));
    showLoginForm(false);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onUseDifferentAccountClicked()
{
    onSwitchAccountClicked();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Fetch the athlete profile and training zones using the OAuth2 Bearer token.
/// After success (or timeout), calls loginWithIntervalsIcuIdentity().
void DialogLogin::fetchIntervalsIcuDataOAuth()
{
    const QString bearerToken = account->intervals_icu_access_token;
    if (bearerToken.isEmpty()) {
        LOG_WARN("DialogLogin", QStringLiteral("fetchIntervalsIcuDataOAuth: no bearer token stored"));
        loginWithIntervalsIcuIdentity();
        return;
    }

    LOG_INFO("DialogLogin", QStringLiteral("Fetching Intervals.icu profile via OAuth Bearer token"));
    ui->label_process->setText(tr("Retrieving your Intervals.icu profile..."));
    ui->stackedWidget_main->setCurrentIndex(2); // loading page

    // Use athlete id "0" = current authenticated user.
    const QString athleteId = account->intervals_icu_athlete_id.isEmpty()
                              ? INTERVALS_ICU_CURRENT_USER_ID
                              : account->intervals_icu_athlete_id;

    m_pendingIntervalsIcuReplies = 2;

    replyIntervalsIcuAthlete = IntervalsIcuDAO::getAthleteBearer(athleteId, bearerToken);
    if (replyIntervalsIcuAthlete) {
        connect(replyIntervalsIcuAthlete, &QNetworkReply::finished,
                this, &DialogLogin::slotFinishedIntervalsIcuAthlete);
    } else {
        m_pendingIntervalsIcuReplies--;
    }

    replyIntervalsIcuSettings = IntervalsIcuDAO::getAthleteSettingsBearer(athleteId, bearerToken);
    if (replyIntervalsIcuSettings) {
        connect(replyIntervalsIcuSettings, &QNetworkReply::finished,
                this, &DialogLogin::slotFinishedIntervalsIcuSettings);
    } else {
        m_pendingIntervalsIcuReplies--;
    }

    if (m_pendingIntervalsIcuReplies <= 0) {
        loginWithIntervalsIcuIdentity();
        return;
    }

    // Safety-net timeout.
    if (m_intervalsIcuTimeout) {
        m_intervalsIcuTimeout->stop();
        m_intervalsIcuTimeout->deleteLater();
    }
    m_intervalsIcuTimeout = new QTimer(this);
    m_intervalsIcuTimeout->setSingleShot(true);
    connect(m_intervalsIcuTimeout, &QTimer::timeout,
            this, [this]() {
                if (!m_loggingInViaIntervalsIcu) return;
                LOG_WARN("DialogLogin",
                         QStringLiteral("OAuth profile fetch timed out – proceeding with login"));
                m_pendingIntervalsIcuReplies = 0;
                if (replyIntervalsIcuAthlete) {
                    auto *r = replyIntervalsIcuAthlete;
                    replyIntervalsIcuAthlete = nullptr;
                    r->abort(); r->deleteLater();
                }
                if (replyIntervalsIcuSettings) {
                    auto *r = replyIntervalsIcuSettings;
                    replyIntervalsIcuSettings = nullptr;
                    r->abort(); r->deleteLater();
                }
                loginWithIntervalsIcuIdentity();
            });
    m_intervalsIcuTimeout->start(15000);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Provision the account using the Intervals.icu OAuth identity and complete login.
void DialogLogin::loginWithIntervalsIcuIdentity()
{
    // Guard against double invocation.
    if (!this->isVisible()) return;

    account->isOffline = false;
    account->id        = 0;

    if (!account->intervals_icu_athlete_id.isEmpty()) {
        QString safeId = account->intervals_icu_athlete_id;
        safeId.remove(QRegularExpression(QStringLiteral("[^a-zA-Z0-9]")));
        account->email_clean = QStringLiteral("icu_") + safeId;
        account->email       = account->intervals_icu_athlete_id + QStringLiteral("@intervals.icu");
    } else {
        account->email_clean = QStringLiteral("icu_user");
        account->email       = QStringLiteral("user@intervals.icu");
    }

    account->subscription_type_id = 2;

    if (account->display_name.isEmpty()) {
        if (!account->first_name.isEmpty())
            account->display_name = account->first_name
                                    + (account->last_name.isEmpty()
                                       ? QString()
                                       : QStringLiteral(" ") + account->last_name);
        else
            account->display_name = tr("Intervals.icu User");
    }

    XmlUtil::parseLocalSaveFile(account);

    account->saveIntervalsIcuCredentials();

    if (!account->intervals_icu_athlete_id.isEmpty()) {
        settings->lastLoggedUsername = account->intervals_icu_athlete_id;
        settings->saveGeneralSettings();
    }

    LOG_INFO("DialogLogin",
             QStringLiteral("Intervals.icu OAuth login complete for athlete: ")
             + account->intervals_icu_athlete_id);
    completeLogin();
}


// ─────────────────────────────────────────────────────────────────────────────
// Intervals.icu data retrieval slots
// ─────────────────────────────────────────────────────────────────────────────

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::slotFinishedIntervalsIcuAthlete()
{
    if (!replyIntervalsIcuAthlete) return;

    const QNetworkReply::NetworkError netError = replyIntervalsIcuAthlete->error();
    const int httpStatus = replyIntervalsIcuAthlete->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (netError == QNetworkReply::NoError) {
        const QByteArray data = replyIntervalsIcuAthlete->readAll();
        Util::parseJsonIntervalsIcuAthlete(QString::fromUtf8(data));
        LOG_INFO("DialogLogin", QStringLiteral("Intervals.icu athlete profile retrieved successfully"));
    } else {
        LOG_WARN("DialogLogin",
                 QStringLiteral("Intervals.icu athlete fetch failed: ")
                 + replyIntervalsIcuAthlete->errorString());
    }

    replyIntervalsIcuAthlete->deleteLater();
    replyIntervalsIcuAthlete = nullptr;

    m_pendingIntervalsIcuReplies--;
    if (m_pendingIntervalsIcuReplies <= 0 && m_loggingInViaIntervalsIcu) {
        if (m_intervalsIcuTimeout) m_intervalsIcuTimeout->stop();
        loginWithIntervalsIcuIdentity();
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::slotFinishedIntervalsIcuSettings()
{
    if (!replyIntervalsIcuSettings) return;

    if (replyIntervalsIcuSettings->error() == QNetworkReply::NoError) {
        const QByteArray data = replyIntervalsIcuSettings->readAll();
        if (Util::parseJsonIntervalsIcuSettings(QString::fromUtf8(data))) {
            // FTP/LTHR pulled from the intervals.icu profile: persist them and
            // record the sync so Preferences can show where the values came from.
            account->saveProfileFields(account->FTP, account->LTHR, account->weight_kg);
            QSettings().setValue(QStringLiteral("intervalsIcu/profileSyncedAt"),
                                 QDateTime::currentDateTime().toString(Qt::ISODate));
            LOG_INFO("DialogLogin",
                     QStringLiteral("Profile synced from intervals.icu: FTP %1 W, LTHR %2 bpm")
                         .arg(account->FTP).arg(account->LTHR));
        } else {
            LOG_WARN("DialogLogin",
                     QStringLiteral("Intervals.icu sport settings carried no FTP/LTHR to apply"));
        }
    } else {
        LOG_WARN("DialogLogin",
                 QStringLiteral("Intervals.icu settings fetch failed: ")
                 + replyIntervalsIcuSettings->errorString());
    }

    replyIntervalsIcuSettings->deleteLater();
    replyIntervalsIcuSettings = nullptr;

    m_pendingIntervalsIcuReplies--;
    if (m_pendingIntervalsIcuReplies <= 0 && m_loggingInViaIntervalsIcu) {
        if (m_intervalsIcuTimeout) m_intervalsIcuTimeout->stop();
        loginWithIntervalsIcuIdentity();
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::showLoginForm(bool showExpiredMessage)
{
    ui->stackedWidget_main->setCurrentIndex(0);
    ui->label_sessionExpired->setVisible(showExpiredMessage);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::clearTokens()
{
    CredentialStore::remove("intervals_icu", "access_token");
    CredentialStore::remove("intervals_icu", "refresh_token");
    account->intervals_icu_access_token.clear();
    account->intervals_icu_refresh_token.clear();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::completeLogin()
{
    if (!this->isVisible()) return;

    if (m_updateDialogOpen) {
        // The update dialog's nested event loop is running. Accepting now would
        // emit accepted() → launchMainWindow()/deleteLater() and destroy this
        // dialog from inside slotFinishedGetVersion (use-after-free). Defer until
        // the update dialog closes; slotFinishedGetVersion() will finish login.
        LOG_INFO("DialogLogin", QStringLiteral("Login complete – deferred until update dialog closes"));
        m_loginCompletePending = true;
        return;
    }

    LOG_INFO("DialogLogin", QStringLiteral("Login complete – entering application"));
    this->accept();
}

// ─────────────────────────────────────────────────────────────────────────────
// WASM OAuth popup login
// ─────────────────────────────────────────────────────────────────────────────
#ifdef Q_OS_WASM

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Opens the Intervals.icu OAuth authorization page in a browser popup.
/// The popup redirects to oauth_callback.html which posts the authorization
/// code back to this window via window.opener.postMessage, which is then
/// forwarded to onWasmOAuthCodeReceived() via the Emscripten bridge.
void DialogLogin::onWasmOAuthLoginClicked()
{
    LOG_INFO("DialogLogin",
             QStringLiteral("WASM: opening Intervals.icu OAuth popup"));

    // Generate and store a fresh CSRF state token for this login attempt.
    m_wasmOAuthState = QUuid::createUuid().toString(QUuid::Id128).left(16);

    const QString authUrl = Environnement::getURLIntervalsIcuAuthorizeWasm(m_wasmOAuthState);

    ui->label_sessionExpired->setVisible(false);
    ui->pushButton_loginIntervalsIcu->setEnabled(false);

#ifdef GC_WASM_BUILD
    js_openOAuthPopup(authUrl.toUtf8().constData());
#else
    Q_UNUSED(authUrl)
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Called (via QMetaObject::invokeMethod) when the OAuth popup posts the
/// authorization code back to this window.  Validates the CSRF state, then
/// starts the token exchange.
void DialogLogin::onWasmOAuthCodeReceived(const QString &code, const QString &state)
{
    if (state != m_wasmOAuthState) {
        LOG_WARN("DialogLogin",
                 QStringLiteral("WASM OAuth: state mismatch — possible CSRF attempt (ignored)"));
        ui->pushButton_loginIntervalsIcu->setEnabled(true);
        ui->label_sessionExpired->setText(tr("Authorization failed — please try again."));
        ui->label_sessionExpired->setVisible(true);
        return;
    }
    if (code.isEmpty()) {
        LOG_WARN("DialogLogin", QStringLiteral("WASM OAuth: empty authorization code"));
        onOAuthFailed();
        return;
    }
    LOG_INFO("DialogLogin", QStringLiteral("WASM OAuth: authorization code received, exchanging for tokens"));
    ui->label_process->setText(tr("Signing in with Intervals.icu..."));
    ui->stackedWidget_main->setCurrentIndex(2); // loading page

    const QString redirectUri = Environnement::getWasmOAuthRedirectUri();
    m_wasmTokenReply = ExtRequest::intervalsIcuOAuthExchange(code, redirectUri);
    if (!m_wasmTokenReply) {
        LOG_WARN("DialogLogin", QStringLiteral("WASM OAuth: failed to create token exchange request"));
        ui->pushButton_loginIntervalsIcu->setEnabled(true);
        showLoginForm(true);
        return;
    }
    connect(m_wasmTokenReply, &QNetworkReply::finished,
            this, &DialogLogin::onWasmOAuthTokenExchangeFinished);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Called when the token exchange request finishes.  Parses the Bearer token,
/// stores credentials, and proceeds to fetch the athlete profile.
void DialogLogin::onWasmOAuthTokenExchangeFinished()
{
    if (!m_wasmTokenReply) return;
    auto *reply = m_wasmTokenReply;
    m_wasmTokenReply = nullptr;

    // Read all data before calling deleteLater() to avoid use-after-free.
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QNetworkReply::NetworkError netErr = reply->error();
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    if (netErr != QNetworkReply::NoError || (httpStatus != 0 && httpStatus != 200)) {
        LOG_WARN("DialogLogin",
                 QStringLiteral("WASM OAuth: token exchange failed (HTTP %1, err %2)")
                 .arg(httpStatus).arg(netErr));
        onOAuthFailed();
        return;
    }

    // parseJsonIntervalsIcuOAuthToken() writes the access token into the
    // Account singleton; check that the token is now populated.
    Util::parseJsonIntervalsIcuOAuthToken(QString::fromUtf8(body));
    if (account->intervals_icu_access_token.isEmpty()) {
        LOG_WARN("DialogLogin",
                 QStringLiteral("WASM OAuth: token parse failed or access_token is empty"));
        onOAuthFailed();
        return;
    }

    account->saveIntervalsIcuCredentials();
    onOAuthSucceeded();
}

#endif // Q_OS_WASM

