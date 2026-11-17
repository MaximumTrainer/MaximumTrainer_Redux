#include "importerworkout.h"
#include "util.h"
#include "interval.h"
#include "xmlutil.h"
#include <QRegularExpression>

ImporterWorkout::~ImporterWorkout()
{
}

ImporterWorkout::ImporterWorkout(QObject *parent) : QObject(parent)
{
}


bool ImporterWorkout::batchImportWorkoutFromFolder(QString folderName, int userFTP) {


    // Find all filename to convert
    QStringList lstWorkoutPaths;
    QDirIterator dirIt(folderName, QDirIterator::Subdirectories);
    while (dirIt.hasNext()) {
        dirIt.next();
        if (QFileInfo(dirIt.filePath()).isFile())
            if (QFileInfo(dirIt.filePath()).suffix() == "erg" || QFileInfo(dirIt.filePath()).suffix() == "mrc")
                lstWorkoutPaths << dirIt.filePath();
    }

    if (lstWorkoutPaths.size() == 0)
        return false;

    // Create folder to put converted workout into
    QString outputPath = folderName + "/MT Workouts/";
    QDir().mkdir(outputPath); //assume it worked, folderName is valid


    //2 loop on all file (.erg and .mrc) and save them to .workout format
    for (const QString &file : std::as_const(lstWorkoutPaths))
    {
        Workout workout = ImporterWorkout::importWorkoutFromFile(file, userFTP);
        XmlUtil::createWorkoutXml(workout, outputPath + workout.getName() + ".workout");

    }


    return true;


}

Workout ImporterWorkout::importWorkoutFromFile(QString filename, int userFTP) {

    //Support .erg and .mrc workout extension for now


    Workout workout;

    QFileInfo fi(filename);
    QString ext = fi.completeSuffix();
    if  (ext.contains("mrc", Qt::CaseInsensitive) || ext.contains("erg", Qt::CaseInsensitive))
        workout = parseMrcErgFile(filename, userFTP);
    else
        qWarning() << "Workout import: unsupported format" << filename;


    return workout;
}


Workout ImporterWorkout::parseMrcErgFile(QString filename, int userFTP) {


    //Parse name
    int posLastSlash = filename.lastIndexOf("/");
    int posDot = filename.lastIndexOf(".");
    QString name = filename.mid(posLastSlash+1, posDot-posLastSlash-1);


    QString description = "Imported description";
    QString author = "-";
    QString plan = "-";

    double ftp = userFTP; //use as default value if not set in file

    QList<Interval> lstInterval;


    bool foundDescription = false;
    bool foundFTP = false;
    // To identify if target is in Watts or percentage
    bool foundDescriptorMinutes_X = false;
    bool targetInPerc = false;

    bool parsingCourseDataNow = false;
    bool parsingOver = false;

    static const QRegularExpression whitespaceRegex("\\s");


    //Interval parsing
    bool firstDataPointInterval = true;
    double timeMinStart = 0.0;
    double timeMinEnd = 0.0;
    double ftpPercStart = 0.0;
    double ftpPercEnd = 0.0;


    // Read file, line by line or readAll and parse QString?
    QFile inputFile(filename);
    if (inputFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&inputFile);
        while ( !in.atEnd() && !parsingOver )
        {
            QString line = in.readLine();

            // FTP
            if (!foundFTP && line.contains("FTP", Qt::CaseInsensitive) && line.contains("=", Qt::CaseInsensitive)) {
                QStringList pieces = line.split( "=" );
                double value = pieces.at(pieces.length()-1).toDouble();
                if (value > 0) {
                    ftp = pieces.at(pieces.length()-1).toDouble();
                    foundFTP = true;
                }
            }
            // Description
            else if (!foundDescription && line.contains("DESCRIPTION", Qt::CaseInsensitive)) {
                QStringList pieces = line.split( "=" );
                description = pieces.at(pieces.length()-1);
                foundDescription = true;
            }
            // foundDescriptorMinutes_X
            else if (!foundDescriptorMinutes_X && line.contains("MINUTES", Qt::CaseInsensitive)) {
                if (line.contains("WATTS", Qt::CaseInsensitive)) {
                    targetInPerc = false;
                }
                else if (line.contains("PERCENTAGE", Qt::CaseInsensitive) || line.contains("PERCENT", Qt::CaseInsensitive)) {
                    targetInPerc = true;
                }
                foundDescriptorMinutes_X = true;
            }
            else if (parsingCourseDataNow) {
                if (line.contains("END COURSE DATA", Qt::CaseInsensitive)) {
                    parsingOver = true;
                    break;
                }
                else {
                    QStringList dataLine = line.split(whitespaceRegex);
                    if (dataLine.size() >= 2) { //last line is empty, do not try to parse

                        //double timeMinute = dataLine[0].toDouble();
                        //double percentFTP = dataLine[2].toDouble();

                        if (firstDataPointInterval) {
                            if (dataLine.size() == 2) {
                                timeMinStart = dataLine[0].toDouble();
                                ftpPercStart = dataLine[1].toDouble();
                            }
                            if (dataLine.size() == 3) {
                                timeMinStart = dataLine[0].toDouble();
                                ftpPercStart = dataLine[2].toDouble();
                            }
                            firstDataPointInterval = false;
                        }
                        else {
                            if (dataLine.size() == 2) {
                                timeMinEnd = dataLine[0].toDouble();
                                ftpPercEnd = dataLine[1].toDouble();
                            }
                            if (dataLine.size() == 3) {
                                timeMinEnd = dataLine[0].toDouble();
                                ftpPercEnd = dataLine[2].toDouble();
                            }

                            // Calculate interval fields
                            double durationMin = timeMinEnd - timeMinStart;
                            if (durationMin > 0) {
                                QTime durationQTime = Util::convertMinutesToQTime(durationMin);
                                Interval::StepType powerStep;
                                ftpPercEnd == ftpPercStart ?  powerStep=Interval::FLAT : powerStep=Interval::PROGRESSIVE;
                                //convert to Percentage if using watts
                                if (!targetInPerc) {
                                    ftpPercStart = ftpPercStart/ftp *100;
                                    ftpPercEnd = ftpPercEnd/ftp *100;
                                }
                                Interval interval(durationQTime, "",
                                                  powerStep, ftpPercStart/100, ftpPercEnd/100, 20, -1,
                                                  Interval::NONE,0, 0, 5,
                                                  Interval::NONE,0, 0, 15,
                                                  false, 0, 0, 0);
                                lstInterval.append(interval);
                            }
                            firstDataPointInterval = true;
                        }
                    }
                }
            }
            else if (line.contains("COURSE DATA", Qt::CaseInsensitive)) {
                //Stop checking for workout descriptor.. too late here
                foundDescription = true;
                foundFTP = true;
                foundDescriptorMinutes_X = true;
                parsingCourseDataNow = true;
            }

        }
        inputFile.close();
    }


    Workout workout("", Workout::USER_MADE, lstInterval,
                    name, author, description, plan, Workout::T_ENDURANCE);
    return workout;

}

