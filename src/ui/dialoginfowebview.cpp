#include "dialoginfowebview.h"
#include "ui_dialoginfowebview.h"

#include <QDesktopServices>
#include <QUrlQuery>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include "util.h"
#include "account.h"
#include "extrequest.h"
#include "environnement.h"
#include "logger.h"

#include "myqwebenginepage.h"

DialogInfoWebView::~DialogInfoWebView()
{
    // Abort any in-flight client-side token exchange so it doesn't update
    // the global Account after the dialog has been destroyed.
    if (m_intervalsTokenReply) {
        m_intervalsTokenReply->abort();
        m_intervalsTokenReply->deleteLater();
        m_intervalsTokenReply = nullptr;
    }
    delete ui;
}



DialogInfoWebView::DialogInfoWebView(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DialogInfoWebView)
{
    ui->setupUi(this);


    // External links
    QStringList lstExternal = {"forum", "maximumtrainer", };

    QWebEngineProfile* newProfile = new QWebEngineProfile(this);
    newProfile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    MyQWebEnginePage *myPage = new MyQWebEnginePage(newProfile, this);

    myPage->setExternalList(lstExternal);
    ui->webView->setPage(myPage);

    usedForStrava = false;
    usedForIntervalsIcu = false;

    connect(ui->webView, SIGNAL(loadStarted()), this, SLOT(onLoadStarted()));
    connect(ui->webView, SIGNAL(loadFinished(bool)), this, SLOT(pageLoaded(bool)));

}


/////////////////////////////////////////////////////////////////////////
void DialogInfoWebView::setTitle(QString title) {

    this->setWindowTitle(title);
}
/////////////////////////////////////////////////////////////////////////
void DialogInfoWebView::setUsedForStrava(bool used) {

    this->usedForStrava = used;
}
/////////////////////////////////////////////////////////////////////////
void DialogInfoWebView::setUsedForIntervalsIcu(bool used) {

    this->usedForIntervalsIcu = used;
}

/////////////////////////////////////////////////////////////////////////
void DialogInfoWebView::setExpectedOAuthState(const QString &state) {

    m_expectedOAuthState = state;
}



/////////////////////////////////////////////////////////////////////////
/// Builds a user-friendly HTML error page for OAuth flow failures.
/// Shows the failed URL (for copy/paste), a link to open it in the
/// browser, and the log file path so the user knows where to look.
QString DialogInfoWebView::buildErrorPageHtml(const QString &failedUrl) const
{
    const QString safePath = Logger::instance().logFilePath().toHtmlEscaped();
    const QString safeUrl  = failedUrl.toHtmlEscaped();
    const QString logHint  = safePath.isEmpty()
        ? QString()
        : QStringLiteral("<p class='hint'>Log file: <code>%1</code></p>").arg(safePath);

    return QStringLiteral(
        "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'/>"
        "<style>"
        "  body{font-family:sans-serif;padding:24px;color:#333;}"
        "  h3{color:#c0392b;}"
        "  a{color:#2980b9;}"
        "  pre{background:#f4f4f4;padding:8px;border-radius:4px;word-break:break-all;}"
        "  .hint{color:#777;font-size:12px;margin-top:16px;}"
        "</style>"
        "</head><body>"
        "<h3>&#9888; Unable to load page</h3>"
        "<p>The authorization page could not be loaded. "
        "Please check your internet connection and try again.</p>"
        "<p><strong>Failed URL:</strong></p>"
        "<pre>%1</pre>"
        "<p>If the problem persists, you can "
        "<a href='%2'>open the page in your browser</a> "
        "and paste the result back manually.</p>"
        "%3"
        "</body></html>")
        .arg(safeUrl, safeUrl, logHint);
}

/////////////////////////////////////////////////////////////////////////
void DialogInfoWebView::onLoadStarted()
{
    // Skip logging for the inline error page we inject ourselves.
    if (m_showingErrorPage) return;
    LOG_INFO("DialogInfoWebView",
             QStringLiteral("Loading page: ") + ui->webView->url().toDisplayString());
}

