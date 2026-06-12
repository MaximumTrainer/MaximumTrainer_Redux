#include "intervalsicudao.h"

#include <QByteArray>

#include "environnement.h"
#include "logger.h"



// ───────────────────────────────────────────────────────────────────────────────
QNetworkRequest IntervalsIcuDAO::buildRequest(const QString &url, const QString &apiKey)
{
    QNetworkRequest request;
    request.setUrl(QUrl(url));

    // Intervals.icu uses HTTP Basic Auth: username="API_KEY", password=<apiKey>
    const QString credentials = QStringLiteral("API_KEY:") + apiKey;
    const QByteArray encoded   = credentials.toUtf8().toBase64();
    request.setRawHeader("Authorization", QByteArray("Basic ") + encoded);
    request.setRawHeader("Accept", "application/json");

    return request;
}


// ───────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}
QNetworkReply* IntervalsIcuDAO::getAthlete(const QString &athleteId, const QString &apiKey)
{
    QNetworkAccessManager *manager = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuDAO", QStringLiteral("getAthlete: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = urlIntervalsIcuApi + athleteId;
    QNetworkRequest request = buildRequest(url, apiKey);

    return manager->get(request);
}


// ───────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}/sport-settings
// (the former /settings endpoint does not exist — it always returned 404)
QNetworkReply* IntervalsIcuDAO::getAthleteSettings(const QString &athleteId, const QString &apiKey)
{
    QNetworkAccessManager *manager = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuDAO", QStringLiteral("getAthleteSettings: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = urlIntervalsIcuApi + athleteId + QStringLiteral("/sport-settings");
    QNetworkRequest request = buildRequest(url, apiKey);

    return manager->get(request);
}


// ───────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{athleteId}/workouts/{workoutId}.zwo
QNetworkReply* IntervalsIcuDAO::downloadWorkoutZwo(const QString &athleteId, const QString &workoutId, const QString &apiKey)
{
    QNetworkAccessManager *manager = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuDAO", QStringLiteral("downloadWorkoutZwo: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = urlIntervalsIcuApi + athleteId
                        + QStringLiteral("/workouts/")
                        + workoutId
                        + QStringLiteral(".zwo");
    QNetworkRequest request = buildRequest(url, apiKey);

    return manager->get(request);
}


// ───────────────────────────────────────────────────────────────────────────────
QNetworkRequest IntervalsIcuDAO::buildBearerRequest(const QString &url, const QString &bearerToken)
{
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    request.setRawHeader("Authorization", QByteArray("Bearer ") + bearerToken.toUtf8());
    request.setRawHeader("Accept", "application/json");
    return request;
}

// ───────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}  (OAuth2 Bearer token variant)
QNetworkReply* IntervalsIcuDAO::getAthleteBearer(const QString &athleteId, const QString &bearerToken)
{
    QNetworkAccessManager *manager = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuDAO", QStringLiteral("getAthleteBearer: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = urlIntervalsIcuApi + athleteId;
    return manager->get(buildBearerRequest(url, bearerToken));
}

// ───────────────────────────────────────────────────────────────────────────────
// GET /api/v1/athlete/{id}/settings  (OAuth2 Bearer token variant)
QNetworkReply* IntervalsIcuDAO::getAthleteSettingsBearer(const QString &athleteId, const QString &bearerToken)
{
    QNetworkAccessManager *manager = qApp->property("NetworkManagerWS").value<QNetworkAccessManager*>();
    if (!manager) {
        LOG_WARN("IntervalsIcuDAO", QStringLiteral("getAthleteSettingsBearer: NetworkManagerWS not available"));
        return nullptr;
    }

    const QString url = urlIntervalsIcuApi + athleteId + QStringLiteral("/sport-settings");
    return manager->get(buildBearerRequest(url, bearerToken));
}
