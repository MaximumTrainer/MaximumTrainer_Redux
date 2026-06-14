#ifndef INTERVALSICUSERVICE_H
#define INTERVALSICUSERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDate>
#include <QList>
#include <QString>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

///
/// Intervals.icu API client.
///
/// Authentication supports two modes:
///   - HTTP Basic Auth: username "API_KEY" + user's personal API key
///     (configured in intervals.icu → Settings → API).  Call setCredentials().
///   - OAuth2 Bearer: access token from OAuth2 login flow.
///     Call setAccessToken().  Bearer takes precedence over Basic auth when set.
///
/// All methods are non-blocking and return a QNetworkReply* whose
/// finished() signal the caller must connect to a slot.
///
class IntervalsIcuService : public QObject
{
    Q_OBJECT

public:
    /// A calendar event as returned by GET /athlete/{id}/events
    struct CalendarEvent {
        QString id;
        QString name;
        QDate   date;
        QString type;
        int     duration_sec;   ///< moving_time in seconds (0 if unavailable)
        QString category;       ///< event category, e.g. "WORKOUT"
        QString description;

        /// True when the event is a structured/planned workout that can be
        /// downloaded as a .zwo file via its event id. Workouts authored
        /// directly on the calendar carry no separate workout_id — the steps
        /// live in the event itself — so downloadability is keyed on category.
        bool isDownloadableWorkout() const
        {
            return category == QLatin1String("WORKOUT");
        }
    };

    explicit IntervalsIcuService(QObject *parent = nullptr);

    /// Set API-key (Basic auth) credentials.  Call before any API method.
    void setCredentials(const QString &apiKey, const QString &athleteId);

    /// Set an OAuth2 Bearer access token.  When non-empty this takes
    /// precedence over the API key and all requests use Bearer auth.
    void setAccessToken(const QString &accessToken);

    QString athleteId() const { return m_athleteId; }

    // ── API methods ──────────────────────────────────────────────────────────

    /// GET /api/v1/athlete/{id}  — lightweight connection test.
    QNetworkReply *testConnection();

    /// GET /api/v1/athlete/{id}/events  — calendar events in [oldest, newest].
    QNetworkReply *fetchCalendar(const QDate &oldest, const QDate &newest);

    /// GET /api/v1/athlete/{id}/events/{eventId}/download.zwo
    /// Downloads a planned calendar event's structured workout as a .zwo file.
    QNetworkReply *downloadEventZwo(const QString &eventId);

    /// POST /api/v1/athlete/0/activities — upload a FIT/TCX file.
    /// @param filePath  Absolute path to the activity file (.fit or .tcx).
    /// @param name      Optional activity name (sent as query param).
    /// @param externalId Optional external_id to prevent duplicate uploads.
    QNetworkReply *uploadActivity(const QString &filePath,
                                  const QString &name       = QString(),
                                  const QString &externalId = QString());

    // ── Static parsers ───────────────────────────────────────────────────────

    /// Parse a JSON byte array from the /events endpoint into a list of
    /// CalendarEvent structs.  Returns an empty list on parse error.
    static QList<CalendarEvent> parseEvents(const QByteArray &data);

private:
    QNetworkRequest buildRequest(const QString &path) const;
    void applyAuth(QNetworkRequest &request) const;

    QString m_apiKey;
    QString m_athleteId;
    QString m_accessToken; ///< OAuth2 Bearer token (takes precedence over m_apiKey when set)
};

#endif // INTERVALSICUSERVICE_H
