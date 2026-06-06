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
    static void parseJsonObjectAccount(QString data);
    static QString parseJsonObjectVersion(const QString &data);
    static bool isVersionNewer(const QString &currentVersion, const QString &latestVersion);
    static void parseJsonStravaObject(QString data);
    static int parseIdJsonStravaUploadObject(QString data);
    static int parseStravaUploadStatus(QString data);

    static void parseJsonTPObject(QString data);

    static QList<Sensor> parseJsonSensorList(QString data);
    static QList<Radio> parseJsonRadioList(QString data);
    static QList<Achievement> parseJsonAchievementList(QString data);

    /// Local radio list — JSON file under the MaximumTrainer document root.
    /// loadLocalRadioList() returns the parsed list; if the file is missing
    /// or unreadable it writes the bundled defaults and returns those, so
    /// the caller always gets at least a few stations to play.
    static QList<Radio> loadLocalRadioList();
    static bool         saveLocalRadioList(const QList<Radio>& lstRadio);
    static QList<Radio> getDefaultRadioList();
    static QString      getLocalRadioListPath();
    static QSet<int> parseJsonAchievementListForUser(QString data);

    /// Intervals.icu — parse the GET /api/v1/athlete/{id} response.
    /// Updates the global Account object with name, weight, FTP, and LTHR.
    static void parseJsonIntervalsIcuAthlete(const QString &data);

    /// Intervals.icu — parse the GET /api/v1/athlete/{id}/settings response.
    /// Updates the global Account object with hr_zones and power_zones lists.
    static void parseJsonIntervalsIcuSettings(const QString &data);

    /// Intervals.icu OAuth2 — parse a token endpoint response and store the
    /// access_token, refresh_token, and athlete_id into the global Account.
    static void parseJsonIntervalsIcuOAuthToken(const QString &data);






private :


};

#endif // UTIL_H
