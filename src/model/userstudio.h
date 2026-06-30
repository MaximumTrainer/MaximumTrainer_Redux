#ifndef USERSTUDIO_H
#define USERSTUDIO_H

#include <QtCore>


class UserStudio
{


public:
    UserStudio() {}
    UserStudio(QString displayName, int FTP, int LTHR, int hrID, int powerID, int cadenceID, int speedID, int fecID,
               int wheelCircMM, int companyID, int brandID);

    /// Studio rider identity (name / FTP / LTHR) persistence in QSettings, group
    /// "studioRiders/riderN". Returns a fixed-size vector (constants::nbMaxUserStudio),
    /// defaulting riders with no saved data. Sensors and ERG live in their own
    /// QSettings groups (BtleSensorStore / studioErg); this is name/FTP/LTHR only.
    static QVector<UserStudio> loadStudioConfig();
    static void saveStudioConfig(const QVector<UserStudio> &riders);




    //getters
    QString getDisplayName() const {
        return this->displayName;
    }
    int getFTP() const {
        return this->FTP;
    }
    int getLTHR() const {
        return this->LTHR;
    }

    int getHrID() const {
        return this->hrID;
    }
    int getPowerID() const {
        return this->powerID;
    }
    int getCadenceID() const {
        return this->cadenceID;
    }
    int getSpeedID() const {
        return this->speedID;
    }
    int getFecID() const {
        return this->fecID;
    }

    int getWheelCircMM() const {
        return this->wheelCircMM;
    }
    int getCompanyID() const {
        return this->companyID;
    }
    int getBrandID() const {
        return this->brandID;
    }


    //QString displayName;  = 0
    //int FTP;              = 1
    //int LTHR;             = 2
    //int hrID;             = 3
    //int power             = 4
    //int cadenceID;        = 5
    //int speedID;          = 6
    //int fecID;            = 7
    //int wheelCircMM;      = 8

    //Setters
    void setDisplayName(QString displayName) {
        this->displayName = displayName;
    }
    void setFTP(int ftp) {
        this->FTP = ftp;
    }
    void setLTHR(int lthr) {
        this->LTHR = lthr;
    }
    void setHrID(int hrID) {
        this->hrID = hrID;
    }
    void setPowerID(int powerID) {
        this->powerID = powerID;
    }
    void setCadenceID(int cadenceID) {
        this->cadenceID = cadenceID;
    }
    void setSpeedID(int speedID) {
        this->speedID = speedID;
    }
    void setFecID(int fecID) {
        this->fecID = fecID;
    }
    void setWheelCircMM(int wheelCircMM) {
        this->wheelCircMM = wheelCircMM;
    }

    void setCompanyID(int id) {
        this->companyID = id;
    }
    void setBrandID(int id) {
        this->brandID = id;
    }


private :

    QString displayName;
    int FTP;
    int LTHR;

    int hrID;
    int powerID;
    int cadenceID;
    int speedID;
    int fecID;

    int wheelCircMM;

    int companyID;
    int brandID;


};
Q_DECLARE_METATYPE(UserStudio)

#endif // USERSTUDIO_H
