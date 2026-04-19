#include "trainingpeaks_service.h"

#include "environnement.h"
#include "logger.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>

// ── Helpers ───────────────────────────────────────────────────────────────────

void TrainingPeaksService::setAccessToken(const QString &token)
{
    m_accessToken = token;
}

QNetworkAccessManager *TrainingPeaksService::networkManager()
{
    auto *mgr = qApp->property("NetworkManagerWS").value<QNetworkAccessManager *>();
    if (!mgr)
        LOG_WARN("TrainingPeaksService", QStringLiteral("NetworkManagerWS not available"));
    return mgr;
}

QNetworkRequest TrainingPeaksService::buildBearerRequest(const QString &url) const
{
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    return request;
}

// ── uploadActivity ────────────────────────────────────────────────────────────

QNetworkReply *TrainingPeaksService::uploadActivity(const QString &filePath,
                                                     const QString &name,
                                                     const QString &description,
                                                     bool isPublic)
{
    QNetworkAccessManager *mgr = networkManager();
    if (!mgr)
        return nullptr;

    QFileInfo fileInfo(filePath);
    const QString fileName = fileInfo.fileName();
    LOG_INFO("TrainingPeaksService", QStringLiteral("uploadActivity: ") + filePath);

    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        LOG_WARN("TrainingPeaksService",
                 QStringLiteral("uploadActivity: cannot open file: ") + filePath);
        delete file;
        return nullptr;
    }

    const QByteArray base64Data = file->readAll().toBase64();
    file->deleteLater();

    const QString desc = description +
                         QCoreApplication::translate("TrainingPeaksService",
                                                 " - Activity done with MaximumTrainer.com");

    QJsonObject payload;
    payload[QStringLiteral("UploadClient")]     = QStringLiteral("MaximumTrainer.com");
    payload[QStringLiteral("Filename")]         = fileName;
    payload[QStringLiteral("Data")]             = QString::fromLatin1(base64Data);
    payload[QStringLiteral("SetWorkoutPublic")] = isPublic ? QStringLiteral("true")
                                                           : QStringLiteral("false");
    payload[QStringLiteral("Title")]            = name;
    payload[QStringLiteral("Comment")]          = desc;
    payload[QStringLiteral("Type")]             = QStringLiteral("Bike");

    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    QNetworkRequest request = buildBearerRequest(URL_POST_FILE_TP);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());

    return mgr->post(request, body);
}

// ── refreshToken (static) ─────────────────────────────────────────────────────

QNetworkReply *TrainingPeaksService::refreshToken(const QString &refreshToken)
{
    QNetworkAccessManager *mgr = networkManager();
    if (!mgr)
        return nullptr;

    LOG_DEBUG("TrainingPeaksService", QStringLiteral("refreshToken"));

    QUrlQuery postData;
    postData.addQueryItem(QStringLiteral("client_id"),     CLIENT_ID_TP);
    postData.addQueryItem(QStringLiteral("client_secret"), CLIENT_SECRET_TP);
    postData.addQueryItem(QStringLiteral("grant_type"),    QStringLiteral("refresh_token"));
    postData.addQueryItem(QStringLiteral("refresh_token"), refreshToken);

    QNetworkRequest request{QUrl(URL_TOKEN_TP)};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

    return mgr->post(request,
                     postData.toString(QUrl::FullyEncoded).toUtf8());
}
