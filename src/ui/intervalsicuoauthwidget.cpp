#include "intervalsicuoauthwidget.h"

#include <QVBoxLayout>
#include <QWebEngineProfile>
#include <QUrlQuery>

#include "util.h"
#include "account.h"
#include "extrequest.h"
#include "environnement.h"
#include "logger.h"
#include "myqwebenginepage.h"

IntervalsIcuOAuthWidget::IntervalsIcuOAuthWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QWebEngineProfile *profile = new QWebEngineProfile(this);
    profile->setPersistentCookiesPolicy(QWebEngineProfile::NoPersistentCookies);

    auto *page = new MyQWebEnginePage(profile, this);
    page->setExternalList({"maximumtrainer"});

    m_webView = new QWebEngineView(this);
    m_webView->setPage(page);
    layout->addWidget(m_webView);

    connect(m_webView, &QWebEngineView::loadFinished,
            this, &IntervalsIcuOAuthWidget::onPageLoaded);
}

IntervalsIcuOAuthWidget::~IntervalsIcuOAuthWidget()
{
    if (m_tokenReply) {
        auto *r = m_tokenReply;
        m_tokenReply = nullptr;
        r->abort(); r->deleteLater();
    }
}

void IntervalsIcuOAuthWidget::startAuth(const QString &authUrl, const QString &csrfState)
{
    m_csrfState    = csrfState;
    m_showingError = false;
    m_settled      = false;
    LOG_INFO("IntervalsIcuOAuthWidget", QStringLiteral("startAuth: ") + authUrl);
    m_webView->setUrl(QUrl(authUrl));
}

void IntervalsIcuOAuthWidget::reset()
{
    if (m_tokenReply) {
        auto *r = m_tokenReply;
        m_tokenReply = nullptr;
        r->abort(); r->deleteLater();
    }
    m_csrfState.clear();
    m_showingError = false;
    m_settled      = false;
    m_webView->setUrl(QUrl(QStringLiteral("about:blank")));
}

void IntervalsIcuOAuthWidget::cancel()
{
    reset();
    emit cancelRequested();
}

// ─────────────────────────────────────────────────────────────────────────────

