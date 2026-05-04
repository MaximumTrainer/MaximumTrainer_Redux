#include "dialoglogin.h"
#include "ui_dialoglogin.h"

#include <QDebug>
#include <QMessageBox>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QUuid>

#include "logger.h"



#include "updatedialog.h"
#include "userdao.h"
#include "versiondao.h"
#include "extrequest.h"
#include "environnement.h"
#include "util.h"
#include "simplecrypt.h"
#include "xmlutil.h"
#include "userdao.h"
#include "intervalsicudao.h"
#include "dialoginfowebview.h"
#include "myqwebenginepage.h"




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

    // External links
    QStringList lstExternal = {"signup", "forgotpw" };
    MyQWebEnginePage *myPage = new MyQWebEnginePage(ui->webView_login);
    myPage->setExternalList(lstExternal);
    ui->webView_login->setPage(myPage);


    gotUpdateDialog = false;
    firstLogin = true;
    replyIntervalsIcuAthlete  = nullptr;
    replyIntervalsIcuSettings = nullptr;
    replyGoogle               = nullptr;
    replyVersion              = nullptr;
    m_intervalsIcuTimeout     = nullptr;
    m_googleTimeout           = nullptr;
    m_versionTimeout          = nullptr;
    m_pendingIntervalsIcuReplies = 0;


    ///Set loading icon (wait for login page to show)
    QMovie *movie = new QMovie(":/image/icon/loading", QByteArray(), this);
    movie->setPaused(false);
    ui->label_loading->setMovie(movie);
    movie->setSpeed(150);
    movie->start();
    /////////////////////////////////////////////////////////////////



    this->account = qApp->property("Account").value<Account*>();
    this->settings = qApp->property("User_Settings").value<Settings*>();


    ///Remember me, language, version
    ui->checkBox_autoLogin->setChecked(settings->rememberMyPassword);
    ui->comboBox_language->setCurrentIndex(settings->language_index);
    ui->label_version->setText(Environnement::getVersion() );


    // "Login with Intervals.icu" button
    connect(ui->pushButton_loginIntervalsIcu, &QPushButton::clicked,
            this, &DialogLogin::onLoginWithIntervalsIcuClicked);
    // label_registerIntervalsIcu uses openExternalLinks=true in the .ui, so
    // no extra linkActivated connection is needed here.

    if (testMode) {
        // In test mode: skip all network requests and immediately reveal the
        // bottom widget so tests can interact with buttons without waiting for
        // real network activity.  The web view and loading indicator are hidden
        // exactly as they would be after loginLoaded(false).
        ui->label_loading->setVisible(false);
        ui->webView_login->setVisible(false);
        ui->widget_loading->setVisible(false);
        ui->widget_bottom->setVisible(true);
        ui->label_process->setText(tr("Test mode – offline ready"));
        return;
    }

    // --------------------------------------------
    /// CHECK FOR INTERNET CONNECTION
    ui->label_process->setText(tr("Connecting to server..."));
    replyGoogle = ExtRequest::checkGoogleConnection();
    if (replyGoogle) {
        connect(replyGoogle, SIGNAL(finished()), this, SLOT(slotFinishedGoogle()));
        // Abort the Google connectivity check if it doesn't complete within 10 s.
        m_googleTimeout = new QTimer(this);
        m_googleTimeout->setSingleShot(true);
        connect(m_googleTimeout, &QTimer::timeout, this, &DialogLogin::onGoogleTimeout);
        m_googleTimeout->start(10000);
    }



    /// Check if new version APP available
    replyVersion = VersionDAO::getVersion();
    if (replyVersion) {
        connect(replyVersion, SIGNAL(finished()), this, SLOT(slotFinishedGetVersion()));
        // Safety-net: if the version check doesn't complete within 10 s (e.g. network
        // latency, firewall, TLS failure) abort the request and proceed to login so the
        // app never hangs on this dialog.
        m_versionTimeout = new QTimer(this);
        m_versionTimeout->setSingleShot(true);
        connect(m_versionTimeout, &QTimer::timeout, this, &DialogLogin::onVersionTimeout);
        m_versionTimeout->start(10000);
    }

    if (!replyGoogle && !replyVersion) {
        // No network manager available — skip loading sequence and show the
        // bottom widget directly so the dialog is always usable.
        QTimer::singleShot(0, this, [this]() { loginLoaded(false); });
    }

    connect(ui->webView_login, SIGNAL(loadProgress(int)), this, SLOT(showLoadingProgress(int)));

    ui->label_loading->setVisible(false);
    ui->webView_login->setVisible(false);
    ui->widget_bottom->setVisible(false);
    ui->widget_loading->setVisible(true);

}




