#include "xmlutil.h"

#include "account.h"
#include "util.h"
#include "environnement.h"
#include "simplecrypt.h"



// Key used to obfuscate the Intervals.icu API key at rest in the .save file.
// This is the same key used for the remembered-password feature in DialogLogin,
// giving consistent protection across locally-stored credentials.
static const quint64 INTERVALS_ICU_CRYPT_KEY = Q_UINT64_C(0xdd85116f2b81d85f);



//http://qt-project.org/doc/qt-5.0/qtcore/qxmlstreamreader.html#details
XmlUtil::XmlUtil(QObject *parent) :QObject(parent) {
}








///////////////////////////////////////////////////////////////////////////////////////////////////
void XmlUtil::parseWorkoutDone(Account *account, QXmlStreamReader& xml) {

    qDebug() << "parseWorkoutDone";

    while (true) {

        if (xml.hasError()) {
            qDebug() << "Error in XML, parseWorkoutDone method" << xml.error();
            return;
        }
        xml.readNextStartElement();
        qDebug() << "name now:" << xml.name();

        //stop condition
        if (xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QLatin1String("WorkoutDone"))
            return;

        ///-------------------------------------------------------------------------------
        if(xml.name() == QLatin1String("Workout")) {
            QString workoutName  = xml.readElementText();
            qDebug() << "workoutName XML is:" << workoutName;
            if (workoutName.size() > 0)
                account->hashWorkoutDone.insert(workoutName);
        }
    }
}


///Read xml file (.save file), construct Settings Object and Account QSet from it
////////////////////////////////////////////////////////////////////////////////////////////////
void XmlUtil::parseLocalSaveFile(Account *account) {

    qDebug() << "\n\n Parsing Local Save file!";

    //Load Xml file
    QString nameFile = Util::getMaximumTrainerDocumentPath() + QDir::separator() + account->email_clean + ".save";
    qDebug() << "name of File should be" << nameFile;
    QFile fileMt(nameFile);
    /// Open File
    QXmlStreamReader xml(&fileMt);

    // Set default values
    //    setSettingsDefaultValues(settings);


    if (!fileMt.open(QIODevice::ReadOnly)) {
        qDebug() << "problem reading file " << nameFile;
        return;
    }
    else {

        while(!xml.atEnd()  ) {

            if (xml.hasError()) {
                qDebug() << "Error in XML - parseLocalSaveFile" << xml.error();
                return;
            }
            xml.readNextStartElement();

            if (xml.name() == QLatin1String("WorkoutDone") && xml.isStartElement()) {
                parseWorkoutDone(account, xml);
            }

            else if (xml.name() == QLatin1String("IntervalsIcu") && xml.isStartElement()) {
                while (!xml.atEnd()) {
                    xml.readNextStartElement();
                    if (xml.name() == QLatin1String("AthleteId"))
                        account->intervals_icu_athlete_id = xml.readElementText();
                    else if (xml.name() == QLatin1String("ApiKey")) {
                        const QString stored = xml.readElementText();
                        // Decrypt the obfuscated API key. If the value is
                        // empty or was somehow stored in plaintext (missing
                        // the SimpleCrypt magic header), decryptToString()
                        // returns an empty string, which is handled safely.
                        SimpleCrypt crypto(INTERVALS_ICU_CRYPT_KEY);
                        const QString decrypted = crypto.decryptToString(stored);
                        account->intervals_icu_api_key = decrypted.isEmpty() ? stored : decrypted;
                    }
                    else if (xml.tokenType() == QXmlStreamReader::EndElement
                             && xml.name() == QLatin1String("IntervalsIcu"))
                        break;
                }
            }
        }
    }

    qDebug() << "\n\n End Parsing Local Save file!";

}




