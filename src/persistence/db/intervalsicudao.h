#ifndef INTERVALSICUDAO_H
#define INTERVALSICUDAO_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>

/// Data-access object for the Intervals.icu REST API.
///
/// Authentication supports two modes:
///  1. HTTP Basic Auth — username is the literal string "API_KEY" and the
///     password is the user's personal API key (legacy / manual-entry flow).
///  2. OAuth2 Bearer token — used after the OAuth2 login flow; pass an empty
///     apiKey and provide the bearer token via the overloads that accept it.
///
/// Endpoints used:
///   GET /api/v1/athlete/{id}          – basic profile (name, weight, FTP, LTHR)
///   GET /api/v1/athlete/{id}/sport-settings – per-sport FTP/LTHR + zones
class IntervalsIcuDAO
{
public:
    /// Fetch the athlete's basic profile from Intervals.icu (API-key auth).
    /// @param athleteId  The Intervals.icu athlete ID (e.g. "i12345").
    /// @param apiKey     The user's Intervals.icu API key.
    /// @return The pending QNetworkReply; caller must connect finished().
    static QNetworkReply* getAthlete(const QString &athleteId, const QString &apiKey);

    /// Fetch the athlete's basic profile using an OAuth2 Bearer token.
    /// Use athlete ID "0" (the Intervals.icu "current user" sentinel) when the
    /// real athlete ID is not yet known.
    /// @param athleteId   The Intervals.icu athlete ID, or "0" for the current user.
    /// @param bearerToken The OAuth2 access token.
    static QNetworkReply* getAthleteBearer(const QString &athleteId, const QString &bearerToken);

    /// Fetch the athlete's detailed training-zone settings from Intervals.icu.
    /// @param athleteId  The Intervals.icu athlete ID (e.g. "i12345").
    /// @param apiKey     The user's Intervals.icu API key.
    /// @return The pending QNetworkReply; caller must connect finished().
    static QNetworkReply* getAthleteSettings(const QString &athleteId, const QString &apiKey);

    /// Fetch training-zone settings using an OAuth2 Bearer token.
    static QNetworkReply* getAthleteSettingsBearer(const QString &athleteId, const QString &bearerToken);

    /// Download a workout as a ZWO (Zwift workout XML) file from Intervals.icu.
    /// Endpoint: GET /api/v1/athlete/{athleteId}/workouts/{workoutId}.zwo
    /// @param athleteId  The Intervals.icu athlete ID (e.g. "i12345").
    /// @param workoutId  The numeric workout ID shown on the calendar (e.g. "12345678").
    /// @param apiKey     The user's Intervals.icu API key.
    /// @return The pending QNetworkReply; caller must connect finished().
    static QNetworkReply* downloadWorkoutZwo(const QString &athleteId, const QString &workoutId, const QString &apiKey);

private:
    /// Build a QNetworkRequest with the Intervals.icu Basic Auth header set.
    static QNetworkRequest buildRequest(const QString &url, const QString &apiKey);
    /// Build a QNetworkRequest with an OAuth2 Bearer Authorization header.
    static QNetworkRequest buildBearerRequest(const QString &url, const QString &bearerToken);
};

#endif // INTERVALSICUDAO_H
