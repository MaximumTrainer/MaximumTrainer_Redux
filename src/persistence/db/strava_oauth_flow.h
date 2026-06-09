#ifndef STRAVA_OAUTH_FLOW_H
#define STRAVA_OAUTH_FLOW_H

#include <QObject>
#include <QString>

class QTcpServer;

/// Runs the Strava OAuth login in the user's SYSTEM browser. Strava blocks
/// embedded webviews (reCAPTCHA / bot detection → "An unexpected error
/// occurred"), and Google/Apple SSO are disallowed in embedded user-agents, so
/// the only reliable flow is the external browser (RFC 8252).
///
/// A short-lived localhost loopback listener catches Strava's redirect, grabs
/// the authorization code, and exchanges it for tokens via the Strava token
/// Worker (StravaService::exchangeAuthCode). On success the tokens are stored
/// on Account and finished(true) is emitted.
///
/// Requires the Strava app's "Authorization Callback Domain" to be `localhost`.
class StravaOAuthFlow : public QObject
{
    Q_OBJECT
public:
    explicit StravaOAuthFlow(QObject *parent = nullptr);
    ~StravaOAuthFlow() override;

    /// Start the loopback listener and open the system browser. Returns false
    /// if the local listener could not be started; otherwise finished() is
    /// emitted exactly once when the flow completes (or fails).
    bool start();

signals:
    void finished(bool linked);

private:
    void onNewConnection();
    void exchangeCode(const QString &code);
    void finish(bool linked);

    QTcpServer *m_server = nullptr;
    QString m_redirectUri;
    bool m_done = false;
};

#endif // STRAVA_OAUTH_FLOW_H
