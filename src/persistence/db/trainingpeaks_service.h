#ifndef TRAININGPEAKS_SERVICE_H
#define TRAININGPEAKS_SERVICE_H

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>

///
/// API client for TrainingPeaks (https://api.trainingpeaks.com).
///
/// Authentication: OAuth2 Bearer token set via setAccessToken().
/// Use the static refreshToken() to exchange a refresh token for a new
/// access token when the current one expires.
///
/// The client_id and client_secret for the token exchange are taken from
/// the TP_CLIENT_SECRET compile-time define (see MaximumTrainer.pro).
///
/// All methods are non-blocking and return a pending QNetworkReply*.
/// The caller must connect QNetworkReply::finished to a slot.
///
class TrainingPeaksService
{
public:
    /// Store the OAuth2 Bearer token used for all subsequent instance calls.
    void setAccessToken(const QString &token);

    QString accessToken() const { return m_accessToken; }

    // ── API methods ──────────────────────────────────────────────────────────

    /// Upload a FIT or TCX activity file to TrainingPeaks.
    /// POST https://api.trainingpeaks.com/v1/file/
    /// @param filePath      Absolute path to the activity file (.fit or .tcx).
    /// @param name          Activity title shown in TrainingPeaks.
    /// @param description   Optional activity description.
    /// @param isPublic      true → activity visible to other athletes.
    QNetworkReply* uploadActivity(const QString &filePath,
                                  const QString &name,
                                  const QString &description,
                                  bool isPublic = false);

    /// Exchange a refresh token for a new access token.
    /// POST https://oauth.trainingpeaks.com/oauth/token/
    /// Uses the TP_CLIENT_SECRET compile-time constant set by the build.
    /// @param refreshToken  Refresh token stored from a previous OAuth exchange.
    static QNetworkReply* refreshToken(const QString &refreshToken);

private:
    QString m_accessToken;

    QNetworkRequest buildBearerRequest(const QString &url) const;
    static QNetworkAccessManager* networkManager();
};

#endif // TRAININGPEAKS_SERVICE_H
