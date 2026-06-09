#include "strava_service.h"
#include "logger.h"
#include "environnement.h"   // URL_TOKEN_STRAVA, INTERVALS_PROXY_CLIENT_HEADER

#include <QCoreApplication>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QFileInfo>
#include <QUrlQuery>

static const QString STRAVA_UPLOAD_URL    = QStringLiteral("https://www.strava.com/api/v3/uploads");
static const QString STRAVA_DEAUTH_URL    = QStringLiteral("https://www.strava.com/oauth/deauthorize");

// ─────────────────────────────────────────────────────────────────────────────
void StravaService::setAccessToken(const QString &token)
{
    m_accessToken = token;
}

// ─────────────────────────────────────────────────────────────────────────────
QNetworkAccessManager* StravaService::networkManager()
{
    auto *mgr = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!mgr)
        LOG_WARN("StravaService", QStringLiteral("NetworkManagerWS not available"));
    return mgr;
}

// ─────────────────────────────────────────────────────────────────────────────
QNetworkRequest StravaService::buildBearerRequest(const QString &url) const
{
    QNetworkRequest req;
    req.setUrl(QUrl(url));
    req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    return req;
}

// ─────────────────────────────────────────────────────────────────────────────
QNetworkReply* StravaService::uploadActivity(const QString &filePath,
                                              const QString &name,
                                              const QString &description,
                                              bool isPrivate,
                                              bool onTrainer,
                                              const QString &activityType)
{
    auto *mgr = networkManager();
    if (!mgr) return nullptr;

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        LOG_WARN("StravaService", QStringLiteral("uploadActivity: file not found: ") + filePath);
        return nullptr;
    }

    const QString fileName = fileInfo.fileName();
    LOG_INFO("StravaService", QStringLiteral("uploadActivity: ") + filePath);

    const QString fullDescription =
        description + QStringLiteral(" - Activity done with MaximumTrainer.com");

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    auto addField = [&](const QString &fieldName, const QByteArray &value) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"") + fieldName + QLatin1Char('"'));
        part.setBody(value);
        multiPart->append(part);
    };

    addField(QStringLiteral("name"),         name.toUtf8());
    addField(QStringLiteral("description"),  fullDescription.toUtf8());
    addField(QStringLiteral("private"),      isPrivate  ? "1" : "0");
    addField(QStringLiteral("trainer"),      onTrainer  ? "1" : "0");
    addField(QStringLiteral("activity_type"), activityType.toUtf8());
    addField(QStringLiteral("data_type"),    "fit");

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QStringLiteral("form-data; name=\"file\"; filename=\"") + fileName + QLatin1Char('"'));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/octet-stream"));

    auto *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        LOG_WARN("StravaService", QStringLiteral("uploadActivity: cannot open file: ") + filePath);
        delete file;
        delete multiPart;
        return nullptr;
    }
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QNetworkRequest req;
    req.setUrl(QUrl(STRAVA_UPLOAD_URL));
    req.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());

    auto *reply = mgr->post(req, multiPart);
    multiPart->setParent(reply);
    return reply;
}

// ─────────────────────────────────────────────────────────────────────────────
QNetworkReply* StravaService::checkUploadStatus(qint64 uploadId)
{
    auto *mgr = networkManager();
    if (!mgr) return nullptr;

    const QString url = STRAVA_UPLOAD_URL + QLatin1Char('/') + QString::number(uploadId);
    return mgr->get(buildBearerRequest(url));
}

// ─────────────────────────────────────────────────────────────────────────────
QNetworkReply* StravaService::deauthorize()
{
    auto *mgr = networkManager();
    if (!mgr) return nullptr;

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("access_token"), m_accessToken);

    QNetworkRequest req;
    req.setUrl(QUrl(STRAVA_DEAUTH_URL));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));

    return mgr->post(req, body.toString(QUrl::FullyEncoded).toUtf8());
}

// ─────────────────────────────────────────────────────────────────────────────
// Token exchange / refresh go through the Strava token Worker (URL_TOKEN_STRAVA)
// so the client_secret never lives in the app. The worker injects client_id +
// client_secret; the app sends only the grant-specific fields.
QNetworkRequest StravaService::buildTokenWorkerRequest()
{
    QNetworkRequest req;
    req.setUrl(QUrl(URL_TOKEN_STRAVA));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
#ifndef Q_OS_WASM
    // Desktop is not a browser and sends no Origin; identify to the worker's
    // allow-list with X-MT-Client (mirrors the Intervals proxy). On WASM the
    // browser sets Origin automatically and refuses header overrides.
    req.setRawHeader(INTERVALS_PROXY_CLIENT_HEADER.toUtf8(),
                     INTERVALS_PROXY_DESKTOP_CLIENT_VALUE.toUtf8());
#endif
    return req;
}

QNetworkReply* StravaService::exchangeAuthCode(const QString &code,
                                               const QString &redirectUri)
{
    auto *mgr = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!mgr) {
        LOG_WARN("StravaService", QStringLiteral("exchangeAuthCode: NetworkManagerWS not available"));
        return nullptr;
    }

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("grant_type"),   QStringLiteral("authorization_code"));
    body.addQueryItem(QStringLiteral("code"),         code);
    body.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);

    return mgr->post(buildTokenWorkerRequest(), body.toString(QUrl::FullyEncoded).toUtf8());
}

QNetworkReply* StravaService::refreshToken(const QString &refreshToken)
{
    auto *mgr = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!mgr) {
        LOG_WARN("StravaService", QStringLiteral("refreshToken: NetworkManagerWS not available"));
        return nullptr;
    }

    QUrlQuery body;
    body.addQueryItem(QStringLiteral("grant_type"),    QStringLiteral("refresh_token"));
    body.addQueryItem(QStringLiteral("refresh_token"), refreshToken);

    return mgr->post(buildTokenWorkerRequest(), body.toString(QUrl::FullyEncoded).toUtf8());
}
