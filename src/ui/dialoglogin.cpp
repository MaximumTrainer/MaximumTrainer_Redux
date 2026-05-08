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
#include "dialoginfowebview.h"




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

    gotUpdateDialog = false;
    replyIntervalsIcuAthlete  = nullptr;
    replyIntervalsIcuSettings = nullptr;
    replyVersion              = nullptr;
    m_intervalsIcuTimeout     = nullptr;
    m_versionTimeout          = nullptr;
    m_pendingIntervalsIcuReplies = 0;

    ///Set loading icon
    movie = new QMovie(":/image/icon/loading", QByteArray(), this);
    movie->setPaused(false);
    ui->label_loading->setMovie(movie);
    movie->setSpeed(150);
    movie->start();

    this->account = qApp->property("Account").value<Account*>();
    this->settings = qApp->property("User_Settings").value<Settings*>();

    ///Remember me, language, version
    ui->checkBox_autoLogin->setChecked(settings->rememberMyPassword);
    ui->comboBox_language->setCurrentIndex(settings->language_index);
    ui->label_version->setText(Environnement::getVersion() );

    // "Login with Intervals.icu" button and Return-key aliases
    connect(ui->pushButton_loginIntervalsIcu, &QPushButton::clicked,
            this, &DialogLogin::onLoginWithIntervalsIcuClicked);
    connect(ui->lineEdit_athleteEmail, &QLineEdit::returnPressed,
            this, &DialogLogin::onLoginWithIntervalsIcuClicked);

    // Pre-populate the email/athlete-ID field from previously saved credentials.
    // Prefer the Intervals.icu athlete ID (persisted by saveIntervalsIcuCredentials),
    // falling back to the last remembered username from general settings.
    if (!account->intervals_icu_athlete_id.isEmpty()) {
        // Show the stored athlete ID so the user can see which account will be used.
        ui->lineEdit_athleteEmail->setText(account->intervals_icu_athlete_id);
    } else if (!settings->lastLoggedUsername.isEmpty()) {
        ui->lineEdit_athleteEmail->setText(settings->lastLoggedUsername);
    }

    if (testMode) {
        // In test mode: skip all network requests and immediately reveal the
        // center widget and bottom widget so tests can interact with buttons.
        ui->label_loading->setVisible(false);
        ui->widget_loading->setVisible(false);
        ui->widget_center->setVisible(true);
        ui->widget_bottom->setVisible(true);
        return;
    }

    // Show the form immediately — no loading spinner needed at startup.
    ui->widget_loading->setVisible(false);
    ui->widget_center->setVisible(true);
    ui->widget_bottom->setVisible(true);

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
    // id=0 is used here as a placeholder (no database record for offline users).
    // Intervals.icu OAuth users also use id=0 with isOffline=false; distinguish
    // the two cases using isOffline, not id alone.
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
// Intervals.icu OAuth2 login
// ─────────────────────────────────────────────────────────────────────────────

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Opens a DialogInfoWebView with the Intervals.icu OAuth2 authorization URL.
/// The dialog emits intervalsIcuLinked(true) when the token has been obtained.
void DialogLogin::onLoginWithIntervalsIcuClicked()
{
    LOG_INFO("DialogLogin", QStringLiteral("User clicked 'Login with Intervals.icu'"));

    // Generate a per-login CSRF state token (64 bits of entropy, 16 hex chars).
    const QString oauthState = QUuid::createUuid().toString(QUuid::Id128).left(16);

    DialogInfoWebView *oauthDialog = new DialogInfoWebView(this);
    oauthDialog->setAttribute(Qt::WA_DeleteOnClose);
    oauthDialog->setTitle(tr("Login with Intervals.icu"));
    oauthDialog->setUsedForIntervalsIcu(true);
    oauthDialog->setExpectedOAuthState(oauthState);
    const QString oauthUrl = Environnement::getURLIntervalsIcuAuthorize(oauthState);

    connect(oauthDialog, &DialogInfoWebView::intervalsIcuLinked,
            this, &DialogLogin::onIntervalsIcuOAuthLinked);
    connect(oauthDialog, &DialogInfoWebView::rejected,
            this, &DialogLogin::onIntervalsIcuOAuthDialogRejected);

    // Defer the URL load until after exec() starts its event loop and the
    // dialog has been painted on screen.
    QTimer::singleShot(0, oauthDialog, [oauthDialog, oauthUrl]() {
        oauthDialog->setUrlWebView(oauthUrl);
    });

    // Notify test observers before entering exec() — the signal fires
    // synchronously so tests can reject the dialog before exec() blocks.
    emit intervalsIcuOAuthDialogCreated(oauthDialog);

    oauthDialog->exec();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Called when the Intervals.icu OAuth dialog reports a result.
void DialogLogin::onIntervalsIcuOAuthLinked(bool linked)
{
    if (!linked) {
        LOG_WARN("DialogLogin", QStringLiteral("Intervals.icu OAuth2 authorization denied or failed"));
        QMessageBox::warning(
            this,
            tr("Intervals.icu Login Failed"),
            tr("Intervals.icu authorization was denied or did not complete.\n\n"
               "Please click \"Login with Intervals.icu\" to try again, or check the "
               "\"Work Offline\" box to continue without an Intervals.icu account."));
        return;
    }

    LOG_INFO("DialogLogin", QStringLiteral("Intervals.icu OAuth2 authorization successful"));
    m_loggingInViaIntervalsIcu = true;
    fetchIntervalsIcuDataOAuth();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Called when the user closes the OAuth dialog without completing authorization.
void DialogLogin::onIntervalsIcuOAuthDialogRejected()
{
    if (!m_loggingInViaIntervalsIcu) {
        // User simply closed the window — no action needed, stay on login page.
        LOG_INFO("DialogLogin", QStringLiteral("Intervals.icu OAuth dialog closed by user"));
    }
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
    ui->widget_loading->setVisible(true);
    ui->widget_center->setVisible(false);

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

    // Safety-net timeout.  Stop and replace any existing timer.
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
    // id=0 is a placeholder — no database record exists for OAuth-only users.
    // Offline users also have id=0; distinguish them via isOffline, not id alone.
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

    // Persist the OAuth tokens and athlete ID for the next session.
    account->saveIntervalsIcuCredentials();

    // Also persist the athlete ID/email in general settings so it pre-populates
    // the login form on the next app start.
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
void DialogLogin::on_checkBox_autoLogin_clicked(bool checked)
{
    settings->rememberMyPassword = checked;
    settings->saveGeneralSettings();
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
void DialogLogin::completeLogin()
{
    if (!this->isVisible()) return;

    LOG_INFO("DialogLogin", QStringLiteral("Login complete – entering application"));
    this->accept();
}
