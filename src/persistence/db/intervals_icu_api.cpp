#include "intervals_icu_api.h"

#include <QCoreApplication>
#include <QByteArray>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include "logger.h"

// ─────────────────────────────────────────────────────────────────────────────
QNetworkRequest IntervalsIcuApi::buildRequest(const QString &url, const QString &apiKey)
{
    QNetworkRequest request;
    request.setUrl(QUrl(url));

    // Intervals.icu uses HTTP Basic Auth: username="API_KEY", password=<apiKey>
    const QString credentials = QStringLiteral("API_KEY:") + apiKey;
    request.setRawHeader("Authorization",
                         QByteArray("Basic ") + credentials.toUtf8().toBase64());
    request.setRawHeader("Accept", "application/json");

    return request;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}
QNetworkReply* IntervalsIcuApi::getAthlete(const QString &athleteId, const QString &apiKey)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("getAthlete: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId;
    return manager->get(buildRequest(url, apiKey));
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}/events?oldest=YYYY-MM-DD&newest=YYYY-MM-DD
QNetworkReply* IntervalsIcuApi::getEvents(const QString &athleteId, const QString &apiKey,
                                              const QDate &startDate, const QDate &endDate)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("getEvents: NetworkManagerWS not available"));
        return nullptr;
    }

    QUrl url(QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
             + QStringLiteral("/events"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("oldest"), startDate.toString(Qt::ISODate));
    query.addQueryItem(QStringLiteral("newest"), endDate.toString(Qt::ISODate));
    url.setQuery(query);

    return manager->get(buildRequest(url.toString(), apiKey));
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}/workouts
QNetworkReply* IntervalsIcuApi::getWorkouts(const QString &athleteId, const QString &apiKey)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("getWorkouts: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/workouts");
    return manager->get(buildRequest(url, apiKey));
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}/workouts/{workoutId}
QNetworkReply* IntervalsIcuApi::getWorkout(const QString &athleteId,
                                               const QString &workoutId,
                                               const QString &apiKey)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("getWorkout: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/workouts/") + workoutId;
    return manager->get(buildRequest(url, apiKey));
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /api/v1/athlete/{id}/download-workout.zwo
QNetworkReply* IntervalsIcuApi::convertWorkoutToZwo(const QString &athleteId,
                                                        const QString &apiKey,
                                                        const QByteArray &workoutJson)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("convertWorkoutToZwo: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/download-workout.zwo");
    QNetworkRequest req = buildRequest(url, apiKey);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    return manager->post(req, workoutJson);
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}/workouts/{workoutId}.zwo
QNetworkReply* IntervalsIcuApi::downloadWorkoutZwo(const QString &athleteId,
                                                       const QString &workoutId,
                                                       const QString &apiKey)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("downloadWorkoutZwo: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/workouts/") + workoutId
                        + QStringLiteral(".zwo");
    return manager->get(buildRequest(url, apiKey));
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}/events/{eventId}/download.zwo
QNetworkReply* IntervalsIcuApi::downloadEventZwo(const QString &athleteId,
                                                     const QString &eventId,
                                                     const QString &apiKey)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("downloadEventZwo: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/events/") + eventId
                        + QStringLiteral("/download.zwo");
    return manager->get(buildRequest(url, apiKey));
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}/workouts/{workoutId}.mrc
QNetworkReply* IntervalsIcuApi::downloadWorkoutMrc(const QString &athleteId,
                                                       const QString &workoutId,
                                                       const QString &apiKey)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("downloadWorkoutMrc: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/workouts/") + workoutId
                        + QStringLiteral(".mrc");
    return manager->get(buildRequest(url, apiKey));
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /api/v1/athlete/{id}/workouts
QNetworkReply* IntervalsIcuApi::createWorkout(const QString &athleteId,
                                                   const QString &apiKey,
                                                   const QByteArray &json)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("createWorkout: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/workouts");
    QNetworkRequest req = buildRequest(url, apiKey);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    return manager->post(req, json);
}

// ─────────────────────────────────────────────────────────────────────────────
// DELETE /api/v1/athlete/{id}/workouts/{workoutId}
QNetworkReply* IntervalsIcuApi::deleteWorkout(const QString &athleteId,
                                                   const QString &workoutId,
                                                   const QString &apiKey)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("deleteWorkout: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/workouts/") + workoutId;
    return manager->deleteResource(buildRequest(url, apiKey));
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}/folders
QNetworkReply* IntervalsIcuApi::listFolders(const QString &athleteId,
                                                const QString &apiKey)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("listFolders: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/folders");
    return manager->get(buildRequest(url, apiKey));
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /api/v1/athlete/{id}/activities  (multipart/form-data)
QNetworkReply* IntervalsIcuApi::uploadActivity(const QString &athleteId,
                                                    const QString &apiKey,
                                                    const QByteArray &data,
                                                    const QString &filename)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("uploadActivity: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/activities");

    // The request must use HTTP Basic Auth but NOT set Content-Type manually —
    // Qt sets the multipart boundary automatically when QHttpMultiPart is used.
    QNetworkRequest req;
    req.setUrl(QUrl(url));
    const QString credentials = QStringLiteral("API_KEY:") + apiKey;
    req.setRawHeader("Authorization",
                     QByteArray("Basic ") + credentials.toUtf8().toBase64());

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(filename)));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant(QStringLiteral("application/octet-stream")));
    filePart.setBody(data);
    multiPart->append(filePart);

    QNetworkReply *reply = manager->post(req, multiPart);
    multiPart->setParent(reply);   // ensures multiPart is deleted when reply is deleted
    return reply;
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}/activities?oldest=YYYY-MM-DD&newest=YYYY-MM-DD
QNetworkReply* IntervalsIcuApi::getActivities(const QString &athleteId,
                                                   const QString &apiKey,
                                                   const QDate &startDate,
                                                   const QDate &endDate)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("getActivities: NetworkManagerWS not available"));
        return nullptr;
    }

    QUrl url(QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
             + QStringLiteral("/activities"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("oldest"), startDate.toString(Qt::ISODate));
    query.addQueryItem(QStringLiteral("newest"), endDate.toString(Qt::ISODate));
    url.setQuery(query);

    return manager->get(buildRequest(url.toString(), apiKey));
}

// ─────────────────────────────────────────────────────────────────────────────
// DELETE /api/v1/athlete/{id}/activities/{activityId}
QNetworkReply* IntervalsIcuApi::deleteActivity(const QString &athleteId,
                                                    const QString &activityId,
                                                    const QString &apiKey)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("deleteActivity: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/activities/") + activityId;
    return manager->deleteResource(buildRequest(url, apiKey));
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /api/v1/athlete/{id}/events
QNetworkReply* IntervalsIcuApi::createEvent(const QString &athleteId,
                                                 const QString &apiKey,
                                                 const QByteArray &json)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("createEvent: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/events");
    QNetworkRequest req = buildRequest(url, apiKey);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/json"));
    return manager->post(req, json);
}

// ─────────────────────────────────────────────────────────────────────────────
// DELETE /api/v1/athlete/{id}/events/{eventId}
QNetworkReply* IntervalsIcuApi::deleteEvent(const QString &athleteId,
                                                 const QString &eventId,
                                                 const QString &apiKey)
{
    QNetworkAccessManager *manager =
        qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuApi", QStringLiteral("deleteEvent: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = QLatin1String(BASE_URL) + QStringLiteral("/athlete/") + athleteId
                        + QStringLiteral("/events/") + eventId;
    return manager->deleteResource(buildRequest(url, apiKey));
}