/////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::clearLstUsername() {

    qDebug() << "CLEAR LST!";


    for (int i=0; i<settings->lstUsername.size(); i++) {
        settings->lstUsername.replace(i, "");
    }
    settings->saveGeneralSettings();

}



/////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::showLoadingProgress(int prog) {
    qDebug() << "show loading icon for this url" << ui->webView_login->url() << "prog" << prog;

    if (prog != 100)
        ui->label_loading->setVisible(true);
    else
        ui->label_loading->setVisible(false);

}



/////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::loginLoaded(bool ok){
    if (!ok) {
        // The login page failed to load (common when offline). Still reveal
        // widget_bottom so the "Work Offline" checkbox and "Start Offline"
        // button remain accessible.
        ui->widget_loading->setVisible(false);
        ui->widget_bottom->setVisible(true);
        return;
    }

    ui->widget_loading->setVisible(false);
    ui->label_loading->setVisible(false);
    ui->webView_login->setVisible(true);
    ui->widget_bottom->setVisible(true);

    qDebug() << "got here login loaded!" << ui->webView_login->url().toString();
    if (ui->webView_login->url().toDisplayString().contains("insideMT#"))  {
        qDebug() << "ok ignore this!";
        return;
    }

    /// ------------------------------- Login sucess! ---------------------------------------
    if (ui->webView_login->url().toDisplayString().contains("/account_rest/"))  {
        qDebug() << "Parse Json Object Account";
        ///  ------------------ Parse JSON object  ----------------------------

        //        this->setVisible(false);

        ui->webView_login->page()->toPlainText([=](const QString &response){
            qDebug() << "HERE IS THE RESP" << response;

            // Add data inside global variable "Account" and "Settings"
            Util::parseJsonObjectAccount(response);

            // Add local settings data (Folder, etc.) to "Account" object
            XmlUtil::parseLocalSaveFile(account);


            // Could not update session_mt_id, should not happen unless problem with DB
            if (account->id == -1)  {
                ui->webView_login->load(QUrl(Environnement::getUrlLogin()));
                return;
            }


            ///------------------ Good login, save password locally if 'remember me' is checked ---------------
            QString pw = account->password;
            SimpleCrypt crypto(Q_UINT64_C(0xdd85116f2b81d85f)); //some random number
            QString encryptedPw = crypto.encryptToString(pw);
            //        QString decrypted = crypto.decryptToString(encryptedPw);
            //        qDebug() << pw << endl << encryptedPw << endl << decrypted;

            //        foreach (QString username, settings->lstUsername)
            //            qDebug() << "User:" << username;


            if (!settings->lstUsername.contains(account->email)) {
                settings->lstUsername.append(account->email);
                if (settings->lstUsername.size() > 5)
                    settings->lstUsername.removeFirst();
            }

            //        foreach (QString username, settings->lstUsername)
            //            qDebug() << "User2:" << username;

            settings->lastLoggedUsername = account->email;
            settings->lastLoggedKey = encryptedPw;
            settings->saveGeneralSettings();

            fetchIntervalsIcuData();
        });

    }


    /// ----------------- Fill default email and set focus on password field ----------------
    /// if first login
    else  {

        // Fill other username used
        //lstUsernameSorted.sort(Qt::CaseInsensitive);

        QString jsToExecute;
        for (int i=0; i<settings->lstUsername.size(); i++) {
            QString username = settings->lstUsername.at(i);
            QString liToAdd = QString("\"<li class='li-username'><a>%1</a></li>\"").arg(username);
            jsToExecute += QString("$(\"#dropdown-select-username\").prepend(%1);").arg(liToAdd);
        }
        //        ui->webView_login->page()->mainFrame()->documentElement().evaluateJavaScript(jsToExecute + "; null" );
        ui->webView_login->page()->runJavaScript(jsToExecute);
        //--------------------



        //more than 1 user in the list? show caret
        if (settings->lstUsername.size() <= 1) {
            QString jsToExecute = "$(\"#button-caret-username\").attr('disabled', true);";
            //            ui->webView_login->page()->mainFrame()->documentElement().evaluateJavaScript(jsToExecute + "; null" );
            ui->webView_login->page()->runJavaScript(jsToExecute);
        }
        else {
            QString jsToExecute = "$(\"#button-caret-username\").attr('disabled', false);";

            QString liToAdd = "\"<li class='divider'></li>\"";
            jsToExecute += QString("$(\"#dropdown-select-username\").append(%1);").arg(liToAdd);

            QString clearList = tr("Clear List");
            liToAdd  = QString("\"<li class='li-username' data-id='clearer'><a>%1</a></li>\"").arg(clearList);;
            jsToExecute += QString("$(\"#dropdown-select-username\").append(%1);").arg(liToAdd);
            //            ui->webView_login->page()->mainFrame()->documentElement().evaluateJavaScript(jsToExecute + "; null" );
            ui->webView_login->page()->runJavaScript(jsToExecute);
        }



        qDebug() << "LAST LOGGER USER NAME IS" << settings->lastLoggedUsername;
        if (settings->lastLoggedUsername != "") {

            /// Name
            if (firstLogin) {


                // Fill Last logged Username
                QString jsToExecute = QString("$('#email').val( '%1' ); ").arg((settings->lastLoggedUsername));
                //                ui->webView_login->page()->mainFrame()->documentElement().evaluateJavaScript(jsToExecute + "; null" );
                qDebug() << "OK we should set it here! pw" << jsToExecute;
                ui->webView_login->page()->runJavaScript(jsToExecute);


                if (settings->rememberMyPassword && settings->lastLoggedKey != "") {


                    // Decrypt password
                    SimpleCrypt crypto(Q_UINT64_C(0xdd85116f2b81d85f)); //some random number
                    QString decrypted = crypto.decryptToString(settings->lastLoggedKey);
                    //                qDebug() << "TEST:" << savedEncryptedPassword << endl << decrypted;

                    // Fill password and click on submit
                    QString jsToExecute = QString("$('#password').val( '%1' ); ").arg((decrypted));
                    jsToExecute += "$('#login-btn').click(); ";
                    //
                    qDebug() << "OK AUTOFILL PARSSWORD WITH " << jsToExecute;
                    ui->webView_login->page()->runJavaScript(jsToExecute);
                }

                ui->webView_login->setFocus();
                QKeyEvent *event = new QKeyEvent ( QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
                QCoreApplication::postEvent (ui->webView_login, event);

                firstLogin = false;
            }
        }
    }


}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onVersionTimeout() {
    if (!replyVersion) return;
    LOG_WARN("DialogLogin", QStringLiteral("Version check timed out after 10 s – aborting and proceeding to login"));
    ui->label_process->setText(tr("Version check timed out, loading login…"));
    // Aborting emits finished() → slotFinishedGetVersion() handles the rest.
    replyVersion->abort();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onGoogleTimeout() {
    if (!replyGoogle) return;
    LOG_WARN("DialogLogin", QStringLiteral("Connectivity check timed out after 10 s – aborting and offering offline mode"));
    ui->label_process->setText(tr("Connection timed out."));
    // Aborting emits finished() → slotFinishedGoogle() handles the rest.
    replyGoogle->abort();
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
            ui->label_process->setText(tr("Update available!"));
            UpdateDialog up(latestVersion, this);
            if (up.exec() == QDialog::Accepted) {
                // User chose to download — browser has been opened.
                gotUpdateDialog = true;
            } else {
                // User declined the update — fall through to normal login.
                LOG_INFO("DialogLogin", QStringLiteral("User declined update – proceeding to login"));
            }
        }

        // Cleanup the version reply regardless of outcome.
        replyVersion->deleteLater();
        replyVersion = nullptr;

        if (gotUpdateDialog) {
            // Abort the Google connectivity check — it is no longer needed.
            if (replyGoogle) {
                m_googleTimeout->stop();
                replyGoogle->abort();
                replyGoogle->deleteLater();
                replyGoogle = nullptr;
            }
            // Close the login dialog so the user can finish the download.
            return QDialog::reject();
        }

        // No update available, or user declined — load the login page.
        ui->label_process->setText(tr("Loading page..."));
        ui->webView_login->setUrl(QUrl(Environnement::getUrlLogin()));
        connect(ui->webView_login, SIGNAL(loadFinished(bool)), this, SLOT(loginLoaded(bool)));

        return;

    } else {
        const QString errMsg = replyVersion->errorString();
        LOG_WARN("DialogLogin", QStringLiteral("Version check failed (") + errMsg
                 + QStringLiteral(") – proceeding to login"));
        ui->label_process->setText(tr("Version check failed, loading login…"));
        // Don't block the user — proceed to login even if the check fails.
        ui->webView_login->setUrl(QUrl(Environnement::getUrlLogin()));
        connect(ui->webView_login, SIGNAL(loadFinished(bool)), this, SLOT(loginLoaded(bool)));
    }

    replyVersion->deleteLater();
    replyVersion = nullptr;
}