void IntervalsIcuOAuthWidget::onPageLoaded(bool /*ok*/)
{
    if (m_settled || m_showingError) return;

    const QString urlStr = m_webView->url().toDisplayString();

    if (!urlStr.contains(QLatin1String("/intervals_icu_token_exchange")))
        return;

    LOG_INFO("IntervalsIcuOAuthWidget", QStringLiteral("OAuth2 callback received"));

    // ── Error in redirect URL ──────────────────────────────────────────────
    if (urlStr.contains(QLatin1String("error="))) {
        const QUrlQuery q{QUrl(urlStr)};
        const QString error     = q.queryItemValue(QStringLiteral("error"));
        const QString errorDesc = q.queryItemValue(QStringLiteral("error_description"));
        LOG_WARN("IntervalsIcuOAuthWidget",
                 QStringLiteral("OAuth error in redirect — ") + error
                 + (errorDesc.isEmpty() ? QString()
                                        : QStringLiteral(": ") + errorDesc));

        if (error == QLatin1String("access_denied")) {
            // User explicitly cancelled — treat as cancel, not failure.
            m_settled = true;
            emit cancelRequested();
        } else {
            m_showingError = true;
            m_webView->setHtml(buildErrorPageHtml(urlStr));
            m_settled = true;
            emit authFailed();
        }
        return;
    }

    // ── Path 1: server-side exchange — page body is JSON ──────────────────
    if (!urlStr.contains(QLatin1String("code="))) {
        LOG_INFO("IntervalsIcuOAuthWidget",
                 QStringLiteral("Server-side token exchange path"));
        m_webView->page()->toPlainText([this](const QString &body) {
            const QString trimmed = body.trimmed();
            if (!trimmed.startsWith('{')) {
                LOG_WARN("IntervalsIcuOAuthWidget",
                         QStringLiteral("Non-JSON response in server-side exchange"));
                m_settled = true;
                emit authFailed();
                return;
            }
            Util::parseJsonIntervalsIcuOAuthToken(body);
            Account *account = qApp->property("Account").value<Account*>();
            if (account && !account->intervals_icu_access_token.isEmpty()) {
                LOG_INFO("IntervalsIcuOAuthWidget",
                         QStringLiteral("Server-side token exchange succeeded"));
                m_settled = true;
                emit authSucceeded();
            } else {
                LOG_WARN("IntervalsIcuOAuthWidget",
                         QStringLiteral("Token missing from server-side exchange response"));
                m_settled = true;
                emit authFailed();
            }
        });
        return;
    }

    // ── Path 2: client-side exchange — URL has ?code= ─────────────────────
    const QUrl redirectUrl(urlStr);
    const QUrlQuery redirectQ(redirectUrl);
    const QString code = redirectQ.queryItemValue(QStringLiteral("code"));
    if (code.isEmpty()) {
        LOG_WARN("IntervalsIcuOAuthWidget",
                 QStringLiteral("No authorization code in redirect URL"));
        m_settled = true;
        emit authFailed();
        return;
    }

    if (!m_csrfState.isEmpty()) {
        const QString returnedState = redirectQ.queryItemValue(QStringLiteral("state"));
        if (returnedState != m_csrfState) {
            LOG_WARN("IntervalsIcuOAuthWidget",
                     QStringLiteral("State mismatch — possible CSRF attack"));
            m_settled = true;
            emit authFailed();
            return;
        }
    }

    LOG_INFO("IntervalsIcuOAuthWidget",
             QStringLiteral("Client-side token exchange"));
    const QString redirectUri =
        Environnement::getURLEnvironnement() + "intervals_icu_token_exchange";
    m_tokenReply = ExtRequest::intervalsIcuOAuthExchange(code, redirectUri);
    if (!m_tokenReply) {
        LOG_WARN("IntervalsIcuOAuthWidget",
                 QStringLiteral("Client-side token exchange request failed to start"));
        m_settled = true;
        emit authFailed();
        return;
    }
    connect(m_tokenReply, &QNetworkReply::finished,
            this, &IntervalsIcuOAuthWidget::onTokenExchangeFinished);
}

void IntervalsIcuOAuthWidget::onTokenExchangeFinished()
{
    if (!m_tokenReply) return;

    if (m_tokenReply->error() == QNetworkReply::NoError) {
        const QByteArray data = m_tokenReply->readAll();
        Util::parseJsonIntervalsIcuOAuthToken(QString::fromUtf8(data));
        Account *account = qApp->property("Account").value<Account*>();
        if (account && !account->intervals_icu_access_token.isEmpty()) {
            m_tokenReply->deleteLater();
            m_tokenReply = nullptr;
            LOG_INFO("IntervalsIcuOAuthWidget",
                     QStringLiteral("Client-side token exchange succeeded"));
            m_settled = true;
            emit authSucceeded();
            return;
        }
    } else {
        LOG_WARN("IntervalsIcuOAuthWidget",
                 QStringLiteral("Client-side token exchange failed: ")
                 + m_tokenReply->errorString());
    }

    m_tokenReply->deleteLater();
    m_tokenReply = nullptr;
    m_settled = true;
    emit authFailed();
}

// ─────────────────────────────────────────────────────────────────────────────

QString IntervalsIcuOAuthWidget::buildErrorPageHtml(const QString &failedUrl) const
{
    return QStringLiteral(
        "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<style>"
        "body{font-family:sans-serif;max-width:600px;margin:60px auto;color:#333;}"
        "h2{color:#c0392b;}"
        "code{background:#f5f5f5;padding:2px 4px;border-radius:3px;font-size:0.85em;}"
        "</style>"
        "</head><body>"
        "<h2>&#9888; Intervals.icu OAuth2 Error</h2>"
        "<p>The authorization request returned an error.</p>"
        "<p><code>%1</code></p>"
        "<p>Please close this window and try again, or check the "
        "<a href='https://intervals.icu'>Intervals.icu</a> status page.</p>"
        "</body></html>").arg(failedUrl.toHtmlEscaped());
}
