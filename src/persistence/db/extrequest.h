#ifndef EXTREQUEST_H
#define EXTREQUEST_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QApplication>
#include <QtCore>
#include "environnement.h"




class ExtRequest
{
public:


    static QNetworkReply* checkGoogleConnection();
    static QNetworkReply* checkIpAddress();

    //-- Strava (token deauthorize only; upload/status now live in StravaService)
    static QNetworkReply* stravaDeauthorization(QString access_token);


    //-- Intervals.icu OAuth2
    /// Exchange an authorization code for an access token by POSTing to the
    /// Cloudflare Worker proxy (URL_TOKEN_ICV).  The Worker looks up the
    /// client_secret in its CLIENT_SECRETS KV namespace keyed by client_id,
    /// so no app binary carries the secret.
    ///
    /// The request body is JSON: { "code": ..., "redirect_uri": ...,
    /// "client_id": ... }.  On error the Worker returns a JSON payload with
    /// standard CORS headers, e.g. { "error": "unauthorized_client" }; the
    /// caller can parse it via Util::parseJsonIntervalsIcuOAuthErrorPayload().
    ///
    /// @param code        The authorization code received from the redirect URI.
    /// @param redirectUri The exact redirect_uri used in the authorization request.
    /// @param clientId    The OAuth2 client_id (mandatory — used by the Worker
    ///                    to look up the matching client_secret in KV).
    static QNetworkReply* intervalsIcuOAuthExchange(const QString &code,
                                                    const QString &redirectUri,
                                                    const QString &clientId);


};

#endif // EXTREQUEST_H