//----------------------------------------------------------------------------------------
void DialogLogin::slotFinishedGoogle() {

    m_googleTimeout->stop();

    //success, process data
    if (replyGoogle->error() == QNetworkReply::NoError) {
        ui->label_process->setText(tr("Checking for updates..."));
    }
    else {
        LOG_WARN("DialogLogin",
                 QStringLiteral("Internet connectivity check failed: ")
                 + replyGoogle->errorString());
        LOG_WARN("DialogLogin", QStringLiteral("No internet connection – offering offline mode"));

        const int choice = QMessageBox::question(
            this,
            tr("No Internet Connection"),
            tr("Could not reach the internet.\n\nWould you like to proceed in Offline Mode?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if (choice == QMessageBox::Yes) {
            loginOffline();
        }
        // If the user chose No, leave the checkbox unchecked and let them
        // wait for the server or close the dialog manually.
    }
    replyGoogle->deleteLater();
}






/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::on_checkBox_workOffline_clicked(bool checked)
{
    if (checked) {
        // Hide the web-based login form and expose the "Start Offline" button.
        ui->webView_login->setVisible(false);
        ui->pushButton_startOffline->setVisible(true);
        ui->label_process->setText(tr("Offline mode selected. Click \"Start Offline\" to continue."));
    } else {
        // Restore normal online login UI.
        ui->webView_login->setVisible(true);
        ui->pushButton_startOffline->setVisible(false);
        ui->label_process->setText(tr("Loading page..."));
    }
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::on_pushButton_startOffline_clicked()
{
    loginOffline();
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::loginOffline()
{
    // Populate the global Account object with safe defaults for a local user.
    account->isOffline        = true;
    // ID 0 is used as the offline-user sentinel: 0 is not a valid database
    // primary key (valid IDs start at 1) and differs from the uninitialized
    // default of -1, so callers can distinguish offline from unauthenticated.
    account->id               = 0;
    account->email            = QStringLiteral("local@offline");
    // email_clean is used by XmlUtil::parseLocalSaveFile() and saveLocalSaveFile()
    // to derive the local .save filename.  Give it a fixed, filesystem-safe value
    // so offline sessions consistently load and write to the same file.
    account->email_clean      = QStringLiteral("offline_user");
    account->display_name     = tr("Local User");
    account->first_name       = tr("Local");
    account->last_name        = tr("User");
    // subscription_type_id 1 = Free tier (lowest privilege); ensures that any
    // feature gated on subscription level behaves conservatively offline.
    account->subscription_type_id = 1;

    // Load any previously saved local preferences (folder paths, etc.).
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
    // dialog has been painted on screen.  On some platforms (notably Windows)
    // the QWebEngineView renderer process is not ready for navigation until
    // at least one event-loop iteration has elapsed after show().  Without
    // this deferral the OAuth server can respond so quickly that
    // accept()/reject() is called before the window is ever rendered,
    // making it appear to "open and immediately close" with nothing shown.
    QTimer::singleShot(0, oauthDialog, [oauthDialog, oauthUrl]() {
        oauthDialog->setUrlWebView(oauthUrl);
    });

    oauthDialog->exec();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Called when the Intervals.icu OAuth dialog reports a result.
void DialogLogin::onIntervalsIcuOAuthLinked(bool linked)
{
    if (!linked) {
        LOG_WARN("DialogLogin", QStringLiteral("Intervals.icu OAuth2 authorization denied or failed"));
        ui->label_process->setText(tr("Intervals.icu authorization failed. Please try again."));
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
    ui->webView_login->setVisible(false);

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

    // Safety-net timeout.  Stop and replace any existing timer to avoid
    // stale timers firing if the user retries OAuth login in the same session.
    if (m_intervalsIcuTimeout) {
        m_intervalsIcuTimeout->stop();
        m_intervalsIcuTimeout->deleteLater();
    }
    m_intervalsIcuTimeout = new QTimer(this);
    m_intervalsIcuTimeout->setSingleShot(true);
    connect(m_intervalsIcuTimeout, &QTimer::timeout,
            this, [this]() {
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

    // The user has authenticated via Intervals.icu OAuth2.
    // Set up the account in "Intervals.icu mode":
    //   – No MaximumTrainer.com account (id = 0)
    //   – Online, authenticated via Intervals.icu
    account->isOffline = false;
    account->id        = 0;

    // Build a stable, filesystem-safe identifier from the athlete ID so that
    // XmlUtil can persist and load the local .save file correctly.
    if (!account->intervals_icu_athlete_id.isEmpty()) {
        // Keep only alphanumeric characters (removes punctuation, retains the "i" prefix).
        // e.g. "i12345" → "i12345", "i123-abc" → "i123abc"
        QString safeId = account->intervals_icu_athlete_id;
        safeId.remove(QRegularExpression(QStringLiteral("[^a-zA-Z0-9]")));
        account->email_clean = QStringLiteral("icu_") + safeId;
        account->email       = account->intervals_icu_athlete_id + QStringLiteral("@intervals.icu");
    } else {
        account->email_clean = QStringLiteral("icu_user");
        account->email       = QStringLiteral("user@intervals.icu");
    }

    // subscription_type_id 2 = Regular tier (same numeric value used in the
    // MaximumTrainer.com database schema for non-free, non-studio accounts).
    account->subscription_type_id = 2;

    // Build display name from data fetched by fetchIntervalsIcuDataOAuth().
    if (account->display_name.isEmpty()) {
        if (!account->first_name.isEmpty())
            account->display_name = account->first_name
                                    + (account->last_name.isEmpty()
                                       ? QString()
                                       : QStringLiteral(" ") + account->last_name);
        else
            account->display_name = tr("Intervals.icu User");
    }

    // Load any previously saved local preferences for this user.
    XmlUtil::parseLocalSaveFile(account);

    // Persist the OAuth tokens so the session survives an app restart.
    account->saveIntervalsIcuCredentials();

    LOG_INFO("DialogLogin",
             QStringLiteral("Intervals.icu OAuth login complete for athlete: ")
             + account->intervals_icu_athlete_id);
    completeLogin();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::on_checkBox_autoLogin_clicked(bool checked)
{

    qDebug() << "on_checkBox_autoLogin_clicked";

    settings->rememberMyPassword = checked;
    settings->saveGeneralSettings();
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::on_comboBox_language_currentIndexChanged(int index) {

    //    qDebug() << "LANGUAGE CHANGED!" <<index;
    Q_UNUSED(index);

    firstLogin = true;
    QString languageToPut = ui->comboBox_language->currentText();


    qApp->removeTranslator(&m_translator);

    bool success;
    if (languageToPut == "English") {
        qDebug() << "got here English";
        success = m_translator.load(":/language/language/powervelo_en.qm");
        settings->language_index = 0;
        settings->language = "en";
        settings->saveLanguage();

        ui->webView_login->setUrl(QUrl(Environnement::getUrlLogin()));
    }
    else if (languageToPut == "Français") {
        qDebug() << "got here French";
        success = m_translator.load(":/language/language/powervelo_fr.qm");
        settings->language_index = 1;
        settings->language = "fr";
        settings->saveLanguage();

        ui->webView_login->setUrl(QUrl(Environnement::getUrlLogin()));
    }
    else {
        success = m_translator.load(":/language/language/powervelo_en.qm");
        settings->language_index = 0;
        settings->language = "en";

        ui->webView_login->setUrl(QUrl(Environnement::getUrlLogin()));
    }

    qDebug() << "SUCCES?" << success;

    if(success) {
        qApp->installTranslator(&m_translator);
        ui->retranslateUi(this);
        m_currLang = languageToPut;
    }


}



// ─────────────────────────────────────────────────────────────────────────────
// Intervals.icu data retrieval
// ─────────────────────────────────────────────────────────────────────────────

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::fetchIntervalsIcuData()
{
    if (account->intervals_icu_athlete_id.isEmpty() || account->intervals_icu_api_key.isEmpty()) {
        // No credentials stored — skip Intervals.icu and proceed directly.
        LOG_INFO("DialogLogin", QStringLiteral("No Intervals.icu credentials configured – skipping retrieval"));
        completeLogin();
        return;
    }

    LOG_INFO("DialogLogin", QStringLiteral("Intervals.icu is retrieving all relevant user details"));
    ui->label_process->setText(tr("Intervals.icu is retrieving all relevant user details..."));

    m_pendingIntervalsIcuReplies = 2;  // athlete + settings

    // Fetch basic profile.
    replyIntervalsIcuAthlete = IntervalsIcuDAO::getAthlete(
            account->intervals_icu_athlete_id,
            account->intervals_icu_api_key);
    if (replyIntervalsIcuAthlete) {
        connect(replyIntervalsIcuAthlete, &QNetworkReply::finished,
                this, &DialogLogin::slotFinishedIntervalsIcuAthlete);
    } else {
        LOG_WARN("DialogLogin", QStringLiteral("Intervals.icu getAthlete returned null reply"));
        m_pendingIntervalsIcuReplies--;
    }

    // Fetch training-zone settings.
    replyIntervalsIcuSettings = IntervalsIcuDAO::getAthleteSettings(
            account->intervals_icu_athlete_id,
            account->intervals_icu_api_key);
    if (replyIntervalsIcuSettings) {
        connect(replyIntervalsIcuSettings, &QNetworkReply::finished,
                this, &DialogLogin::slotFinishedIntervalsIcuSettings);
    } else {
        LOG_WARN("DialogLogin", QStringLiteral("Intervals.icu getAthleteSettings returned null reply"));
        m_pendingIntervalsIcuReplies--;
    }

    // If both requests failed immediately (no manager), proceed without waiting.
    if (m_pendingIntervalsIcuReplies <= 0) {
        completeLogin();
        return;
    }

    // Safety-net: if either reply stalls, proceed after 15 s anyway.
    if (m_intervalsIcuTimeout) {
        m_intervalsIcuTimeout->stop();
        m_intervalsIcuTimeout->deleteLater();
    }
    m_intervalsIcuTimeout = new QTimer(this);
    m_intervalsIcuTimeout->setSingleShot(true);
    connect(m_intervalsIcuTimeout, &QTimer::timeout,
            this, &DialogLogin::onIntervalsIcuTimeout);
    m_intervalsIcuTimeout->start(15000);
}


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
    if (m_pendingIntervalsIcuReplies <= 0) {
        if (m_intervalsIcuTimeout) m_intervalsIcuTimeout->stop();
        if (m_loggingInViaIntervalsIcu)
            loginWithIntervalsIcuIdentity();
        else
            completeLogin();
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
    if (m_pendingIntervalsIcuReplies <= 0) {
        if (m_intervalsIcuTimeout) m_intervalsIcuTimeout->stop();
        if (m_loggingInViaIntervalsIcu)
            loginWithIntervalsIcuIdentity();
        else
            completeLogin();
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::onIntervalsIcuTimeout()
{
    LOG_WARN("DialogLogin",
             QStringLiteral("Intervals.icu retrieval timed out after 15 s – proceeding with login"));
    ui->label_process->setText(tr("Intervals.icu retrieval timed out, continuing..."));

    // Set the counter to 0 and null out the pointers BEFORE calling abort(),
    // because abort() may emit finished() synchronously.  The slots check the
    // pointer for nullptr at the very top, so they will return early even if
    // the finished() signal fires inside abort().
    m_pendingIntervalsIcuReplies = 0;

    if (replyIntervalsIcuAthlete) {
        auto *reply = replyIntervalsIcuAthlete;
        replyIntervalsIcuAthlete = nullptr;
        reply->abort();
        reply->deleteLater();
    }
    if (replyIntervalsIcuSettings) {
        auto *reply = replyIntervalsIcuSettings;
        replyIntervalsIcuSettings = nullptr;
        reply->abort();
        reply->deleteLater();
    }

    if (m_loggingInViaIntervalsIcu)
        loginWithIntervalsIcuIdentity();
    else
        completeLogin();
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void DialogLogin::completeLogin()
{
    // Guard against being called more than once (e.g. if both replies finish
    // nearly simultaneously after the counter reaches zero).
    if (!this->isVisible()) return;

    LOG_INFO("DialogLogin", QStringLiteral("Login complete – entering application"));
    this->accept();
}