/////////////////////////////////////////////////////////////////////////
void DialogInfoWebView::pageLoaded(bool ok){

    const QString currentUrl = ui->webView->url().toDisplayString();
    LOG_DEBUG("DialogInfoWebView",
              QStringLiteral("pageLoaded ok=") + (ok ? QStringLiteral("true") : QStringLiteral("false"))
              + QStringLiteral(" url=") + currentUrl);

    // If we injected an error HTML page ourselves, suppress further handling.
    if (m_showingErrorPage) {
        m_showingErrorPage = false;
        return;
    }

    if (!ok) {
        LOG_WARN("DialogInfoWebView",
                 QStringLiteral("Page failed to load: ") + currentUrl);

        if (usedForIntervalsIcu || usedForStrava) {
            m_showingErrorPage = true;
            ui->webView->setHtml(buildErrorPageHtml(currentUrl));
        }
        return;
    }

    if (!usedForStrava && !usedForIntervalsIcu)
        return;

    LOG_DEBUG("DialogInfoWebView",
              QStringLiteral("Checking OAuth callback for url: ") + currentUrl);
    /// ------------------------------- Login sucess! ---------------------------------------
    if (ui->webView->url().toDisplayString().contains("/strava_token_exchange"))  {

        // Only try to parse the json object when request was successful
        if (!ui->webView->url().toDisplayString().contains("&error") && ui->webView->url().toDisplayString().contains("&code")) {
            LOG_INFO("DialogInfoWebView", QStringLiteral("Strava token exchange callback received"));

            ui->webView->page()->toPlainText([=](const QString &response){
                LOG_DEBUG("DialogInfoWebView",
                          QStringLiteral("Strava token exchange response length: ")
                          + QString::number(response.size()));
                Util::parseJsonStravaObject(response);

                Account *account = qApp->property("Account").value<Account*>();
                if (account->strava_access_token.size() > 2) {
                    LOG_INFO("DialogInfoWebView", QStringLiteral("Strava OAuth linked successfully"));
                    emit stravaLinked(true);
                }
            });




        }
    }
    /// ------ Intervals.icu OAuth2 callback ------------------------------------------------
    else if (usedForIntervalsIcu
             && ui->webView->url().toDisplayString().contains("/oauth_callback.html"))  {

        const QString urlStr = ui->webView->url().toDisplayString();
        LOG_INFO("DialogInfoWebView", QStringLiteral("Intervals.icu OAuth2 callback received"));

        // If the authorization server returned an error in the redirect URL,
        // determine whether the user explicitly cancelled (access_denied) or
        // whether a technical OAuth error occurred (invalid_client,
        // invalid_redirect_uri, server_error, etc.).
        if (urlStr.contains(QLatin1String("error="))) {
            const QUrlQuery errorQuery{QUrl(urlStr)};
            const QString error     = errorQuery.queryItemValue(QStringLiteral("error"));
            const QString errorDesc = errorQuery.queryItemValue(QStringLiteral("error_description"));
            LOG_WARN("DialogInfoWebView",
                     QStringLiteral("Intervals.icu OAuth: error in redirect — ")
                     + error
                     + (errorDesc.isEmpty() ? QString()
                                            : QStringLiteral(": ") + errorDesc));

            if (error == QLatin1String("access_denied")) {
                // User explicitly cancelled the authorization — close silently.
                // Do not treat this as a login failure; the rejected() signal
                // on the dialog is sufficient for the caller to clean up.
                this->reject();
            } else {
                // Technical OAuth error (e.g. invalid_client, invalid_scope,
                // server_error).  Signal failure and show a descriptive error
                // page so the user understands what happened instead of having
                // the window silently close.
                emit intervalsIcuLinked(false);
                m_showingErrorPage = true;
                ui->webView->setHtml(buildErrorPageHtml(urlStr));
            }
            return;
        }

        // Case 1: The MaximumTrainer.com backend proxy exchanged the code and
        // returned a JSON body — read it directly as plain text.
        // We verify the response contains a JSON object with an "access_token"
        // field before accepting the result.
        if (!urlStr.contains("code=")) {
            LOG_INFO("DialogInfoWebView",
                     QStringLiteral("Intervals.icu OAuth: server-side token exchange path"));
            ui->webView->page()->toPlainText([=](const QString &response){
                // Validate that the response looks like a JSON object before parsing.
                const QString trimmed = response.trimmed();
                if (!trimmed.startsWith('{')) {
                    LOG_WARN("DialogInfoWebView",
                             QStringLiteral("Intervals.icu token exchange: unexpected non-JSON response"));
                    emit intervalsIcuLinked(false);
                    this->reject();
                    return;
                }
                Util::parseJsonIntervalsIcuOAuthToken(response);
                Account *account = qApp->property("Account").value<Account*>();
                if (account && !account->intervals_icu_access_token.isEmpty()) {
                    LOG_INFO("DialogInfoWebView",
                             QStringLiteral("Intervals.icu OAuth: server-side token exchange succeeded"));
                    emit intervalsIcuLinked(true);
                    this->accept();
                } else {
                    LOG_WARN("DialogInfoWebView",
                             QStringLiteral("Intervals.icu OAuth: token missing from server-side exchange response"));
                    emit intervalsIcuLinked(false);
                    this->reject();
                }
            });
            return;
        }

        // Case 2: The redirect URI still has ?code= in it (i.e. the backend
        // proxy forwarded it unchanged).  Extract the code and exchange it
        // client-side by POSTing directly to the Intervals.icu token endpoint.
        const QUrl redirectUrl(urlStr);
        const QUrlQuery redirectQuery(redirectUrl);
        const QString code = redirectQuery.queryItemValue("code");
        if (code.isEmpty()) {
            LOG_WARN("DialogInfoWebView",
                     QStringLiteral("Intervals.icu OAuth: no authorization code found in redirect URL"));
            emit intervalsIcuLinked(false);
            this->reject();
            return;
        }

        // Validate CSRF state if one was set at authorization time.
        if (!m_expectedOAuthState.isEmpty()) {
            const QString returnedState = redirectQuery.queryItemValue("state");
            if (returnedState != m_expectedOAuthState) {
                LOG_WARN("DialogInfoWebView",
                         QStringLiteral("Intervals.icu OAuth: state mismatch — possible CSRF attack"));
                emit intervalsIcuLinked(false);
                this->reject();
                return;
            }
        }

        LOG_INFO("DialogInfoWebView",
                 QStringLiteral("Intervals.icu OAuth: exchanging authorization code via Cloudflare proxy"));
        const QString redirectUri = Environnement::getWasmOAuthRedirectUri();
        m_intervalsTokenReply = ExtRequest::intervalsIcuOAuthExchange(code, redirectUri);
        if (!m_intervalsTokenReply) {
            LOG_WARN("DialogInfoWebView",
                     QStringLiteral("Intervals.icu OAuth: client-side token exchange request failed to start"));
            emit intervalsIcuLinked(false);
            this->reject();
            return;
        }
        connect(m_intervalsTokenReply, &QNetworkReply::finished,
                this, &DialogInfoWebView::slotIntervalsTokenExchangeFinished);
    }

    LOG_DEBUG("DialogInfoWebView", QStringLiteral("pageLoaded handler complete"));


}





