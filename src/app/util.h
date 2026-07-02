#ifndef UTIL_H
#define UTIL_H

#include <QtCore>
#include <QApplication>

#include "qwt_plot.h"
#include "qwt_plot_grid.h"
#include "qwt_plot_histogram.h"
#include "workout.h"
#include "interval.h"
#include "sensor.h"
#include "radio.h"
#include "achievement.h"


class Util
{
public:
    enum Color
    {
        LINE_POWER,
        SQUARE_POWER,

        LINE_CADENCE,
        SQUARE_CADENCE,

        LINE_HEARTRATE,
        SQUARE_HEARTRATE,

        LINE_SPEED,

        TOO_LOW,
        TOO_HIGH,
        ON_TARGET,
        NOT_DONE,
        DONE,

        BALANCE_POWER_TXT,
        LINE_ON_TARGET_GRAPH,
    };



    Util();



    static double convertQTimeToSecD(const QTime &time);
    static QTime convertMinutesToQTime(double minutes);

    static QString cleanQString(QString toClean);
    static QString cleanForOsSaving(QString toClean);


    static QString showCurrentTimeAsString(const QTime &time);
    static QString showQTimeAsString(const QTime &time);
    static QString showQTimeAsStringWithMs(const QTime &time);
    static QString getStringFromUCHAR(unsigned char* ch);
    static QColor getColor(Color color);

    // Load a resource pixmap scaled into a `logicalSize` square, rendered crisp
    // for the given device pixel ratio. The result carries the DPR, so a QLabel
    // or QIcon lays it out at logicalSize device-independent px but renders at
    // full resolution - no blur on HiDPI / fractional-scale (e.g. Windows 150%)
    // displays. Pass a widget's devicePixelRatioF() (or qApp->devicePixelRatio()).
    static QPixmap loadIconForDpr(const QString &resourcePath, int logicalSize, qreal devicePixelRatio);


    //// Folders
    static bool checkFolderPathIsValidForWrite(QString path);
    static QString getMaximumTrainerDocumentPath();
    static QString getSystemPathWorkout();
    static QString getSystemPathHistory();



    static QString getSystemPathHelperReturnDefaultLoc(QString docType); //workout, history



    /// --------


    static QStringList getListFiles(QString fileType); //.workout
    static void openWorkoutFolder(QString workoutPath);
    static void openHistoryFolder();


    static bool checkFileNameAlreadyExist(QString pathFile);
    static void deleteLocalFile(QString fileName);


    // Zip, Unzip
    static bool zipFileToDisk(QString filename, QString zipFilename, bool useGzip);
    static bool unzipFile(QString zipFilename , QString filename);
    // used for Gzip convert
    static QByteArray zipFileHelperConvertToGzip(const QByteArray& data);
    static quint32 crc32buf(const QByteArray& data);
    static quint32 updateCRC32(unsigned char ch, quint32 crc);





    /// Parse JSON
    static QString parseJsonObjectVersion(const QString &data);
    static bool isVersionNewer(const QString &currentVersion, const QString &latestVersion);
    static void parseJsonStravaObject(QString data);
    static qint64 parseIdJsonStravaUploadObject(QString data);
    static int parseStravaUploadStatus(QString data);
    static qint64 parseStravaActivityId(QString data);

    static QList<Radio> parseJsonRadioList(QString data);

    /// Local radio list — JSON file under the MaximumTrainer document root.
    /// loadLocalRadioList() returns the parsed list; if the file is missing
    /// or unreadable it writes the bundled defaults and returns those, so
    /// the caller always gets at least a few stations to play.
    static QList<Radio> loadLocalRadioList();
    static bool         saveLocalRadioList(const QList<Radio>& lstRadio);
    static QList<Radio> getDefaultRadioList();
    static QString      getLocalRadioListPath();

    /// Intervals.icu — parse the GET /api/v1/athlete/{id} response.
    /// Updates the global Account object with name, weight, FTP, and LTHR.
    static void parseJsonIntervalsIcuAthlete(const QString &data);

    /// Intervals.icu — parse the GET /api/v1/athlete/{id}/sport-settings
    /// response (array of per-sport settings).  Applies the cycling entry's
    /// FTP (indoor_ftp preferred), LTHR, and absolute zone bounds to the
    /// global Account.  Returns true when an FTP or LTHR value was applied.
    static bool parseJsonIntervalsIcuSettings(const QString &data);

    /// Intervals.icu OAuth2 — parse a token endpoint response and store the
    /// access_token, refresh_token, and athlete_id into the global Account.
    static void parseJsonIntervalsIcuOAuthToken(const QString &data);

    /// Intervals.icu OAuth2 — parse a JSON error payload returned by the
    /// Cloudflare Worker on a failed token exchange (see
    /// workers/intervals-cors-proxy/worker.js).  The payload has the shape
    /// { "error": "<code>" } where <code> is one of missing_client_id,
    /// unauthorized_client, kv_unavailable, worker_misconfigured,
    /// unsupported_grant_type, upstream_error, internal_error, or an
    /// upstream OAuth2 error code (invalid_grant, invalid_client, ...).
    /// Returns the error code string, or an empty QString if the payload is
    /// not JSON, not an object, or does not carry an "error" field.
    static QString parseJsonIntervalsIcuOAuthErrorPayload(const QString &data);






private :


};

#endif // UTIL_H