////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool XmlUtil::saveLocalSaveFile(Account *account) {



    QString nameFile = Util::getMaximumTrainerDocumentPath() + QDir::separator() + account->email_clean + ".save";
    qDebug() << "name of File should be" << nameFile;
    QFile fileMt(nameFile);


    if (!fileMt.open(QIODevice::WriteOnly)) {
        qDebug() << "problem writing to file xml SaveLstWorkoutDone";
        return false;
    }
    else {
        //        this->setDevice(&fileMt);

        QXmlStreamWriter writer(&fileMt);
        writer.setAutoFormatting(true);

        writer.writeStartDocument();
        writer.writeStartElement("MaximumTrainer");

        ///-------------------------------------------------------------------------------------------------------
        //        writer.writeStartElement("ProgramSettings");
        //        writer.writeEndElement();  // ProgramSettings


        ///-------------------------------------------------------------------------------------------------------
        writer.writeStartElement("WorkoutDone");

        foreach (const QString value, account->hashWorkoutDone) {
            if (value.size() > 1)
                writer.writeTextElement("Workout", value);
        }
        writer.writeEndElement();  // WorkoutDone

        ///-------------------------------------------------------------------------------------------------------
        // Intervals.icu credentials — stored locally so they survive re-login.
        // The API key is obfuscated with SimpleCrypt before writing to disk.
        writer.writeStartElement("IntervalsIcu");
        writer.writeTextElement("AthleteId", account->intervals_icu_athlete_id);
        {
            SimpleCrypt crypto(INTERVALS_ICU_CRYPT_KEY);
            writer.writeTextElement("ApiKey", crypto.encryptToString(account->intervals_icu_api_key));
        }
        writer.writeEndElement();  // IntervalsIcu
        ///-------------------------------------------------------------------------------------------------------


        writer.writeEndElement();  // MaximumTrainer


        writer.writeEndDocument();
        fileMt.close();



        qDebug() << "SAVING FILE xml saveLstWorkoutDone done";
        return true;
    }

}




///Save QSet to xml file (.save file)
//---------------------------------------------------------------------------------------------
//bool XmlUtil::saveLstWorkoutDone(QString email_clean, QSet<QString> hashWorkout) {



//    QString nameFile = Util::getMaximumTrainerDocumentPath() + QDir::separator() + email_clean + ".save";
//    qDebug() << "name of File should be" << nameFile;
//    QFile fileMt(nameFile);


//    if (!fileMt.open(QIODevice::WriteOnly)) {
//        qDebug() << "problem writing to file xml SaveLstWorkoutDone";
//        return false;
//    }
//    else {
//        //        this->setDevice(&fileMt);

//        QXmlStreamWriter writer(&fileMt);
//        writer.setAutoFormatting(true);

//        writer.writeStartDocument();
//        writer.writeStartElement("WorkoutDone");

//        foreach (const QString value, hashWorkout) {
//            if (value.size() > 1)
//                writer.writeTextElement("Workout", value);
//        }


//        writer.writeEndElement();  /// WorkoutDone



//        writer.writeEndDocument();
//        fileMt.close();


//        qDebug() << "SAVING FILE xml saveLstWorkoutDone done";
//        return true;
//    }

//}


//---------------------------------------------------------------------------------------------
QList<Workout> XmlUtil::parseWorkoutLstPath(QStringList lstPath, Workout::WORKOUT_NAME workoutType) {

    QList<Workout> lstWorkout;
    Workout workout;

    foreach (QString filePath, lstPath)
    {
        workout = parseSingleWorkoutXml(filePath);
        //        qDebug() << "ok workout has been parsed!" << workout.getName() <<"WorkoutNameEnum:" << workout.getWorkoutNameEnum() << "size source" << workout.getLstIntervalSource().size() <<
        //                    "lst repeat:" << workout.getLstRepeat().size();


        if (workoutType == Workout::INCLUDED_WORKOUT)
            workout.setWorkout_name_enum(Workout::INCLUDED_WORKOUT);
        else if(workoutType == Workout::USER_MADE)
            workout.setWorkout_name_enum(Workout::USER_MADE);
        //        else if(workoutType == Workout::CP_TEST)
        //            workout.setWorkout_name_enum(Workout::CP_TEST);

        lstWorkout.append(workout);

    }
    return lstWorkout;
}




