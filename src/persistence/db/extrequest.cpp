#include "extrequest.h"
#include "util.h"
#include "logger.h"

#include <QHttpMultiPart>




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QNetworkReply* ExtRequest::checkGoogleConnection() {

    QNetworkAccessManager *managerWS = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!managerWS) {
        LOG_WARN("ExtRequest", QStringLiteral("checkGoogleConnection: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString urlGoogle = "http://www.google.com/";
    QNetworkRequest request2;
    request2.setUrl(QUrl(urlGoogle));
    request2.setRawHeader("User-Agent", "MyOwnBrowser 1.0");
    QNetworkReply *replyGoogle = managerWS->get(request2);

    return replyGoogle;


}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QNetworkReply* ExtRequest::checkIpAddress() {

    QNetworkAccessManager *managerWS = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!managerWS) {
        LOG_WARN("ExtRequest", QStringLiteral("checkIpAddress: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url2 = "http://bot.whatismyipaddress.com/";
    QNetworkRequest request1;
    request1.setUrl(QUrl(url2));
    request1.setRawHeader("User-Agent", "MyOwnBrowser 1.0");
    QNetworkReply *replyMyIp = managerWS->get(request1);
    return replyMyIp;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QNetworkReply* ExtRequest::stravaDeauthorization(QString access_token) {

    QNetworkAccessManager *managerWS = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!managerWS) {
        LOG_WARN("ExtRequest", QStringLiteral("stravaDeauthorization: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url =  "https://www.strava.com/oauth/deauthorize";
    QUrlQuery postData;
    postData.addQueryItem("access_token", access_token);

    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");

    QNetworkReply *replyPutUser = managerWS->post(request, postData.toString(QUrl::FullyEncoded).toUtf8() );

    return replyPutUser;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// POST <token endpoint>  (grant_type=authorization_code)
/// Exchanges an authorization code for an OAuth2 access + refresh token pair.
///
/// intervals.icu REQUIRES the client_secret here (HTTP 422 without it).
/// When one is configured locally (qmake INTERVALS_OAUTH_CLIENT_SECRET or
/// CredentialStore) the desktop posts to intervals.icu directly with it;
/// otherwise the request goes through the Cloudflare proxy, which injects
/// the secret server-side (see Environnement::getIntervalsIcuTokenUrl()).
QNetworkReply* ExtRequest::intervalsIcuOAuthExchange(const QString &code, const QString &redirectUri)
{
    QNetworkAccessManager *managerWS = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!managerWS) {
        LOG_WARN("ExtRequest", QStringLiteral("intervalsIcuOAuthExchange: NetworkManagerWS not available"));
        return nullptr;
    }

    QUrlQuery postData;
    postData.addQueryItem("grant_type",    "authorization_code");
    postData.addQueryItem("client_id",     Environnement::getIntervalsIcuClientId());
    postData.addQueryItem("code",          code);
    postData.addQueryItem("redirect_uri",  redirectUri);
    const QString secret = Environnement::getIntervalsIcuClientSecret();
    if (!secret.isEmpty())
        postData.addQueryItem("client_secret", secret);

    const QString tokenUrl = Environnement::getIntervalsIcuTokenUrl();
    LOG_INFO("ExtRequest",
             QStringLiteral("intervalsIcuOAuthExchange via ") + tokenUrl);

    QNetworkRequest request;
    request.setUrl(QUrl(tokenUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
#ifndef Q_OS_WASM
    // When routed through the proxy, identify ourselves to its allow-list.
    // We deliberately do NOT set Origin: the desktop build is not a browser
    // and is not subject to CORS.  (On WASM the browser sets Origin itself.)
    if (tokenUrl == URL_TOKEN_ICV_PROXY)
        request.setRawHeader(INTERVALS_PROXY_CLIENT_HEADER.toUtf8(),
                             INTERVALS_PROXY_DESKTOP_CLIENT_VALUE.toUtf8());
#endif

    return managerWS->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// POST <token endpoint>  (grant_type=refresh_token)
/// Exchanges a stored refresh token for a new access + refresh token pair.
/// Same endpoint and client_secret requirement as intervalsIcuOAuthExchange.
/// The caller must connect finished() and parse the response with
/// Util::parseJsonIntervalsIcuOAuthToken(), then call
/// account->saveIntervalsIcuCredentials().
QNetworkReply* ExtRequest::intervalsIcuOAuthRefresh(const QString &refreshToken)
{
    QNetworkAccessManager *managerWS = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!managerWS) {
        LOG_WARN("ExtRequest", QStringLiteral("intervalsIcuOAuthRefresh: NetworkManagerWS not available"));
        return nullptr;
    }

    QUrlQuery postData;
    postData.addQueryItem("grant_type",    "refresh_token");
    postData.addQueryItem("client_id",     Environnement::getIntervalsIcuClientId());
    postData.addQueryItem("refresh_token", refreshToken);
    const QString secret = Environnement::getIntervalsIcuClientSecret();
    if (!secret.isEmpty())
        postData.addQueryItem("client_secret", secret);

    const QString tokenUrl = Environnement::getIntervalsIcuTokenUrl();

    QNetworkRequest request;
    request.setUrl(QUrl(tokenUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
#ifndef Q_OS_WASM
    if (tokenUrl == URL_TOKEN_ICV_PROXY)
        request.setRawHeader(INTERVALS_PROXY_CLIENT_HEADER.toUtf8(),
                             INTERVALS_PROXY_DESKTOP_CLIENT_VALUE.toUtf8());
#endif

    LOG_INFO("ExtRequest", QStringLiteral("intervalsIcuOAuthRefresh: refreshing access token"));
    return managerWS->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}
