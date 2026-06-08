#ifndef STRAVA_SERVICE_H
#define STRAVA_SERVICE_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>

///
/// API client for Strava (https://www.strava.com/api/v3).
///
/// Authentication: OAuth2 Bearer token set via setAccessToken().
/// Tokens expire every 6 hours; use the static refreshToken() to obtain a new
/// access token from a stored refresh token.
///
/// All methods are non-blocking and return a pending QNetworkReply*.
/// The caller must connect QNetworkReply::finished to a slot and call
/// reply->readAll() inside it — identical to the existing ExtRequest pattern.
///
class StravaService
{
public:
    /// Store the OAuth2 Bearer token used for all subsequent instance calls.
    void setAccessToken(const QString &token);

    QString accessToken() const { return m_accessToken; }

    // ── API methods ──────────────────────────────────────────────────────────

    /// Upload a FIT activity file to Strava.
    /// POST https://www.strava.com/api/v3/uploads
    /// @param filePath      Absolute path to the .fit file.
    /// @param name          Activity name shown in Strava.
    /// @param description   Activity description (a MaximumTrainer reference is appended).
    /// @param isPrivate     true → visible only to the athlete.
    /// @param onTrainer     true → marks the activity as an indoor trainer ride.
    /// @param activityType  Strava sport type string (default "ride").
    QNetworkReply* uploadActivity(const QString &filePath,
                                  const QString &name,
                                  const QString &description,
                                  bool isPrivate    = false,
                                  bool onTrainer    = true,
                                  const QString &activityType = QStringLiteral("ride"));

    /// Poll the status of a pending Strava upload.
    /// GET https://www.strava.com/api/v3/uploads/{uploadId}
    QNetworkReply* checkUploadStatus(int uploadId);

    /// Revoke the current access token.
    /// POST https://www.strava.com/oauth/deauthorize
    QNetworkReply* deauthorize();

    /// Exchange an OAuth2 authorization code for an access + refresh token pair.
    /// POSTs to the Strava token Worker (URL_TOKEN_STRAVA), which injects the
    /// client_id/client_secret — the secret never lives in the app. Parse the
    /// reply with Util::parseJsonStravaObject() and persist the tokens.
    /// @param code         Authorization code from the OAuth redirect.
    /// @param redirectUri  The exact redirect_uri used in the authorize request.
    static QNetworkReply* exchangeAuthCode(const QString &code,
                                           const QString &redirectUri);

    /// Exchange a refresh token for a new access + refresh token pair, via the
    /// same Strava token Worker (no client_secret in the app).
    /// @param refreshToken  Refresh token stored from the previous OAuth exchange.
    static QNetworkReply* refreshToken(const QString &refreshToken);

private:
    QString m_accessToken;

    QNetworkRequest buildBearerRequest(const QString &url) const;
    static QNetworkAccessManager* networkManager();

    /// Build the POST request to the Strava token Worker, tagging desktop
    /// clients with X-MT-Client so the worker's allow-list accepts them.
    static QNetworkRequest buildTokenWorkerRequest();
};

#endif // STRAVA_SERVICE_H
