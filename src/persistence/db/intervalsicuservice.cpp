#include "intervalsicuservice.h"

#include <QCoreApplication>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QByteArray>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QDebug>
#include "logger.h"

static const QString INTERVALS_BASE_URL = "https://intervals.icu/api/v1";

// ─────────────────────────────────────────────────────────────────────────────
IntervalsIcuService::IntervalsIcuService(QObject *parent)
    : QObject(parent)
{
}

// ─────────────────────────────────────────────────────────────────────────────
void IntervalsIcuService::setCredentials(const QString &apiKey, const QString &athleteId)
{
    m_apiKey    = apiKey;
    m_athleteId = athleteId;
}

// ─────────────────────────────────────────────────────────────────────────────
QNetworkRequest IntervalsIcuService::buildRequest(const QString &path) const
{
    QNetworkRequest request;
    request.setUrl(QUrl(INTERVALS_BASE_URL + path));

    // HTTP Basic Auth: username = "API_KEY", password = user API key
    const QByteArray credentials =
        ("API_KEY:" + m_apiKey).toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + credentials);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");
    return request;
}

// ─────────────────────────────────────────────────────────────────────────────
QNetworkReply *IntervalsIcuService::testConnection()
{
    auto *mgr = qApp->property("NetworkManagerWS")
                    .value<QNetworkAccessManager *>();
    if (!mgr) {
        LOG_WARN("IntervalsIcuService", QStringLiteral("testConnection: NetworkManagerWS not available"));
        return nullptr;
    }
    return mgr->get(buildRequest("/athlete/" + m_athleteId));
}

// ─────────────────────────────────────────────────────────────────────────────
QNetworkReply *IntervalsIcuService::fetchCalendar(const QDate &oldest,
                                                   const QDate &newest)
{
    const QString path = QString("/athlete/%1/events?oldest=%2&newest=%3")
                             .arg(m_athleteId,
                                  oldest.toString(Qt::ISODate),
                                  newest.toString(Qt::ISODate));

    auto *mgr = qApp->property("NetworkManagerWS")
                    .value<QNetworkAccessManager *>();
    if (!mgr) {
        LOG_WARN("IntervalsIcuService", QStringLiteral("fetchCalendar: NetworkManagerWS not available"));
        return nullptr;
    }
    return mgr->get(buildRequest(path));
}

// ─────────────────────────────────────────────────────────────────────────────
QNetworkReply *IntervalsIcuService::downloadWorkoutZwo(const QString &workoutId)
{
    const QString path =
        QString("/athlete/%1/workouts/%2.zwo")
            .arg(m_athleteId, workoutId);

    auto *mgr = qApp->property("NetworkManagerWS")
                    .value<QNetworkAccessManager *>();
    if (!mgr) {
        LOG_WARN("IntervalsIcuService", QStringLiteral("downloadWorkoutZwo: NetworkManagerWS not available"));
        return nullptr;
    }
    return mgr->get(buildRequest(path));
}

// ─────────────────────────────────────────────────────────────────────────────
QNetworkReply *IntervalsIcuService::uploadActivity(const QString &filePath,
                                                    const QString &name,
                                                    const QString &externalId)
{
    // Build URL with optional query parameters
    QUrl url(INTERVALS_BASE_URL + "/athlete/0/activities");
    QUrlQuery q;
    if (!name.isEmpty())
        q.addQueryItem("name", name);
    if (!externalId.isEmpty())
        q.addQueryItem("external_id", externalId);
    if (!q.isEmpty())
        url.setQuery(q);

    QNetworkRequest request;
    request.setUrl(url);

    const QByteArray credentials =
        ("API_KEY:" + m_apiKey).toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + credentials);
    // Content-Type is set automatically by Qt for multipart requests

    const QString suffix = QFileInfo(filePath).suffix().toLower();
    const QString mimeType = (suffix == "fit") ? "application/octet-stream"
                                                : "application/xml";

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QString("form-data; name=\"file\"; filename=\"%1\"")
                           .arg(QFileInfo(filePath).fileName()));
    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        LOG_WARN("IntervalsIcuService",
                 QStringLiteral("uploadActivity: cannot open file: ") + filePath);
        delete file;
        delete multiPart;
        return nullptr;
    }
    file->setParent(multiPart);
    filePart.setBodyDevice(file);
    multiPart->append(filePart);

    auto *mgr = qApp->property("NetworkManagerWS")
                    .value<QNetworkAccessManager *>();
    if (!mgr) {
        LOG_WARN("IntervalsIcuService", QStringLiteral("uploadActivity: NetworkManagerWS not available"));
        delete multiPart; // also deletes file (parented to multiPart)
        return nullptr;
    }
    QNetworkReply *reply = mgr->post(request, multiPart);
    multiPart->setParent(reply);
    return reply;
}

// ─────────────────────────────────────────────────────────────────────────────
// static
QList<IntervalsIcuService::CalendarEvent>
IntervalsIcuService::parseEvents(const QByteArray &data)
{
    QList<CalendarEvent> events;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        LOG_WARN("IntervalsIcuService",
                 QStringLiteral("parseEvents: JSON parse error: ") + err.errorString());
        return events;
    }

    if (!doc.isArray()) {
        LOG_WARN("IntervalsIcuService", QStringLiteral("parseEvents: expected JSON array"));
        return events;
    }

    for (const QJsonValue &val : doc.array()) {
        if (!val.isObject())
            continue;
        const QJsonObject obj = val.toObject();

        CalendarEvent ev;
        // Use toVariant().toLongLong() to preserve full 64-bit IDs on both Qt 5 and Qt 6
        ev.id          = QString::number(obj["id"].toVariant().toLongLong());
        ev.name        = obj["name"].toString();
        ev.type        = obj["type"].toString();
        ev.duration_sec = obj["moving_time"].toInt(0);
        ev.description = obj["description"].toString();

        // workout_id may be absent (free event, not a structured workout)
        if (obj.contains("workout_id") && !obj["workout_id"].isNull())
            ev.workout_id = QString::number(obj["workout_id"].toVariant().toLongLong());

        // start_date_local is ISO 8601 "YYYY-MM-DDThh:mm:ss"
        const QString dateStr =
            obj["start_date_local"].toString().left(10);
        ev.date = QDate::fromString(dateStr, Qt::ISODate);

        events.append(ev);
    }

    return events;
}
