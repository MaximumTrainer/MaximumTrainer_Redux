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
QNetworkReply* ExtRequest::stravaCheckUploadStatus(QString access_token, qint64 uploadID) {

    QNetworkAccessManager *managerWS = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!managerWS) {
        LOG_WARN("ExtRequest", QStringLiteral("stravaCheckUploadStatus: NetworkManagerWS not available"));
        return nullptr;
    }

    QString urlStrava = "https://www.strava.com/api/v3/uploads/" + QString::number(uploadID);

    QNetworkRequest request;
    request.setUrl(QUrl(urlStrava));
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    request.setRawHeader("Authorization", "Bearer " + access_token.toUtf8());

    QNetworkReply *replyLogin = managerWS->get(request);

    return replyLogin;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QNetworkReply* ExtRequest::stravaUploadFile(QString access_token, QString activityName, QString activityDescription,
                                            bool activityOnTrainer, bool activityIsPrivate, QString typeActivity, QString pathToFile) {


    QFileInfo fileInfo(pathToFile);
    QString fileName = fileInfo.fileName(); //just the filename without the path
    LOG_INFO("ExtRequest", QStringLiteral("stravaUploadFile: ") + pathToFile);

    QNetworkAccessManager *managerWS = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!managerWS) {
        LOG_WARN("ExtRequest", QStringLiteral("stravaUploadFile: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString urlStrava = "https://www.strava.com/api/v3/uploads";
    const QString fileType = "fit";

    QString fileActivityType;
    if (typeActivity == "course") {
        fileActivityType = "ride";
    }
    else {
        //        fileActivityType = "workout";
        //        fileActivityType = "cycling";
        fileActivityType = "ride";
    }

    QString reference = QApplication::translate("ExtRequest: ", " - Activity done with MaximumTrainer.com");
    activityDescription = activityDescription + reference;

    //activities without lat/lng info in the file are auto marked as stationary, set to 1 to force
    QString activityOnTrainerStr = "1";
    if (!activityOnTrainer) {
        activityOnTrainerStr = "0";
    }

    //activities without lat/lng info in the file are auto marked as stationary, set to 1 to force
    QString activityIsPrivateStr = "1";
    if (!activityIsPrivate) {
        activityIsPrivateStr = "0";
    }



    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart accessPart;
    accessPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"access_token\""));
    accessPart.setBody(access_token.toUtf8());

    QHttpPart activityNamePart;
    activityNamePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"name\""));
    activityNamePart.setBody(activityName.toUtf8());

    QHttpPart activityDescriptionPart;
    activityDescriptionPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"description\""));
    activityDescriptionPart.setBody(activityDescription.toUtf8());

    QHttpPart activityPrivatePart;
    activityPrivatePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"private\""));
    activityPrivatePart.setBody(activityIsPrivateStr.toUtf8());

    QHttpPart activityTrainerPart;
    activityTrainerPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"trainer\""));
    activityTrainerPart.setBody(activityOnTrainerStr.toUtf8());

    QHttpPart activityTypePart;
    activityTypePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"activity_type\""));
    activityTypePart.setBody(fileActivityType.toUtf8());

    QHttpPart fileDataPart;
    QString contentDispoHeaderStr = "form-data; name=\"file\"; filename=\"" + fileName + "\"";
    fileDataPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant(contentDispoHeaderStr));
    fileDataPart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));

    QFile *file = new QFile(pathToFile);
    if (!file->open(QIODevice::ReadOnly)) {
        LOG_WARN("ExtRequest", QStringLiteral("stravaUploadFile: cannot open file: ") + pathToFile);
        delete file;
        delete multiPart;
        return nullptr;
    }

    LOG_DEBUG("ExtRequest", QStringLiteral("stravaUploadFile: file size ") + QString::number(file->size()));
    fileDataPart.setBodyDevice(file);
    file->setParent(multiPart); // we cannot delete the file now, so delete it with the multiPart


    QHttpPart fileTypePart;
    fileTypePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"data_type\""));
    fileTypePart.setBody(fileType.toUtf8());


    multiPart->append(accessPart);
    multiPart->append(activityNamePart);
    multiPart->append(activityDescriptionPart);
    multiPart->append(activityPrivatePart);
    multiPart->append(activityTrainerPart);
    multiPart->append(activityTypePart);
    multiPart->append(fileDataPart);
    multiPart->append(fileTypePart);


    QUrl url(urlStrava);
    QNetworkRequest request(url);

    QNetworkReply *reply = managerWS->post(request, multiPart);
    multiPart->setParent(reply); // delete the multiPart with the reply

    return reply;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// POST <Cloudflare proxy>/proxy/oauth/token  (grant_type=authorization_code)
/// Exchanges an authorization code for an OAuth2 access + refresh token pair.
/// Both desktop and WASM builds route this through the Cloudflare Worker
/// CORS proxy (URL_TOKEN_ICV is the proxied URL).  On WASM the browser sets
/// the Origin header automatically (https://maximumtrainer.github.io); on
/// desktop we send INTERVALS_PROXY_CLIENT_HEADER instead (the desktop build
/// is not a browser and is not subject to CORS — see worker.js).
///
/// Note: A client_secret is omitted because Intervals.icu client 259 is
/// registered as a public client (no client secret required).  This is a
/// plain Authorization Code flow without PKCE.
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

    QNetworkRequest request;
    request.setUrl(QUrl(URL_TOKEN_ICV));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
#ifndef Q_OS_WASM
    // Desktop: identify ourselves to the Cloudflare proxy's allow-list.
    // We deliberately do NOT set Origin here: the desktop build is not a
    // browser, has no web origin, and is not subject to CORS.  Instead we
    // send X-MT-Client so the worker can distinguish our own desktop
    // traffic from random third-party callers.  (On WASM the browser sets
    // Origin automatically and refuses to let application code override
    // it, so this header would be ignored there.)
    request.setRawHeader(INTERVALS_PROXY_CLIENT_HEADER.toUtf8(),
                         INTERVALS_PROXY_DESKTOP_CLIENT_VALUE.toUtf8());
#endif

    return managerWS->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// POST <Cloudflare proxy>/proxy/oauth/token  (grant_type=refresh_token)
/// Exchanges a stored refresh token for a new access + refresh token pair.
/// Routed through the Cloudflare Worker CORS proxy (see
/// intervalsIcuOAuthExchange above for the Origin-header rationale).
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

    QNetworkRequest request;
    request.setUrl(QUrl(URL_TOKEN_ICV));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
#ifndef Q_OS_WASM
    request.setRawHeader(INTERVALS_PROXY_CLIENT_HEADER.toUtf8(),
                         INTERVALS_PROXY_DESKTOP_CLIENT_VALUE.toUtf8());
#endif

    LOG_INFO("ExtRequest", QStringLiteral("intervalsIcuOAuthRefresh: refreshing access token"));
    return managerWS->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}
