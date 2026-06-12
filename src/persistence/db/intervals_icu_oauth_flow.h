#ifndef INTERVALS_ICU_OAUTH_FLOW_H
#define INTERVALS_ICU_OAUTH_FLOW_H

#include <QObject>
#include <QString>

class QTcpServer;
class QNetworkReply;

/// Runs the Intervals.icu OAuth login in the user's SYSTEM browser.
/// The previous desktop flow embedded the login page in a QWebEngineView, but
/// Google/Apple SSO (which intervals.icu accounts can be backed by) are
/// disallowed in embedded user-agents, so — like StravaOAuthFlow — the only
/// reliable flow is the external browser (RFC 8252).
///
/// A short-lived localhost loopback listener catches the redirect, validates
/// the CSRF state, grabs the authorization code, and exchanges it for tokens
/// via the Cloudflare Worker (ExtRequest::intervalsIcuOAuthExchange; the
/// Worker injects the client_secret server-side). On success the tokens are
/// stored on Account and succeeded() is emitted.
///
/// Intervals.icu always accepts http://localhost/ as a redirect URI, so no
/// per-app redirect registration is needed.
class IntervalsIcuOAuthFlow : public QObject
{
    Q_OBJECT
public:
    explicit IntervalsIcuOAuthFlow(QObject *parent = nullptr);
    ~IntervalsIcuOAuthFlow() override;

    /// When false, start() prepares the loopback listener and authorization
    /// URL but does not launch the system browser (integration tests).
    void setOpenExternalBrowser(bool open);

    /// Start the loopback listener and open the system browser. Returns false
    /// if the local listener could not be started; otherwise exactly one of
    /// succeeded() / failed() / cancelled() is emitted when the flow settles.
    bool start();

    /// Stop listening and abort any in-flight token exchange. Emits nothing —
    /// for the UI "Cancel" button and teardown paths.
    void abort();

    /// Full authorization URL built by start() — for "open the page again".
    QString authorizationUrl() const { return m_authUrl; }

    /// Loopback port chosen by start() (tests simulate the redirect with it).
    quint16 listenPort() const;

signals:
    void succeeded();
    void failed();
    void cancelled();   ///< the user declined authorization (access_denied)

private:
    void onNewConnection();
    void exchangeCode(const QString &code);
    void settle(void (IntervalsIcuOAuthFlow::*signal)());

    QTcpServer    *m_server     = nullptr;
    QNetworkReply *m_tokenReply = nullptr;
    QString m_redirectUri;
    QString m_authUrl;
    QString m_csrfState;
    bool m_openExternalBrowser = true;
    bool m_done = false;
};

#endif // INTERVALS_ICU_OAUTH_FLOW_H
