#include "extrequest.h"
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
/// POST URL_TOKEN_ICV  (grant_type=authorization_code)
/// Exchanges an authorization code for an OAuth2 access + refresh token pair.
/// All platforms go through the Cloudflare Worker, which looks up the
/// client_secret in its CLIENT_SECRETS KV namespace keyed by the supplied
/// client_id.  The Worker returns standard OAuth2 JSON on success and a
/// JSON error payload with CORS headers on failure (see
/// workers/intervals-cors-proxy/worker.js — e.g. {"error":"unauthorized_client"}).
///
/// Body is JSON: { "code": ..., "redirect_uri": ..., "client_id": ... }.
/// The Worker also accepts application/x-www-form-urlencoded for legacy
/// callers, but JSON is the canonical multi-tenant shape.
QNetworkReply* ExtRequest::intervalsIcuOAuthExchange(const QString &code,
                                                     const QString &redirectUri,
                                                     const QString &clientId)
{
    QNetworkAccessManager *managerWS = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!managerWS) {
        LOG_WARN("ExtRequest", QStringLiteral("intervalsIcuOAuthExchange: NetworkManagerWS not available"));
        return nullptr;
    }
    if (clientId.isEmpty()) {
        LOG_WARN("ExtRequest", QStringLiteral("intervalsIcuOAuthExchange: client_id is empty"));
        return nullptr;
    }

    QJsonObject body;
    body.insert(QStringLiteral("code"),         code);
    body.insert(QStringLiteral("redirect_uri"), redirectUri);
    body.insert(QStringLiteral("client_id"),    clientId);

    QNetworkRequest request;
    request.setUrl(QUrl(URL_TOKEN_ICV));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
#ifndef Q_OS_WASM
    // Identify ourselves to the proxy's allow-list.  We deliberately do NOT
    // set Origin: the desktop build is not a browser and is not subject to
    // CORS.  (On WASM the browser sets Origin itself.)
    request.setRawHeader(INTERVALS_PROXY_CLIENT_HEADER.toUtf8(),
                         INTERVALS_PROXY_DESKTOP_CLIENT_VALUE.toUtf8());
#endif

    return managerWS->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
}

