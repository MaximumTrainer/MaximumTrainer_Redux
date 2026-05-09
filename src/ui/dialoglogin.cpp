#include "dialoglogin.h"
#include "ui_dialoglogin.h"

#include <QDebug>
#include <QMessageBox>
#include <QRegularExpression>
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
#include "intervalsicuoauthwidget.h"




void DialogLogin::changeEvent(QEvent *event) {

    if (event->type() == QEvent::LanguageChange)
        ui->label_version->setText(Environnement::getVersion() );

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
DialogLogin::DialogLogin(QWidget *parent, bool testMode)
    : QDialog(parent), ui(new Ui::DialogLogin)
{
    ui->setupUi(this);

    Qt::WindowFlags flags(Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    this->setWindowFlags(flags);

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

    ui->comboBox_language->setCurrentIndex(settings->language_index);
    ui->label_version->setText(Environnement::getVersion());

    // Create the embedded OAuth widget and insert it into the placeholder page.
    m_oauthWidget = new IntervalsIcuOAuthWidget(this);
    ui->widget_oauthPage->layout()->addWidget(m_oauthWidget);

    connect(m_oauthWidget, &IntervalsIcuOAuthWidget::authSucceeded,
            this, &DialogLogin::onOAuthSucceeded);
    connect(m_oauthWidget, &IntervalsIcuOAuthWidget::authFailed,
            this, &DialogLogin::onOAuthFailed);
    connect(m_oauthWidget, &IntervalsIcuOAuthWidget::cancelRequested,
            this, &DialogLogin::onOAuthCancelRequested);

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

    if (testMode) {
        // In test mode skip all network requests.  stackedWidget_main defaults
        // to page 0 (widget_center) from Qt Designer, so widget_center is
        // already "logically visible" and widget_loading is hidden.
        ui->label_loading->setVisible(false);
        return;
    }

    // Check for app updates in the background; never block the login UI.
    replyVersion = VersionDAO::getVersion();
    if (replyVersion) {
        connect(replyVersion, &QNetworkReply::finished,
                this, &DialogLogin::slotFinishedGetVersion);
        m_versionTimeout = new QTimer(this);
        m_versionTimeout->setSingleShot(true);
        connect(m_versionTimeout, &QTimer::timeout,
                this, &DialogLogin::onVersionTimeout);
        m_versionTimeout->start(10000);
    }

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

        if (!latestVersion.isEmpty() && Util::isVersionNewer(Environnement::getVersion(), latestVersion)) {
            LOG_INFO("DialogLogin", QStringLiteral("Update available – showing dialog"));
            UpdateDialog up(latestVersion, this);
            if (up.exec() == QDialog::Accepted) {
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
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Offline mode
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void DialogLogin::on_checkBox_workOffline_clicked(bool checked)
{
    if (checked) {
        ui->pushButton_startOffline->setVisible(true);
    } else {
        ui->pushButton_startOffline->setVisible(false);
    }
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
/// Switches to the embedded OAuth WebView (page 1) and loads the auth URL.
void DialogLogin::onLoginWithIntervalsIcuClicked()
{
    LOG_INFO("DialogLogin", QStringLiteral("User clicked 'Sign in with Intervals.icu'"));

    const QString oauthState = QUuid::createUuid().toString(QUuid::Id128).left(16);
    const QString oauthUrl   = Environnement::getURLIntervalsIcuAuthorize(oauthState);

    ui->label_sessionExpired->setVisible(false);
    m_oauthWidget->startAuth(oauthUrl, oauthState);
    ui->stackedWidget_main->setCurrentIndex(1); // OAuth WebView page

    emit intervalsIcuOAuthStarted();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onOAuthSucceeded()
{
    LOG_INFO("DialogLogin", QStringLiteral("Intervals.icu OAuth2 authorization successful"));
    m_loggingInViaIntervalsIcu = true;
    fetchIntervalsIcuDataOAuth(); // switches stacked widget to page 2 (loading)
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onOAuthFailed()
{
    LOG_WARN("DialogLogin", QStringLiteral("Intervals.icu OAuth2 authorization denied or failed"));
    m_oauthWidget->reset();
    showLoginForm(false);
    QMessageBox::warning(
        this,
        tr("Intervals.icu Login Failed"),
        tr("Intervals.icu authorization was denied or did not complete.\n\n"
           "Please click \"Sign in with Intervals.icu\" to try again, or check the "
           "\"Work Offline\" box to continue without an Intervals.icu account."));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onOAuthCancelRequested()
{
    showLoginForm(false);
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

    m_oauthWidget->reset();

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


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::on_comboBox_language_currentIndexChanged(int index) {

    Q_UNUSED(index);

    QString languageToPut = ui->comboBox_language->currentText();

    qApp->removeTranslator(&m_translator);

    bool success;
    if (languageToPut == "English") {
        success = m_translator.load(":/language/language/powervelo_en.qm");
        settings->language_index = 0;
        settings->language = "en";
        settings->saveLanguage();
    }
    else if (languageToPut == "Français") {
        success = m_translator.load(":/language/language/powervelo_fr.qm");
        settings->language_index = 1;
        settings->language = "fr";
        settings->saveLanguage();
    }
    else {
        success = m_translator.load(":/language/language/powervelo_en.qm");
        settings->language_index = 0;
        settings->language = "en";
    }

    if(success) {
        qApp->installTranslator(&m_translator);
        ui->retranslateUi(this);
        m_currLang = languageToPut;
    }
}



// ─────────────────────────────────────────────────────────────────────────────
// Intervals.icu data retrieval slots
// ─────────────────────────────────────────────────────────────────────────────

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::slotFinishedIntervalsIcuAthlete()
{
    if (!replyIntervalsIcuAthlete) return;

    if (replyIntervalsIcuAthlete->error() == QNetworkReply::NoError) {
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
        Util::parseJsonIntervalsIcuSettings(QString::fromUtf8(data));
        LOG_INFO("DialogLogin", QStringLiteral("Intervals.icu training zones retrieved successfully"));
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

    LOG_INFO("DialogLogin", QStringLiteral("Login complete – entering application"));
    this->accept();
}
