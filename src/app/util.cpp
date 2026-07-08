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
#include <QCryptographicHash>

#include "account.h"
#include "settings.h"
#include "xmlutil.h"
#include "myconstants.h"
#include <numeric>




Util::Util() {
}



///----------------------------------------------- JSON Radio list -----------------------------------------------------
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

    Account *account = qApp->property("Account").value<Account*>();

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = jsonResponse.object();

    account->strava_access_token = jsonObj["access_token"].toString();
    // Refresh-token rotation: persist whatever comes back. expires_at is epoch
    // seconds; refresh before that time on the next upload.
    if (jsonObj.contains("refresh_token"))
        account->strava_refresh_token = jsonObj["refresh_token"].toString();
    if (jsonObj.contains("expires_at"))
        account->strava_token_expires_at =
            static_cast<qint64>(jsonObj["expires_at"].toDouble());
}



///--------------------------------------------------------------------------------------------------------------------
qint64 Util::parseIdJsonStravaUploadObject(QString data) {

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = jsonResponse.object();

    // Strava upload IDs exceed 32-bit int range — read as 64-bit, not toInt().
    return jsonObj["id"].toVariant().toLongLong();
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


// The Strava upload-status response carries the created activity's id (a 64-bit
// value, null until processing completes). Returns 0 when absent.
///--------------------------------------------------------------------------------------------------------------------
qint64 Util::parseStravaActivityId(QString data) {

    QJsonDocument jsonResponse = QJsonDocument::fromJson(data.toUtf8());
    QJsonObject jsonObj = jsonResponse.object();

    return jsonObj["activity_id"].toVariant().toLongLong();
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



//static QStringList getListFiles(QString fileType); //.workout
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QStringList Util::getListFiles(QString fileType) {

    QStringList lstPath;
    QString pathToLook;

    if (fileType == "workout") {
        pathToLook = getSystemPathWorkout() + QDir::separator();
    }

    QDirIterator dirIt(pathToLook, QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
        dirIt.next();
        if (QFileInfo(dirIt.filePath()).isFile())
            if (QFileInfo(dirIt.filePath()).suffix() == fileType) {
                lstPath << dirIt.filePath();
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
    return Util::getSystemPathHelperReturnDefaultLoc("history");
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QString Util::getSystemPathWorkout() {
    return Util::getSystemPathHelperReturnDefaultLoc("workout");
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QString Util::getSystemPathHelperReturnDefaultLoc(QString docType) {

    QString folderName;

    if (docType == "workout") {
        folderName = "Workouts";
    }
    else {
        folderName = "History";
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
bool Util::zipFileToDisk(QString filename, QString zipFilename, bool useGzip) {

    QFile infile(filename);
    QFile outfile(zipFilename);
    if (!infile.open(QIODevice::ReadOnly)) {
        qWarning() << "zipFileToDisk: cannot read" << filename << infile.errorString();
        return false;
    }
    if (!outfile.open(QIODevice::WriteOnly)) {
        qWarning() << "zipFileToDisk: cannot write" << zipFilename << outfile.errorString();
        return false;
    }
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
    return true;
}


bool Util::unzipFile(QString zipFilename , QString filename) {

    QFile infile(zipFilename);
    QFile outfile(filename);
    if (!infile.open(QIODevice::ReadOnly)) {
        qWarning() << "unzipFile: cannot read" << zipFilename << infile.errorString();
        return false;
    }
    if (!outfile.open(QIODevice::WriteOnly)) {
        qWarning() << "unzipFile: cannot write" << filename << outfile.errorString();
        return false;
    }
    QByteArray uncompressedData = infile.readAll();
    QByteArray compressedData = qUncompress(uncompressedData);
    outfile.write(compressedData);
    infile.close();
    outfile.close();
    return true;
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QPixmap Util::loadIconForDpr(const QString &resourcePath, int logicalSize, qreal devicePixelRatio)
{
    if (devicePixelRatio <= 0.0)
        devicePixelRatio = 1.0;
    QPixmap src(resourcePath);
    if (src.isNull())
        return src;
    const int device = qRound(logicalSize * devicePixelRatio);
    QPixmap scaled = src.scaled(device, device, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(devicePixelRatio);
    return scaled;
}

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
// Parse GET /api/v1/athlete/{id}/sport-settings — an ARRAY of per-sport
// SportSettings objects ({types:["Ride",…], ftp, indoor_ftp, lthr,
// power_zones (% of FTP), hr_zones (bpm), …}).
//
// Applies the cycling entry (types containing "Ride", falling back to
// "VirtualRide", then the first entry) to the account: FTP (indoor_ftp
// preferred — this is an indoor trainer), LTHR, and the absolute zone bounds.
// Returns true when an FTP or LTHR value was applied, so the caller can
// persist the profile and record the sync.
bool Util::parseJsonIntervalsIcuSettings(const QString &data)
{
    Account *account = qApp->property("Account").value<Account*>();
    if (!account)
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (doc.isNull() || !doc.isArray()) {
        qWarning() << "parseJsonIntervalsIcuSettings: expected a JSON array of sport settings";
        return false;
    }

    const QJsonArray sports = doc.array();
    if (sports.isEmpty())
        return false;

    auto findByType = [&sports](const QString &type) -> QJsonObject {
        for (const QJsonValue &sv : sports) {
            const QJsonObject s = sv.toObject();
            for (const QJsonValue &tv : s.value(QStringLiteral("types")).toArray())
                if (tv.toString() == type)
                    return s;
        }
        return QJsonObject();
    };

    QJsonObject ride = findByType(QStringLiteral("Ride"));
    if (ride.isEmpty())
        ride = findByType(QStringLiteral("VirtualRide"));
    if (ride.isEmpty())
        ride = sports.first().toObject();

    bool applied = false;

    const int outdoorFtp = ride.value(QStringLiteral("ftp")).toInt(0);
    const int indoorFtp  = ride.value(QStringLiteral("indoor_ftp")).toInt(0);
    const int ftp        = indoorFtp > 0 ? indoorFtp : outdoorFtp;
    if (ftp > 0) {
        account->FTP = ftp;
        applied = true;
    }

    const int lthr = ride.value(QStringLiteral("lthr")).toInt(0);
    if (lthr > 0) {
        account->LTHR = lthr;
        applied = true;
    }

    // hr_zones are absolute bpm upper bounds.
    const QJsonValue hrZonesVal = ride.value(QStringLiteral("hr_zones"));
    if (hrZonesVal.isArray()) {
        QList<int> zones;
        for (const QJsonValue &zv : hrZonesVal.toArray())
            if (zv.toInt(0) > 0)
                zones.append(zv.toInt());
        if (!zones.isEmpty())
            account->hr_zones = zones;
    }

    // power_zones are PERCENT of FTP — convert to absolute watts.
    const QJsonValue pwrZonesVal = ride.value(QStringLiteral("power_zones"));
    if (pwrZonesVal.isArray() && ftp > 0) {
        QList<int> zones;
        for (const QJsonValue &zv : pwrZonesVal.toArray())
            if (zv.toInt(0) > 0)
                zones.append(qRound(zv.toInt() * ftp / 100.0));
        if (!zones.isEmpty())
            account->power_zones = zones;
    }

    return applied;
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

    // Intervals.icu's token response carries only the access token: there are
    // no refresh tokens, and the athlete id comes from the follow-up
    // /athlete/0 profile fetch, not from this payload.
    const QString accessToken = obj.value(QStringLiteral("access_token")).toString();

    if (!accessToken.isEmpty())
        account->intervals_icu_access_token = accessToken;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Extract the "error" field from a JSON error payload returned by the
// intervals-cors-proxy Cloudflare Worker.  Returns an empty QString when the
// body is empty, non-JSON, non-object, or has no "error" key — never throws.
QString Util::parseJsonIntervalsIcuOAuthErrorPayload(const QString &data)
{
    if (data.isEmpty())
        return QString();
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (doc.isNull() || !doc.isObject())
        return QString();
    return doc.object().value(QStringLiteral("error")).toString();
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

    // Whenever the bundled default list changes (any release that edits
    // default_radios.json), the user's radios.json is REPLACED wholesale with
    // the new defaults — nothing from the old file is kept; users re-add
    // their custom stations.  Keyed on a content hash of the bundled JSON so
    // no version constant has to be bumped by hand.
    {
        QFile bundled(QStringLiteral(":/data/resources/data/default_radios.json"));
        if (bundled.open(QIODevice::ReadOnly)) {
            const QString bundledHash = QString::fromLatin1(
                QCryptographicHash::hash(bundled.readAll(),
                                         QCryptographicHash::Sha256).toHex());
            QSettings settings;
            const QString seenHash =
                settings.value(QStringLiteral("radios/defaultsHash")).toString();
            if (seenHash != bundledHash) {
                settings.setValue(QStringLiteral("radios/defaultsHash"), bundledHash);
                return fallbackToDefaults();
            }
        }
    }

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