//---------------------------------------------------------------------------------------------
//QList<Workout> XmlUtil::getLstWorkoutTest() {
//    QStringList lstWorkoutPat;
//    lstWorkoutPat.append(":/included_workout/test/included_workout/Test/CP5 Test.workout");
//    lstWorkoutPat.append(":/included_workout/test/included_workout/Test/CP20 Test.workout");
//    return parseWorkoutLstPath(lstWorkoutPat, Workout::CP_TEST);
//}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
QList<Workout> XmlUtil::getLstWorkoutFtpKickstart() {

    QStringList lstWorkoutPat;
    lstWorkoutPat.append(":/included_workout/kickstart/resources/included_workout/FTP Kickstart/Kickstart 1-1 Sweet Spot 2x12.workout");
    lstWorkoutPat.append(":/included_workout/kickstart/resources/included_workout/FTP Kickstart/Kickstart 1-2 Endurance Spin.workout");
    lstWorkoutPat.append(":/included_workout/kickstart/resources/included_workout/FTP Kickstart/Kickstart 1-3 Over-Unders Intro.workout");
    lstWorkoutPat.append(":/included_workout/kickstart/resources/included_workout/FTP Kickstart/Kickstart 2-1 Sweet Spot 2x15.workout");
    lstWorkoutPat.append(":/included_workout/kickstart/resources/included_workout/FTP Kickstart/Kickstart 2-2 Tempo Steps.workout");
    lstWorkoutPat.append(":/included_workout/kickstart/resources/included_workout/FTP Kickstart/Kickstart 2-3 Over-Unders.workout");
    lstWorkoutPat.append(":/included_workout/kickstart/resources/included_workout/FTP Kickstart/Kickstart 3-1 Sweet Spot 3x12.workout");
    lstWorkoutPat.append(":/included_workout/kickstart/resources/included_workout/FTP Kickstart/Kickstart 3-2 Openers.workout");
    lstWorkoutPat.append(":/included_workout/kickstart/resources/included_workout/FTP Kickstart/Kickstart 3-3 Test Primer.workout");
    return parseWorkoutLstPath(lstWorkoutPat, Workout::INCLUDED_WORKOUT);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
QList<Workout> XmlUtil::getLstWorkoutPolarized3x() {

    QStringList lstWorkoutPat;
    lstWorkoutPat.append(":/included_workout/polarized/resources/included_workout/Polarized 3x/Polarized Zone 2.workout");
    lstWorkoutPat.append(":/included_workout/polarized/resources/included_workout/Polarized 3x/Polarized Over-Unders.workout");
    lstWorkoutPat.append(":/included_workout/polarized/resources/included_workout/Polarized 3x/Polarized 30-30s.workout");
    return parseWorkoutLstPath(lstWorkoutPat, Workout::INCLUDED_WORKOUT);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
QList<Workout> XmlUtil::getLstWorkoutVo2ShockBlock() {

    QStringList lstWorkoutPat;
    lstWorkoutPat.append(":/included_workout/shock/resources/included_workout/VO2 Shock Block/Shock 1-1 30-30s.workout");
    lstWorkoutPat.append(":/included_workout/shock/resources/included_workout/VO2 Shock Block/Shock 1-2 Recovery Spin.workout");
    lstWorkoutPat.append(":/included_workout/shock/resources/included_workout/VO2 Shock Block/Shock 1-3 40-20s.workout");
    lstWorkoutPat.append(":/included_workout/shock/resources/included_workout/VO2 Shock Block/Shock 2-1 4x4 Classic.workout");
    lstWorkoutPat.append(":/included_workout/shock/resources/included_workout/VO2 Shock Block/Shock 2-2 Easy Spin.workout");
    lstWorkoutPat.append(":/included_workout/shock/resources/included_workout/VO2 Shock Block/Shock 2-3 30-30s Max.workout");
    return parseWorkoutLstPath(lstWorkoutPat, Workout::INCLUDED_WORKOUT);
}


//---------------------------------------------------------------------------------------------
/// Workout files on user system
QList<Workout> XmlUtil::getLstUserWorkout() {

    QStringList lstWorkoutPat = Util::getListFiles("workout");
    return parseWorkoutLstPath(lstWorkoutPat, Workout::USER_MADE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
QString XmlUtil::parseFileNameFromPath(QString filePath) {

    QFileInfo fileInfo(filePath);
    return fileInfo.baseName();

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Trackpoint XmlUtil::parseTrackpoint(QXmlStreamReader &xml) {


    Trackpoint tp;
    double lon         = 0.0;
    double lat         = 0.0;
    double elevation   = 0.0;
    double slopePercentage = 0.0;
    double distanceAtThisPoint = -1;

    while (xml.tokenType() != QXmlStreamReader::EndElement || xml.name() != QLatin1String("Trackpoint"))
    {
        if (xml.hasError()) {
            qDebug() << "Error in XML, parseTrackpoint method" << xml.error();
            return tp;
        }
        xml.readNextStartElement();


        if(xml.name() == QLatin1String("Lon")) {
            lon = xml.readElementText().toDouble();
        }
        else if(xml.name() == QLatin1String("Lat")) {
            lat = xml.readElementText().toDouble();
        }
        else if(xml.name() == QLatin1String("ElevationMeters")) {
            elevation = xml.readElementText().toDouble();
        }
        else if(xml.name() == QLatin1String("SlopePercentage")) {
            slopePercentage = xml.readElementText().toDouble();
        }
        else if(xml.name() == QLatin1String("Distance")) {
            distanceAtThisPoint = xml.readElementText().toDouble();
        }
    }

    tp = Trackpoint(lon, lat, elevation, slopePercentage, distanceAtThisPoint);
    qDebug() << "Trackpoint parse, lon:" << lon << "lat" << lat << "ele" << elevation << "slopePercentage" << slopePercentage << "distanceAtThisPoint" << distanceAtThisPoint;

    return tp;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Workout XmlUtil::parseSingleWorkoutXml(QString filePath) {

    //    qDebug() << "OK GO TO PARSE THIS WORKOUT" << filePath;

    /// Version of workout xml
    QString versionXml = "-1";

    Workout workout;
    QList<Interval> lstIntervalSource;
    QList<RepeatData> lstRepeat;
    QString description;
    QString creator;
    QString plan = "-";
    Workout::Type type = Workout::Type::T_INTERVAL;
    QString name = parseFileNameFromPath(filePath);


    // Open File
    QFile fileXml(filePath);
    QXmlStreamReader xml(&fileXml);

    if (!fileXml.open(QIODevice::ReadOnly)) {
        qDebug() << "problem reading file " << filePath;
        return workout;
    }
    else {

        while(!xml.atEnd()  )
        {
            if (xml.hasError()) {
                qDebug() << "Error in XML - parseSingleWorkoutXml" << xml.error();
                return workout;
            }
            xml.readNextStartElement();

            if(xml.name() == QLatin1String("Version")) {
                versionXml  = xml.readElementText();
            }
            else if(xml.name() == QLatin1String("Plan")) {
                plan = xml.readElementText();
            }
            else if(xml.name() == QLatin1String("Author")) {
                creator = xml.readElementText();
            }
            else if(xml.name() == QLatin1String("Description")) {
                description = xml.readElementText();
            }
            else if(xml.name() == QLatin1String("Type")) {
                type = static_cast<Workout::Type>(xml.readElementText().toInt());
            }
            /// --------------------------------- Parse Interval -------------------------------------------------
            else if(xml.name() == QLatin1String("Interval")) {
                Interval interval = parseInterval(xml);
                lstIntervalSource.append(interval);
            }

            /// --------------------------------- Parse Repeat -------------------------------------------------
            else if(xml.name() == QLatin1String("Repeat")) {
                RepeatData rep = parseRepeat(xml);
                //                qDebug() << "RepeatWidget parsed, first row is" << rep.getFirstRow();
                if (rep.getId() != -1)
                    lstRepeat.append(rep);
            }
        }
    }

    //    qDebug() << "OK CREATING WORKOUT, file path is" << filePath;
    workout =  Workout(filePath, Workout::WORKOUT_NAME::USER_MADE, lstIntervalSource, lstRepeat,
                       name, creator, description, plan, type );
    //    qDebug() << "size Source inside function " << lstIntervalSource.size() << "lst repeat:" << lstRepeat.size();
    return workout;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Interval XmlUtil::parseInterval(QXmlStreamReader &xml) {



    QTime duration =  QTime(0,0,5);
    QString displayMessage = "";
    bool testInterval = false;
    double repeatIncreaseFTP = 0;
    int repeatIncreaseCadence = 0;
    double repeatIncreaseLTHR = 0;

    Interval::StepType powerStepType = Interval::StepType::NONE;
    double targetFTP_start = 0;
    double targetFTP_end = 0;
    int targetFTP_range = 20;
    double rightPowerTarget = -1;

    Interval::StepType cadenceStepType = Interval::StepType::NONE;
    int targetCadence_start = 0;
    int targetCadence_end = 0;
    int cadence_range = 5;

    Interval::StepType hrStepType = Interval::StepType::NONE;
    double targetHR_start = 0;
    double targetHR_end = 0;
    int HR_range = 20;






    while (xml.tokenType() != QXmlStreamReader::EndElement || xml.name() != QLatin1String("Interval"))
    {
        if (xml.hasError()) {
            qDebug() << "Error in XML, parseInterval method" << xml.error();
            return Interval();
        }
        xml.readNextStartElement();


        if(xml.name() == QLatin1String("Duration")) {
            duration = QTime::fromString( xml.readElementText(), "hh:mm:ss");
        }
        else if(xml.name() == QLatin1String("DisplayMessage")) {
            displayMessage = xml.readElementText();

        }
        else if(xml.name() == QLatin1String("TestInterval")) {
            testInterval = xml.readElementText().toInt();
        }
        else if(xml.name() == QLatin1String("RepeatIncreaseFTP")) {
            repeatIncreaseFTP = xml.readElementText().toDouble();
        }
        else if(xml.name() == QLatin1String("RepeatIncreaseCadence")) {
            repeatIncreaseCadence = xml.readElementText().toInt();
        }
        else if(xml.name() == QLatin1String("RepeatIncreaseLTHR")) {
            repeatIncreaseLTHR = xml.readElementText().toDouble();
        }


        /// ----------------------------- POWER -------------------------------------
        else if(xml.name() == QLatin1String("Power"))
        {
            while (xml.tokenType() != QXmlStreamReader::EndElement || xml.name() != QLatin1String("Power"))
            {
                xml.readNextStartElement();
                if(xml.name() == QLatin1String("StepType")) {
                    powerStepType = static_cast<Interval::StepType>(xml.readElementText().toInt());
                }
                else if(xml.name() == QLatin1String("Start")) {
                    targetFTP_start = xml.readElementText().toDouble();
                }
                else if(xml.name() == QLatin1String("End")) {
                    targetFTP_end = xml.readElementText().toDouble();
                }
                else if(xml.name() == QLatin1String("Range")) {
                    targetFTP_range = xml.readElementText().toDouble();
                }
                else if(xml.name() == QLatin1String("RightBalance")) {
                    rightPowerTarget = xml.readElementText().toInt();
                }
            }
        }
        /// ----------------------------- CADENCE -------------------------------------
        else if(xml.name() == QLatin1String("Cadence"))
        {
            while (xml.tokenType() != QXmlStreamReader::EndElement || xml.name() != QLatin1String("Cadence"))
            {
                xml.readNextStartElement();
                if(xml.name() == QLatin1String("StepType")) {
                    cadenceStepType= static_cast<Interval::StepType>(xml.readElementText().toInt());
                }
                else if(xml.name() == QLatin1String("Start")) {
                    targetCadence_start = xml.readElementText().toInt();
                }
                else if(xml.name() == QLatin1String("End")) {
                    targetCadence_end = xml.readElementText().toInt();
                }
                else if(xml.name() == QLatin1String("Range")) {
                    cadence_range = xml.readElementText().toInt();
                }
            }
        }
        /// ----------------------------- HR -------------------------------------
        else if(xml.name() == QLatin1String("HeartRate"))
        {
            while (xml.tokenType() != QXmlStreamReader::EndElement || xml.name() != QLatin1String("HeartRate"))
            {
                xml.readNextStartElement();
                if(xml.name() == QLatin1String("StepType")) {
                    hrStepType = static_cast<Interval::StepType>(xml.readElementText().toInt());
                }
                else if(xml.name() == QLatin1String("Start")) {
                    targetHR_start = xml.readElementText().toDouble();
                }
                else if(xml.name() == QLatin1String("End")) {
                    targetHR_end = xml.readElementText().toDouble();
                }
                else if(xml.name() == QLatin1String("Range")) {
                    HR_range = xml.readElementText().toInt();
                }
            }
        }


    }

    Interval interval(duration, displayMessage, powerStepType, targetFTP_start, targetFTP_end, targetFTP_range, rightPowerTarget,
                      cadenceStepType, targetCadence_start, targetCadence_end, cadence_range,
                      hrStepType, targetHR_start, targetHR_end, HR_range,
                      testInterval, repeatIncreaseFTP, repeatIncreaseCadence, repeatIncreaseLTHR);
    return interval;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RepeatData XmlUtil::parseRepeat(QXmlStreamReader &xml) {



    RepeatData rep;

    while (xml.tokenType() != QXmlStreamReader::EndElement || xml.name() != QLatin1String("Repeat"))
    {
        if (xml.hasError()) {
            qDebug() << "Error in XML, parseRepeat method" << xml.error();
            rep.setId(-1);
            return rep;
        }
        xml.readNextStartElement();

        //        qDebug() << "parseRepeat:" << xml.name();

        if(xml.name() == QLatin1String("Id")) {
            rep.setId(xml.readElementText().toInt());
        }
        else if(xml.name() == QLatin1String("FirstRow")) {
            rep.setFirstRow(xml.readElementText().toInt());
        }
        else if(xml.name() == QLatin1String("LastRow")) {
            rep.setLastRow(xml.readElementText().toInt());
        }
        else if(xml.name() == QLatin1String("NumberRepeat")) {
            rep.setNumberRepeat(xml.readElementText().toInt());
        }
    }

    /// TODO Verif validity first row, with interval list

    return rep;

}


//-------------------------------------------------------------------------------------
bool XmlUtil::createWorkoutXml(Workout workout, QString destinationPath) {


    QXmlStreamWriter stream;
    stream.setAutoFormatting(true);

    QString nameFile;
    if (destinationPath == "") {
        nameFile = workout.getFilePath();
    }
    else {
        nameFile = destinationPath;
    }
    qDebug() << "name of File should be" << nameFile;
    QFile fileMt(nameFile);


    if (!fileMt.open(QIODevice::WriteOnly)) {
        qDebug() << "problem writing to file tcx";
        return false;
    }
    ///----------------------------------- WRITE TO MT FILE ------------------------------------------------
    else {
        stream.setDevice(&fileMt);

        stream.writeStartDocument();
        stream.writeStartElement("Workout");
        stream.writeTextElement("Version", Environnement::getVersion());
        stream.writeTextElement("Plan", workout.getPlan());
        stream.writeTextElement("Author", workout.getCreatedBy());
        stream.writeTextElement("Description", workout.getDescription());
        stream.writeTextElement("Type", QString::number(workout.getType()) );   // 0=Tempo, 1=Endurance, 2=Test, 3=Other


        ///---------------------- INTERVALS -----------------------------
        stream.writeStartElement("Intervals");
        foreach (Interval interval, workout.getLstIntervalSource())
        {
            stream.writeStartElement("Interval");
            stream.writeTextElement("Duration", interval.getDurationQTime().toString(Qt::ISODate));
            stream.writeTextElement("DisplayMessage", interval.getDisplayMessage());
            stream.writeTextElement("TestInterval", QString::number(interval.isTestInterval()) );
            stream.writeTextElement("RepeatIncreaseFTP", QString::number(interval.getRepeatIncreaseFTP()) );
            stream.writeTextElement("RepeatIncreaseCadence", QString::number(interval.getRepeatIncreaseCadence()) );
            stream.writeTextElement("RepeatIncreaseLTHR", QString::number(interval.getRepeatIncreaseLTHR()) );


            stream.writeStartElement("Power");
            stream.writeTextElement("StepType", QString::number(interval.getPowerStepType()) );
            stream.writeTextElement("Start", QString::number(interval.getFTP_start()) );
            stream.writeTextElement("End", QString::number(interval.getFTP_end()) );
            stream.writeTextElement("Range", QString::number(interval.getFTP_range()) );
            stream.writeTextElement("RightBalance", QString::number(interval.getRightPowerTarget()) );
            stream.writeEndElement();  /// Power

            stream.writeStartElement("Cadence");
            stream.writeTextElement("StepType", QString::number(interval.getCadenceStepType()) );
            stream.writeTextElement("Start", QString::number(interval.getCadence_start()) );
            stream.writeTextElement("End", QString::number(interval.getCadence_end()) );
            stream.writeTextElement("Range", QString::number(interval.getCadence_range()) );
            stream.writeEndElement();  /// Cadence

            stream.writeStartElement("HeartRate");
            stream.writeTextElement("StepType", QString::number(interval.getHRStepType()) );
            stream.writeTextElement("Start", QString::number(interval.getHR_start()) );
            stream.writeTextElement("End", QString::number(interval.getHR_end()) );
            stream.writeTextElement("Range", QString::number(interval.getHR_range()) );
            stream.writeEndElement();  /// Heartrate

            stream.writeEndElement();  /// Interval
        }
        stream.writeEndElement();  /// Intervals


        ///---------------------- REPEAT --------------------------------
        stream.writeStartElement("Repeats");
        foreach (RepeatData repeat, workout.getLstRepeat())
        {
            stream.writeStartElement("Repeat");
            stream.writeTextElement("Id", QString::number(repeat.getId()));
            stream.writeTextElement("FirstRow", QString::number(repeat.getFirstRow()));
            stream.writeTextElement("LastRow", QString::number(repeat.getLastRow()));
            stream.writeTextElement("NumberRepeat", QString::number(repeat.getNumberRepeat()));
            stream.writeEndElement();  /// Repeat
        }
        stream.writeEndElement();  /// Repeats


        stream.writeEndElement();  /// Workout
        stream.writeEndDocument();
        fileMt.close();


        qDebug() << "SAVING FILE DONE";
        return true;
    }

}