/////////////////////////////////////////////////////////////////////////
void DialogInfoWebView::setUrlWebView(QString url) {

    m_showingErrorPage = false;
    LOG_INFO("DialogInfoWebView", QStringLiteral("setUrlWebView: ") + url);
    ui->webView->setUrl(QUrl(url));
}

/////////////////////////////////////////////////////////////////////////
/// Called when the direct client-side Intervals.icu token exchange POST completes.
void DialogInfoWebView::slotIntervalsTokenExchangeFinished()
{
    if (!m_intervalsTokenReply) return;

    if (m_intervalsTokenReply->error() == QNetworkReply::NoError) {
        const QByteArray data = m_intervalsTokenReply->readAll();
        Util::parseJsonIntervalsIcuOAuthToken(QString::fromUtf8(data));
        Account *account = qApp->property("Account").value<Account*>();
        if (account && !account->intervals_icu_access_token.isEmpty()) {
            m_intervalsTokenReply->deleteLater();
            m_intervalsTokenReply = nullptr;
            LOG_INFO("DialogInfoWebView",
                     QStringLiteral("Intervals.icu OAuth: client-side token exchange succeeded"));
            emit intervalsIcuLinked(true);
            this->accept();
            return;
        }
    } else {
        LOG_WARN("DialogInfoWebView",
                 QStringLiteral("Intervals.icu OAuth: client-side token exchange failed: ")
                 + m_intervalsTokenReply->errorString());
    }

    m_intervalsTokenReply->deleteLater();
    m_intervalsTokenReply = nullptr;
    emit intervalsIcuLinked(false);
    this->reject();
}




