#include "util.h"
#include <memory>
#include <QDebug>
#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "account.h"
#include "settings.h"
#include "xmlutil.h"
#include "myconstants.h"
#include <numeric>




Util::Util() {
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// http://localhost/index.php/api/achievement_rest/achievementuser/id/1/format/json
QSet<int> Util::parseJsonAchievementListForUser(QString data) {


    qDebug() << "OK PARSE parseJsonAchievementListForUser****";

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    //    QJsonObject jsonObj = jsonResponse.object();

    QJsonArray jsonArray = jsonResponse.array();
    qDebug() << "SIZE OF THE ARRAY IS" << jsonArray.size();


    QSet<int> hashAchievementDone;


    /// Loop on achievement done
    for (int i=0; i<jsonArray.size(); i++)
    {
        QJsonValue jsonValue = jsonArray.at(i);
        QJsonObject jsonObj = jsonValue.toObject();

        int idAchievement = jsonObj["achievement_id"].toString().toInt();
        hashAchievementDone.insert(idAchievement);
    }

    return hashAchievementDone;
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// http://localhost/index.php/api/achievement_rest/achievement
QList<Achievement> Util::parseJsonAchievementList(QString data) {


    qDebug() << "OK PARSE Achievement List HERE*****";

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    //    QJsonObject jsonObj = jsonResponse.object();

    QJsonArray jsonArray = jsonResponse.array();
    //    qDebug() << "SIZE OF THE ARRAY IS" << jsonArray.size();


    QList<Achievement> lstAchievement;

    int idDB;
    QString name;
    QString description;
    QString iconUrl;

    Settings *mySettings = qApp->property("User_Settings").value<Settings*>();


    /// Loop on achievement
    for (int i=0; i<jsonArray.size(); i++)
    {
        QJsonValue jsonValue = jsonArray.at(i);
        QJsonObject jsonObj = jsonValue.toObject();


        idDB = jsonObj["id"].toString().toInt();

        if (mySettings->language == "en") {
            name = jsonObj["name_en"].toString();
            description = jsonObj["description_en"].toString();
        }
        //fr
        else  {
            name = jsonObj["name_fr"].toString();
            description = jsonObj["description_fr"].toString();
        }

        iconUrl= jsonObj["icon_url"].toString();


        qDebug() << "Achievement is - idDB:" << idDB << "name:" << name << "description:" << description << "iconUrl:" << iconUrl;

        Achievement achivement(idDB, name, description, iconUrl);
        lstAchievement.append(achivement);
    }

    return lstAchievement;

}


///http://localhost/index.php/api/sensor_rest/sensor/id/1/format/json
///----------------------------------------------- JSON Sensor list -----------------------------------------------------
QList<Sensor> Util::parseJsonSensorList(QString data) {

    qDebug() << "OK PARSE SENSORLIST HERE*****";

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    //    QJsonObject jsonObj = jsonResponse.object();

    QJsonArray jsonArray = jsonResponse.array();
    qDebug() << "SIZE OF THE ARRAY IS" << jsonArray.size();


    QList<Sensor> lstSensor;

    int ant_id;
    int device_type;
    Sensor::SENSOR_TYPE sensorType;
    QString name;
    QString details;



    /// Loop on sensors
    for (int i=0; i<jsonArray.size(); i++)
    {
        QJsonValue jsonValue = jsonArray.at(i);
        QJsonObject jsonObj = jsonValue.toObject();


        ant_id = jsonObj["ant_id"].toString().toInt();
        device_type = jsonObj["device_type"].toString().toInt();
        sensorType = (Sensor::SENSOR_TYPE)device_type;

        name = jsonObj["name"].toString();
        details = jsonObj["details"].toString();

        //        qDebug() << "Sensor is - ant_id:" << ant_id << "device_type:" << device_type << "name:" << name << "details:" << details;


        Sensor sensor(ant_id, sensorType, name, details );
        lstSensor.append(sensor);
    }

    return lstSensor;



}


///----------------------------------------------- JSON Sensor list -----------------------------------------------------
QList<Radio> Util::parseJsonRadioList(QString data) {



    qDebug() << "parseJsonRadioList*****";

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());

    QJsonArray jsonArray = jsonResponse.array();
    qDebug() << "SIZE OF THE ARRAY IS" << jsonArray.size();


    QList<Radio> lstRadio;
    //    (1, "NERadio Nonstop", "EuroDance", 1, 192, "IDK", "http://www4.no-ip.org:443/"),

    QString name;
    QString genre;
    bool gotAds;
    int bitrate;
    QString lang;
    QString url;



    /// Loop on sensors
    for (int i=0; i<jsonArray.size(); i++)
    {
        QJsonValue jsonValue = jsonArray.at(i);
        QJsonObject jsonObj = jsonValue.toObject();

        name = jsonObj["name"].toString();
        genre = jsonObj["genre"].toString();
        gotAds = jsonObj["gotAds"].toString().toInt();
        bitrate = jsonObj["bitrate"].toString().toInt();
        lang = jsonObj["lang"].toString();
        url = jsonObj["url"].toString();

        qDebug() << "Radio is - name:" << name << "genre:" << genre << "gotAds:" << gotAds << "bitrate:" << bitrate << "lang:" << lang << "url:" << url;


        Radio radio(name, genre, gotAds, bitrate, lang, url);
        lstRadio.append(radio);

    }

    return lstRadio;

}


///----------------------------------------------- JSON PARSING -----------------------------------------------------
// Parses the GitHub Releases API response and returns the latest release tag
// (e.g. "v0.0.26"). Returns an empty string if the response is invalid.
QString Util::parseJsonObjectVersion(const QString &data) {

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    if (jsonResponse.isNull() || !jsonResponse.isObject())
        return QString();

    return jsonResponse.object()["tag_name"].toString();  // e.g. "v0.0.26"
}

// Returns true if latestVersion is strictly newer than currentVersion.
// Versions may carry a leading "v" (e.g. "v0.0.26") which is stripped before
// the numeric comparison. Each component is compared left-to-right.
bool Util::isVersionNewer(const QString &currentVersion, const QString &latestVersion) {

    auto toNums = [](const QString &v) -> QVector<int> {
        QString s = v;
        s.remove(QRegularExpression("^[vV]"));
        const QStringList parts = s.split('.');
        QVector<int> nums;
        for (const QString &p : parts)
            nums << p.toInt();
        while (nums.size() < 3)
            nums << 0;
        return nums;
    };

    const QVector<int> cur    = toNums(currentVersion);
    const QVector<int> latest = toNums(latestVersion);
    const int len = qMin(cur.size(), latest.size());
    for (int i = 0; i < len; ++i) {
        if (latest[i] > cur[i]) return true;
        if (latest[i] < cur[i]) return false;
    }
    return false;
}

///--------------------------------------------------------------------------------------------------------------------
void Util::parseJsonStravaObject(QString data) {

    qDebug() << "start parseJsonStravaObject";

    Account *account = qApp->property("Account").value<Account*>();

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = jsonResponse.object();

    account->strava_access_token = jsonObj["access_token"].toString();

    qDebug() << "end parseJsonStravaObject";
}



///--------------------------------------------------------------------------------------------------------------------
void Util::parseJsonTPObject(QString data) {

    qDebug() << "parseJsonTPObject!";

    Account *account = qApp->property("Account").value<Account*>();

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = jsonResponse.object();

    account->training_peaks_access_token = jsonObj["access_token"].toString();
    account->training_peaks_refresh_token = jsonObj["refresh_token"].toString();

    qDebug() << "new values are: training_peaks_access_token:" << account->training_peaks_access_token;
    qDebug() << "new values are: training_peaks_refresh_token:" << account->training_peaks_refresh_token;
}

///--------------------------------------------------------------------------------------------------------------------
int Util::parseIdJsonStravaUploadObject(QString data) {


    qDebug() << "PARSE STRAVA DATA" << data;

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = jsonResponse.object();

    int myId = jsonObj["id"].toInt();
    return myId;
}


// -1 = Not normal, stop checking for status...
//  0 = Completed (Ready)
//  1 = Still In process
//  2 = Error
///--------------------------------------------------------------------------------------------------------------------
int Util::parseStravaUploadStatus(QString data) {


    qDebug() << "PARSE STRAVA DATA" << data;

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = jsonResponse.object();

    QString status = jsonObj["status"].toString();


    //    describes the error, possible values:
    //‘Your activity is still being processed.’, ‘The created activity has been deleted.’, ‘There was an error processing your activity.’, ‘Your activity is ready.’


    if (status.contains("ready", Qt::CaseInsensitive) ) {
        return 0;
    }
    else if (status.contains("processed", Qt::CaseInsensitive) ) {
        return 1;
    }
    else if (status.contains("error", Qt::CaseInsensitive) ) {
        return 2;
    }
    else {
        return -1;
    }
}








///----------------------------------------------- JSON PARSING -----------------------------------------------------
void Util::parseJsonObjectAccount(QString data) {


    Account *account = qApp->property("Account").value<Account*>();



    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    QJsonArray jsonArray = jsonResponse.array();


    if (jsonArray.size() < 1)
        return;

    QJsonValue jsonValue = jsonArray.at(0);
    QJsonObject jsonObj = jsonValue.toObject();

    //    QJsonObject jsonObj = jsonResponse.object();


    account->id = jsonObj["id"].toString().toInt();
    if (account->id < 1) {
        account->id = -1;
    }

    account->subscription_type_id = jsonObj["subscription_type_id"].toString().toInt();

    account->email =  jsonObj["email"].toString();
    account->password = jsonObj["password"].toString();
    account->session_mt_id = jsonObj["session_mt_id"].toString();
    QString session_mt_expire_str = jsonObj["session_mt_expire"].toString();
    account->session_mt_expire = QDateTime::currentDateTime().fromString(session_mt_expire_str, "yyyy-MM-dd hh:mm:ss");

    account->first_name =  jsonObj["first_name"].toString();
    account->last_name =  jsonObj["last_name"].toString();
    account->display_name =  jsonObj["display_name"].toString();

    account->FTP = jsonObj["FTP"].toString().toInt();
    account->LTHR = jsonObj["LTHR"].toString().toInt();
    account->minutes_rode = jsonObj["minutes_rode"].toString().toInt();
    account->weight_kg = jsonObj["weight_kg"].toString().toDouble();
    account->height_cm = jsonObj["height_cm"].toString().toInt();


    int trainer_curve_id = jsonObj["trainer_curve_id"].toString().toInt();
    PowerCurve curve;
    curve.setId(trainer_curve_id);
    curve.setRiderWeightKg(account->weight_kg);
    account->powerCurve = curve;
    account->wheel_circ = jsonObj["wheel_circ"].toString().toInt();
    account->bike_weight_kg = jsonObj["bike_weight_kg"].toString().toDouble();
    account->bike_type = jsonObj["bike_type"].toString().toInt();



    //--- Settings ------------------------------------
    account->nb_user_studio = jsonObj["nb_user_studio"].toString().toInt();
    account->enable_studio_mode = jsonObj["enable_studio_mode"].toString().toInt();
    account->use_pm_for_cadence = jsonObj["use_pm_for_cadence"].toString().toInt();
    account->use_pm_for_speed = jsonObj["use_pm_for_speed"].toString().toInt();

    account->force_workout_window_on_top = jsonObj["force_workout_window_on_top"].toString().toInt();
    account->show_included_workout = jsonObj["show_included_workout"].toString().toInt();
    account->show_included_course = jsonObj["show_included_course"].toString().toInt();
    account->distance_in_km = jsonObj["distance_in_km"].toString().toInt();
    account->strava_access_token = jsonObj["strava_access_token"].toString();
    account->strava_private_upload = jsonObj["strava_private_upload"].toString().toInt();

    account->training_peaks_access_token = jsonObj["training_peaks_access_token"].toString();
    account->training_peaks_refresh_token = jsonObj["training_peaks_refresh_token"].toString();
    account->training_peaks_public_upload = jsonObj["training_peaks_public_upload"].toString().toInt();

    account->selfloops_user = jsonObj["selfloops_user"].toString();
    account->selfloops_pw = jsonObj["selfloops_pw"].toString();
    account->control_trainer_resistance = jsonObj["control_trainer_resistance"].toString().toInt();
    account->stop_pairing_on_found = jsonObj["stop_pairing_on_found"].toString().toInt();
    account->nb_sec_pairing = jsonObj["nb_sec_pairing"].toString().toInt();
    /* ----- */


    account->last_index_selected_config_workout = jsonObj["last_index_selected_config_workout"].toString().toInt();
    account->last_tab_sub_config_selected =  jsonObj["last_tab_sub_config_selected"].toString().toInt();
    account->tab_display[0] = jsonObj["tab_display0"].toString();
    account->tab_display[1] = jsonObj["tab_display1"].toString();
    account->tab_display[2] = jsonObj["tab_display2"].toString();
    account->tab_display[3] = jsonObj["tab_display3"].toString();
    account->tab_display[4] = jsonObj["tab_display4"].toString();
    account->tab_display[5] = jsonObj["tab_display5"].toString();
    account->tab_display[6] = jsonObj["tab_display6"].toString();
    account->tab_display[7] = jsonObj["tab_display7"].toString();

    account->start_trigger  = jsonObj["start_trigger"].toString().toInt();
    account->value_cadence_start  = jsonObj["value_cadence_start"].toString().toInt();
    account->value_power_start  = jsonObj["value_power_start"].toString().toInt();
    account->value_speed_start  = jsonObj["value_speed_start"].toString().toInt();

    account->show_hr_widget  = jsonObj["show_hr_widget"].toString().toInt();
    account->show_power_widget  = jsonObj["show_power_widget"].toString().toInt();
    account->show_power_balance_widget  = jsonObj["show_power_balance_widget"].toString().toInt();
    account->show_cadence_widget  = jsonObj["show_cadence_widget"].toString().toInt();
    account->show_speed_widget  = jsonObj["show_speed_widget"].toString().toInt();
    account->show_calories_widget  = jsonObj["show_calories_widget"].toString().toInt();
    account->show_oxygen_widget  = jsonObj["show_oxygen_widget"].toString().toInt();
    account->use_virtual_speed  = jsonObj["use_virtual_speed"].toString().toInt();
    account->show_trainer_speed  = jsonObj["show_trainer_speed"].toString().toInt();


    account->display_hr  = jsonObj["display_hr"].toString().toInt();
    account->display_power  = jsonObj["display_power"].toString().toInt();
    account->display_power_balance  = jsonObj["display_power_balance"].toString().toInt();
    account->display_cadence  = jsonObj["display_cadence"].toString().toInt();

    account->show_timer_on_top  = jsonObj["show_timer_on_top"].toString().toInt();
    account->show_interval_remaining  = jsonObj["show_interval_remaining"].toString().toInt();
    account->show_workout_remaining  = jsonObj["show_workout_remaining"].toString().toInt();
    account->show_elapsed  = jsonObj["show_elapsed"].toString().toInt();
    account->font_size_timer  = jsonObj["font_size_timer"].toString().toInt();

    account->averaging_power  = jsonObj["averaging_power"].toString().toInt();
    account->offset_power  = jsonObj["offset_power"].toString().toInt();

    account->show_seperator_interval  = jsonObj["show_seperator_interval"].toString().toInt();
    account->show_grid  = jsonObj["show_grid"].toString().toInt();
    account->show_hr_target  = jsonObj["show_hr_target"].toString().toInt();
    account->show_power_target  = jsonObj["show_power_target"].toString().toInt();
    account->show_cadence_target  = jsonObj["show_cadence_target"].toString().toInt();
    account->show_speed_target  = jsonObj["show_speed_target"].toString().toInt();
    account->show_hr_curve  = jsonObj["show_hr_curve"].toString().toInt();
    account->show_power_curve  = jsonObj["show_power_curve"].toString().toInt();
    account->show_cadence_curve  = jsonObj["show_cadence_curve"].toString().toInt();
    account->show_speed_curve  = jsonObj["show_speed_curve"].toString().toInt();

    account->display_video  = jsonObj["display_video"].toString().toInt();


    /* ----- */
    account->sound_player_vol  = jsonObj["sound_player_vol"].toString().toInt();
    account->enable_sound  = jsonObj["enable_sound"].toString().toInt();
    account->sound_interval  = jsonObj["sound_interval"].toString().toInt();
    account->sound_pause_resume_workout  = jsonObj["sound_pause_resume_workout"].toString().toInt();
    account->sound_achievement  = jsonObj["sound_achievement"].toString().toInt();
    account->sound_end_workout  = jsonObj["sound_end_workout"].toString().toInt();

    account->sound_alert_power_under_target  = jsonObj["sound_alert_power_under_target"].toString().toInt();
    account->sound_alert_power_above_target  = jsonObj["sound_alert_power_above_target"].toString().toInt();
    account->sound_alert_cadence_under_target  = jsonObj["sound_alert_cadence_under_target"].toString().toInt();
    account->sound_alert_cadence_above_target  = jsonObj["sound_alert_cadence_above_target"].toString().toInt();



    //////////////////////////////////////////////////////////////////////
    //    qDebug() << "id:" << id;
    //    qDebug() << "is_expired_subscription:" << is_expired_subscription;

    //    qDebug() << "email:" << email;
    //    qDebug() << "password:" << password;
    //    qDebug() << "session_mt_id:" << session_mt_id;
    //    qDebug() << "session_mt_expire:" << session_mt_expire;

    //    qDebug() << "first_name:" << first_name;
    //    qDebug() << "last_name:" << last_name;
    //    qDebug() << "display_name:" << display_name;

    //    qDebug() << "**********WEIGHT IS " << weight_kg;
    //    qDebug() << "FTP:" << FTP;
    //    qDebug() << "LTHR:" << LTHR;

    //    qDebug() << "minutes_rode:" << minutes_rode;

    ////////////////////////////////////////////////////////////////////////////////////



    ///Stripped email
    int posArobas = account->email.indexOf("@");
    if (posArobas == -1) //should never happen
        posArobas = 5;
    QString tmpEmail = account->email.left(posArobas);
    account->email_clean = Util::cleanQString(tmpEmail);



    qDebug() << "Saving Account Object - done";


}


//--------------------------------------------------------------------------
QString Util::cleanQString(QString toClean) {

    QString toReturn = toClean;
    //        toReturn.remove(QRegularExpression(QString::fromUtf8("[-`~!@#$%^&*()_—+=|:;<>«»,.?/{}\'\"\\\[\\\]\\\\]")));
    toReturn.remove(QRegularExpression(QString::fromUtf8("[-`~!@#$%^&*()_—+=|:;<>«»,.?/{}\'\"\\[\\]\\\\]")));
    return toReturn;
}
//--------------------------------------------------------------------------
QString Util::cleanForOsSaving(QString toClean) {

    QString toReturn = toClean;
    toReturn.remove(QRegularExpression(QString::fromUtf8("[-`~!@#$%^&*()_—+=|:;<>«»,.?/{}\'\"\\[\\]\\\\]")));
    return toReturn;



}
//--------------------------------------------------------------------------
QTime Util::convertMinutesToQTime(double minutes) {

    QTime myTime(0, 0, 0);
    myTime = myTime.addMSecs(minutes * 60 * 1000);


    //    qDebug() << "MINute IS: " <<minutes << "Qtimeis:" << myTime;
    return myTime;

}



//static QStringList getListFiles(QString fileType); //.workout or //.course
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QStringList Util::getListFiles(QString fileType) {

    QStringList lstPath;
    QString pathToLook;

    bool checkCourseFiles = false;

    if (fileType == "workout") {
        pathToLook = getSystemPathWorkout() + QDir::separator();
    }
    else if (fileType == "course") {
        pathToLook = getSystemPathCourse() + QDir::separator();
        checkCourseFiles = true;
    }

    QDirIterator dirIt(pathToLook, QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
        dirIt.next();
        if (QFileInfo(dirIt.filePath()).isFile())
            if (checkCourseFiles) {
                if (QFileInfo(dirIt.filePath()).suffix() == "tcx") {
                    lstPath << dirIt.filePath();
                }
            }
            else {
                if (QFileInfo(dirIt.filePath()).suffix() == fileType) {
                    lstPath << dirIt.filePath();
                }
            }

    }

    return lstPath;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Util::openWorkoutFolder(QString workoutPath) {

    qDebug() << "openWorkoutFolder";
    if (workoutPath == "null")
    {
        QString path = getSystemPathWorkout() + QDir::separator();
        QDesktopServices::openUrl(QUrl("file:///" + path));
    }
    else {
        QFileInfo fileInfo(workoutPath);
        //        qDebug() << "ABSOULTE DIR IS:" << fileInfo.absolutePath();
        QDesktopServices::openUrl(QUrl("file:///" + fileInfo.absolutePath()));
    }


}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Util::openCourseFolder(QString coursePath) {

    qDebug() << "openCourseFolder";
    if (coursePath == "null")
    {
        QString path = getSystemPathCourse() + QDir::separator();
        QDesktopServices::openUrl(QUrl("file:///" + path));
    }
    else {
        QFileInfo fileInfo(coursePath);
        //        qDebug() << "ABSOULTE DIR IS:" << fileInfo.absolutePath();
        QDesktopServices::openUrl(QUrl("file:///" + fileInfo.absolutePath()));
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Util::openHistoryFolder() {

    QString path = getSystemPathHistory() + QDir::separator();
    QDesktopServices::openUrl(QUrl("file:///" + path));
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Util::checkFolderPathIsValidForWrite(QString path) {

    bool status = true;

    QDir dir(path);
    if (!dir.exists()) {
        status = dir.mkpath(".");
    }

    QFileInfo fi(path);
    if (fi.isDir() && fi.isWritable()) {
        qDebug() << "folder is writable";
    }
    else {
        status = false;
    }

    return status;
}


//------------------------------------------------------------------
QString Util::getMaximumTrainerDocumentPath() {

    QString writableLocation = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString appFolderMT = writableLocation + QDir::separator() + "MaximumTrainer";

    QDir dir(appFolderMT);
    bool status = Util::checkFolderPathIsValidForWrite(dir.absolutePath() );

    if (status)
        return dir.absolutePath();
    else {
        return "invalid_writable_path";
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QString Util::getSystemPathHistory() {

    QString folderName = "history";
    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    /// Return default historyFolder
    if (settings->historyFolder == "") {
        return Util::getSystemPathHelperReturnDefaultLoc(folderName);
    }
    /// Return custom path, check if still valid
    else {
        if (checkFolderPathIsValidForWrite(settings->historyFolder) )
            return settings->historyFolder;
        else
            return Util::getSystemPathHelperReturnDefaultLoc(folderName);
    }
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QString Util::getSystemPathWorkout() {

    QString folderName = "workout";
    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    /// Return default workoutFolder
    if (settings->workoutFolder == "") {
        return Util::getSystemPathHelperReturnDefaultLoc(folderName);
    }
    /// Return custom path, check if still valid
    else {
        if (checkFolderPathIsValidForWrite(settings->workoutFolder) )
            return settings->workoutFolder;
        else
            return Util::getSystemPathHelperReturnDefaultLoc(folderName);
    }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QString Util::getSystemPathCourse() {

    QString folderName = "course";
    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    // Return default courseFolder
    if (settings->courseFolder == "") {
        return Util::getSystemPathHelperReturnDefaultLoc(folderName);
    }
    // Return custom path, check if still valid
    else {
        if (checkFolderPathIsValidForWrite(settings->courseFolder) )
            return settings->courseFolder;
        else
            return Util::getSystemPathHelperReturnDefaultLoc(folderName);
    }

}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QString Util::getSystemPathHelperReturnDefaultLoc(QString docType) {

    QString folderName;

    if (docType == "workout") {
        folderName = "Workouts";
    }
    else if (docType == "history") {
        folderName = "History";
    }
    else {
        folderName = "Courses";
    }

    ///---
    QString writableLocation = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString appFolderWorkout = writableLocation + QDir::separator() + "MaximumTrainer" + QDir::separator() + folderName;

    QDir dir(appFolderWorkout);
    bool status = Util::checkFolderPathIsValidForWrite(dir.absolutePath() );

    if (status)
        return dir.absolutePath();
    else {
        return "invalid_writable_path";
    }
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool Util::checkFileNameAlreadyExist(QString pathFile) {

    QFile file(pathFile);
    if(file.exists())
        return true;
    return false;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Util::deleteLocalFile(QString filePath) {


    /*
    QString writableLocation = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString fileToDelete = writableLocation + QDir::separator() + "MaximumTrainer" + QDir::separator() + "Workouts" + QDir::separator() + fileName + ".workout";
    qDebug() << "file to delete is:" << fileToDelete;
    */

    //    QString fileToDelete = Util::getSystemPathWorkout() + QDir::separator() + fileName + ".workout";

    qDebug() << "should delete this:" << filePath;


    QFile file(filePath);
    file.remove();

    qDebug() << "delete done";
}







quint32 Util::updateCRC32(unsigned char ch, quint32 crc)
{
    return (constants::crc_32_tab[((crc) ^ ((quint8)ch)) & 0xff] ^ ((crc) >> 8));
}

quint32 Util::crc32buf(const QByteArray& data)
{
    return ~std::accumulate(
                data.begin(),
                data.end(),
                quint32(0xFFFFFFFF),
                [](quint32 oldcrc32, char buf){ return updateCRC32(buf, oldcrc32); });
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QByteArray Util::zipFileHelperConvertToGzip(const QByteArray& data) {

    //  Strip the first six bytes (a 4-byte length put on by qCompress and a 2-byte zlib header)
    // and the last four bytes (a zlib integrity check).
    auto compressedData = qCompress(data);
    compressedData.remove(0, 6);
    compressedData.chop(4);

    QByteArray header;
    QDataStream ds1(&header, QIODevice::WriteOnly);
    // Prepend a generic 10-byte gzip header (see RFC 1952),
    ds1 << quint16(0x1f8b)
        << quint16(0x0800)
        << quint16(0x0000)
        << quint16(0x0000)
        << quint16(0x000b);

    // Append a four-byte CRC-32 of the uncompressed data
    // Append 4 bytes uncompressed input size modulo 2^32
    QByteArray footer;
    QDataStream ds2(&footer, QIODevice::WriteOnly);
    ds2.setByteOrder(QDataStream::LittleEndian);
    ds2 << crc32buf(data)
        << quint32(data.size());

    return header + compressedData + footer;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Util::zipFileToDisk(QString filename, QString zipFilename, bool useGzip) {

    QFile infile(filename);
    QFile outfile(zipFilename);
    infile.open(QIODevice::ReadOnly);
    outfile.open(QIODevice::WriteOnly);
    QByteArray uncompressedData = infile.readAll();
    QByteArray compressedData;

    if (useGzip) {
        compressedData = zipFileHelperConvertToGzip(uncompressedData);
    }
    else {
        compressedData = qCompress(uncompressedData);
    }

    outfile.write(compressedData);
    infile.close();
    outfile.close();
}


void Util::unzipFile(QString zipFilename , QString filename) {

    QFile infile(zipFilename);
    QFile outfile(filename);
    infile.open(QIODevice::ReadOnly);
    outfile.open(QIODevice::WriteOnly);
    QByteArray uncompressedData = infile.readAll();
    QByteArray compressedData = qUncompress(uncompressedData);
    outfile.write(compressedData);
    infile.close();
    outfile.close();
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QColor Util::getColor(Color color) {


    /// Power
    QColor colorPowerShapeTarget(46, 150, 87, 20);
    QColor linePower(248,228,7);


    /// Cadence
    //    QColor colorCadenceShapeTarget(216, 222, 255, 20);
    QColor colorCadenceShapeTarget(178,183,210);
    //    QColor colorCadenceShapeTarget(126, 149, 196);
    //    QColor lineCadence("RoyalBlue");
    //    QColor lineCadence(20,104,244);
    QColor lineCadence(0,0,255);

    /// HR
    //    QColor colorHRShapeTarget("gray");
    QColor colorHRShapeTarget(255, 191, 191, 20);
    QColor lineHR("red");

    /// Speed
    QColor lineSpeed(170, 170, 255);

    //    QColor greenMaximumTrainer(112,161,0);

    QColor blue_too_low(14, 61, 170);
    QColor brownColor(128,0,0);
    QColor black_on_target(35, 35, 35);
    QColor color_done(35,35,35);
    QColor color_notDone(65,65,65);

    QColor colorBalancePowerText(254,153,0);
    QColor onTargetGraphLine = Qt::white;


    if (color == Util::SQUARE_POWER ) {
        return colorPowerShapeTarget;
    }
    else if( color == Util::LINE_POWER) {
        return linePower;
    }
    else if (color == Util::SQUARE_CADENCE) {
        return colorCadenceShapeTarget;
    }
    else if (color == Util::LINE_CADENCE) {
        return lineCadence;
    }
    else if (color == Util::SQUARE_HEARTRATE) {
        return colorHRShapeTarget;
    }
    else if (color == Util::LINE_HEARTRATE) {
        return lineHR;
    }
    else if (color == Util::LINE_SPEED) {
        return lineSpeed;
    }
    else if (color == Util::TOO_LOW ) {
        return blue_too_low;
    }
    else if (color == Util::TOO_HIGH) {
        return brownColor;
    }
    else if (color == Util::ON_TARGET) {
        return black_on_target;
    }
    else if (color == Util::NOT_DONE ) {
        return color_notDone;
    }
    else if (color == Util::DONE) {
        return color_done;
    }
    else if (color == Util::BALANCE_POWER_TXT) {
        return colorBalancePowerText;
    }
    else if (color == Util::LINE_ON_TARGET_GRAPH) {
        return onTargetGraphLine;
    }

    return brownColor;
}





/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//QTime Util::convertMinutesToQTime(double minutes) {

//    int hours, newMinutes, secs = 0;

//    double rest = minutes - ((int)minutes);
//    secs = rest*60;
//    hours = minutes / 60;
//    newMinutes = minutes - (hours*60);
//    //    qDebug() << "hrs :" << hours;
//    //    qDebug() << "min :" << newMinutes;
//    //    qDebug() << "secs :" << secs;
//    return ( QTime(hours, newMinutes, secs, 0) );

//}



///////////////////////////////////////////////////////////////////////////////////////
QString Util::showCurrentTimeAsString(const QTime &time) {


    QString text = "";
    QString toAdd = "";


    QLocale locale;
    if (locale.timeFormat().contains("AP"))
        toAdd = "AP";


    text = time.toString("h:mm " + toAdd);



    return text;
}




////////////////////////////////////////////////////////////////////////////////////////////
QString Util::showQTimeAsString(const QTime &time) {


    if (time.hour()>=1) {
        return time.toString("h:mm:ss");
    }
    else {
        return time.toString("mm:ss");
    }
}

//-----------------------------------------------------
QString Util::showQTimeAsStringWithMs(const QTime &time) {

    if (time.hour()>=1) {
        return time.toString("h:mm:ss:z");
    }
    else {
        return time.toString("mm:ss:z");
    }
}




//--------------------------------------------------------------------------
double Util::convertQTimeToSecD(const QTime &time) {

    QTime time0(0,0,0,0);

    return (time0.msecsTo(time)/1000.0 );
}




//--------------------------------------------------------------------------
//double Util::convertQTimeToMinutes(const QTime &timeElasped) {

//    int hours = timeElasped.hour();
//    int minutes = timeElasped.minute();
//    double secs = timeElasped.second();

//    double totalMinute=0;
//    if (hours>0)
//        totalMinute += hours*60;
//    if (secs>0)
//        totalMinute += secs/60.0;
//    totalMinute+= minutes;

//    return totalMinute;
//}


/////////////////////////////////////////////////////////////////////////////////////
QString Util::getStringFromUCHAR(unsigned char* ch) {

    QString temp;

    int len = strlen((char*)ch);

    for (int i=0; i<len; i++) {
        char s = ch[i];
        QChar p(s);
        temp.append(p);
    }


    return temp;
}




// ───────────────────────────────────────────────────────────────────────────────
// Parse GET /api/v1/athlete/{id}
// Updates account->first_name, last_name, display_name, weight_kg, FTP, LTHR.
void Util::parseJsonIntervalsIcuAthlete(const QString &data)
{
    Account *account = qApp->property("Account").value<Account*>();

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "parseJsonIntervalsIcuAthlete: invalid JSON";
        return;
    }

    QJsonObject obj = doc.object();

    // Athlete ID — update the stored ID when the response includes it.
    // This is important for the OAuth flow where we query with id="0" (current user)
    // and need to record the real athlete ID returned by the server.
    const QString athleteId = obj.value(QStringLiteral("id")).toString();
    if (!athleteId.isEmpty())
        account->intervals_icu_athlete_id = athleteId;

    // Name fields — Intervals.icu may return separate firstname/lastname or a
    // combined name field depending on the API version; handle both.
    const QString firstname = obj.value(QStringLiteral("firstname")).toString(
                              obj.value(QStringLiteral("name")).toString());
    const QString lastname  = obj.value(QStringLiteral("lastname")).toString();

    if (!firstname.isEmpty())
        account->first_name = firstname;
    if (!lastname.isEmpty())
        account->last_name = lastname;

    // Rebuild display_name from the retrieved parts when available.
    const QString displayName = obj.value(QStringLiteral("name")).toString();
    if (!displayName.isEmpty())
        account->display_name = displayName;
    else if (!account->first_name.isEmpty())
        account->display_name = account->first_name + (account->last_name.isEmpty()
                                ? QString()
                                : QStringLiteral(" ") + account->last_name);

    // Weight — Intervals.icu stores in kilograms.
    const double weight = obj.value(QStringLiteral("weight")).toDouble(-1.0);
    if (weight > 0.0)
        account->weight_kg = weight;

    // FTP — stored as integer watts.
    const int ftp = obj.value(QStringLiteral("ftp")).toInt(-1);
    if (ftp > 0)
        account->FTP = ftp;

    // LTHR (Lactate Threshold Heart Rate).
    const int lthr = obj.value(QStringLiteral("lthr")).toInt(-1);
    if (lthr > 0)
        account->LTHR = lthr;
}


// ───────────────────────────────────────────────────────────────────────────────
// Parse GET /api/v1/athlete/{id}/settings
// Updates account->hr_zones and account->power_zones with the upper-bound of
// each zone, extracted from the Intervals.icu settings JSON.
void Util::parseJsonIntervalsIcuSettings(const QString &data)
{
    Account *account = qApp->property("Account").value<Account*>();

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "parseJsonIntervalsIcuSettings: invalid JSON";
        return;
    }

    QJsonObject obj = doc.object();

    // HR zones — array of zone objects each containing a "max" bound.
    const QJsonValue hrZonesVal = obj.value(QStringLiteral("hrZones"));
    if (hrZonesVal.isArray()) {
        QList<int> zones;
        for (const QJsonValue &zv : hrZonesVal.toArray()) {
            const QJsonObject z = zv.toObject();
            const int upper = z.value(QStringLiteral("max")).toInt(
                              z.value(QStringLiteral("to")).toInt(0));
            if (upper > 0)
                zones.append(upper);
        }
        if (!zones.isEmpty())
            account->hr_zones = zones;
    }

    // Power zones — same structure.
    const QJsonValue pwrZonesVal = obj.value(QStringLiteral("powerZones"));
    if (pwrZonesVal.isArray()) {
        QList<int> zones;
        for (const QJsonValue &zv : pwrZonesVal.toArray()) {
            const QJsonObject z = zv.toObject();
            const int upper = z.value(QStringLiteral("max")).toInt(
                              z.value(QStringLiteral("to")).toInt(0));
            if (upper > 0)
                zones.append(upper);
        }
        if (!zones.isEmpty())
            account->power_zones = zones;
    }
}




// ───────────────────────────────────────────────────────────────────────────────
// Parse the Intervals.icu OAuth2 token endpoint response.
// Expected JSON:
//   { "access_token": "…", "refresh_token": "…", "athlete_id": "i12345",
//     "expires_in": 3600, "token_type": "Bearer" }
// Stores access_token, refresh_token, and athlete_id into the global Account.
void Util::parseJsonIntervalsIcuOAuthToken(const QString &data)
{
    Account *account = qApp->property("Account").value<Account*>();
    if (!account) {
        qWarning() << "parseJsonIntervalsIcuOAuthToken: Account not found";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "parseJsonIntervalsIcuOAuthToken: invalid JSON received from token endpoint";
        return;
    }

    QJsonObject obj = doc.object();

    const QString accessToken  = obj.value(QStringLiteral("access_token")).toString();
    const QString refreshToken = obj.value(QStringLiteral("refresh_token")).toString();
    // Intervals.icu returns the athlete ID as "athlete_id" in the token response.
    const QString athleteId    = obj.value(QStringLiteral("athlete_id")).toString();

    if (!accessToken.isEmpty())
        account->intervals_icu_access_token = accessToken;

    if (!refreshToken.isEmpty())
        account->intervals_icu_refresh_token = refreshToken;

    if (!athleteId.isEmpty())
        account->intervals_icu_athlete_id = athleteId;

    qDebug() << "parseJsonIntervalsIcuOAuthToken: athlete_id =" << account->intervals_icu_athlete_id;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Local radio list — stored as JSON under the MaximumTrainer document root.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QString Util::getLocalRadioListPath() {

    const QString base = Util::getMaximumTrainerDocumentPath();
    if (base == "invalid_writable_path")
        return QString();
    return base + QDir::separator() + "radios.json";
}


QList<Radio> Util::getDefaultRadioList() {

    QFile bundled(QStringLiteral(":/data/resources/data/default_radios.json"));
    if (!bundled.open(QIODevice::ReadOnly)) {
        qWarning() << "getDefaultRadioList: cannot open bundled resource:"
                   << bundled.errorString();
        return QList<Radio>();
    }
    return Util::parseJsonRadioList(QString::fromUtf8(bundled.readAll()));
}


bool Util::saveLocalRadioList(const QList<Radio>& lstRadio) {

    const QString path = getLocalRadioListPath();
    if (path.isEmpty()) {
        qWarning() << "saveLocalRadioList: writable path unavailable";
        return false;
    }

    QJsonArray arr;
    for (const Radio& r : lstRadio) {
        QJsonObject obj;
        obj["name"]    = r.getName();
        obj["genre"]   = r.getGenre();
        // Match the on-the-wire schema parseJsonRadioList expects: gotAds and
        // bitrate were strings (the legacy server returned them as such).
        obj["gotAds"]  = QString::number(r.getGotAds() ? 1 : 0);
        obj["bitrate"] = QString::number(r.getBitrate());
        obj["lang"]    = r.getLanguage();
        obj["url"]     = r.getUrl();
        arr.append(obj);
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "saveLocalRadioList: cannot open" << path << ":" << file.errorString();
        return false;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return true;
}


QList<Radio> Util::loadLocalRadioList() {

    const QString path = getLocalRadioListPath();

    /// First-run / corrupt-path: drop the bundled defaults to disk and use
    /// them. saveLocalRadioList may itself fail (read-only home, etc.) — in
    /// that case we still return the defaults so the in-memory list is
    /// usable for the session.
    auto fallbackToDefaults = [&]() {
        QList<Radio> defaults = getDefaultRadioList();
        if (!path.isEmpty())
            saveLocalRadioList(defaults);
        return defaults;
    };

    if (path.isEmpty())
        return fallbackToDefaults();

    QFile file(path);
    if (!file.exists())
        return fallbackToDefaults();

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "loadLocalRadioList: cannot open" << path << ":" << file.errorString();
        return fallbackToDefaults();
    }

    /// Reuse parseJsonRadioList — same schema as the legacy server.
    /// On parse failure we keep the user's file untouched (so they can fix
    /// it manually) and fall back to defaults for this session only.
    QList<Radio> parsed = Util::parseJsonRadioList(QString::fromUtf8(file.readAll()));
    if (parsed.isEmpty()) {
        qWarning() << "loadLocalRadioList: parsed 0 radios from" << path
                   << "— using bundled defaults for this session";
        return getDefaultRadioList();
    }
    return parsed;
}
