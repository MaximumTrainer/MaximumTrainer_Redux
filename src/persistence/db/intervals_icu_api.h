#ifndef INTERVALS_ICU_API_H
#define INTERVALS_ICU_API_H

#include <QDate>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>

/// Static service class for the Intervals.icu REST API.
///
/// Authentication uses HTTP Basic Auth where the username is the literal
/// string "athlete" and the password is the user's personal API key.
///
/// All methods return the pending QNetworkReply; the caller must connect
/// &QNetworkReply::finished to its own slot and call reply->readAll() inside
/// it — identical to the existing ExtRequest pattern.
class IntervalsIcuApi
{
public:
    /// Validate credentials / fetch athlete profile.
    /// GET /athlete/{id}
    static QNetworkReply* getAthlete(const QString &athleteId, const QString &apiKey);

    /// Fetch calendar events for the given date range.
    /// GET /athlete/{id}/events?oldest=YYYY-MM-DD&newest=YYYY-MM-DD
    static QNetworkReply* getEvents(const QString &athleteId, const QString &apiKey,
                                    const QDate &startDate, const QDate &endDate);

    /// Fetch the athlete's workout library.
    /// GET /athlete/{id}/workouts
    static QNetworkReply* getWorkouts(const QString &athleteId, const QString &apiKey);

    /// Fetch a single workout from the athlete's library by ID.
    /// GET /athlete/{id}/workouts/{workoutId}
    static QNetworkReply* getWorkout(const QString &athleteId,
                                     const QString &workoutId,
                                     const QString &apiKey);

    /// Convert a Workout JSON object to a ZWO file.
    /// POST /athlete/{id}/download-workout.zwo
    /// @param workoutJson  UTF-8 encoded Workout JSON object (e.g. from getWorkout).
    static QNetworkReply* convertWorkoutToZwo(const QString &athleteId,
                                              const QString &apiKey,
                                              const QByteArray &workoutJson);

    /// Download a workout as a ZWO file.
    /// GET /athlete/{id}/workouts/{workoutId}.zwo
    static QNetworkReply* downloadWorkoutZwo(const QString &athleteId,
                                             const QString &workoutId,
                                             const QString &apiKey);

    /// Download a workout as an MRC file.
    /// GET /athlete/{id}/workouts/{workoutId}.mrc
    static QNetworkReply* downloadWorkoutMrc(const QString &athleteId,
                                             const QString &workoutId,
                                             const QString &apiKey);

    /// Download a planned calendar event's structured workout as a ZWO file.
    /// Calendar-authored workouts have no library workout_id — the steps live
    /// in the event itself — so the download is keyed on the event id.
    /// GET /athlete/{id}/events/{eventId}/download.zwo
    static QNetworkReply* downloadEventZwo(const QString &athleteId,
                                           const QString &eventId,
                                           const QString &apiKey);

    /// Create a workout in the athlete's library.
    /// POST /athlete/{id}/workouts
    /// @param json  UTF-8 encoded JSON object describing the new workout.
    ///              Minimum required fields: "name" (string), "type" (string).
    /// Returns the pending reply; on success the server responds HTTP 200 with
    /// the created workout object (including its "id" field).
    static QNetworkReply* createWorkout(const QString &athleteId,
                                        const QString &apiKey,
                                        const QByteArray &json);

    /// Delete a workout from the athlete's library.
    /// DELETE /athlete/{id}/workouts/{workoutId}
    static QNetworkReply* deleteWorkout(const QString &athleteId,
                                        const QString &workoutId,
                                        const QString &apiKey);

    /// List all the athlete's workout folders.
    /// GET /athlete/{id}/folders
    static QNetworkReply* listFolders(const QString &athleteId, const QString &apiKey);

    /// Upload a completed activity file (FIT, TCX, GPX) to the athlete's history.
    /// POST /athlete/{id}/activities  (multipart/form-data, field name: "file")
    /// @param data      Raw file bytes (e.g. TCX or FIT content).
    /// @param filename  Filename including extension — determines the format
    ///                  Intervals.icu uses for parsing (e.g. "activity.tcx").
    /// Returns the pending reply; on success HTTP 201 with the created activity
    /// object (including its "id" field).
    static QNetworkReply* uploadActivity(const QString &athleteId,
                                         const QString &apiKey,
                                         const QByteArray &data,
                                         const QString &filename);

    /// Fetch the athlete's recent activities for a date range.
    /// GET /athlete/{id}/activities?oldest=YYYY-MM-DD&newest=YYYY-MM-DD
    static QNetworkReply* getActivities(const QString &athleteId,
                                        const QString &apiKey,
                                        const QDate &startDate,
                                        const QDate &endDate);

    /// Delete an activity from the athlete's history.
    /// DELETE /athlete/{id}/activities/{activityId}
    static QNetworkReply* deleteActivity(const QString &athleteId,
                                         const QString &activityId,
                                         const QString &apiKey);

    /// Create a calendar event (e.g. a planned workout) for the athlete.
    /// POST /athlete/{id}/events
    /// @param json  UTF-8 encoded JSON object.  Required fields:
    ///              "category" (e.g. "WORKOUT"), "start_date_local" (ISO date string),
    ///              "name" (string).
    static QNetworkReply* createEvent(const QString &athleteId,
                                      const QString &apiKey,
                                      const QByteArray &json);

    /// Delete a calendar event.
    /// DELETE /athlete/{id}/events/{eventId}
    static QNetworkReply* deleteEvent(const QString &athleteId,
                                      const QString &eventId,
                                      const QString &apiKey);

private:
    /// Build a QNetworkRequest with Authorization: Basic and Accept: application/json.
    static QNetworkRequest buildRequest(const QString &url, const QString &apiKey);

    static constexpr const char BASE_URL[] = "https://intervals.icu/api/v1";
};

#endif // INTERVALS_ICU_API_H
