#include "userstudio.h"







UserStudio::UserStudio(QString displayName, int FTP, int LTHR, int hrID, int powerID, int cadenceID, int speedID, int fecID,
                       int wheelCircMM, bool usingPowerCurve, int companyID, int brandID) {

    this->displayName = displayName;
    this->FTP = FTP;
    this->LTHR = LTHR;

    this->hrID = hrID;
    this->powerID = powerID;
    this->cadenceID = cadenceID;
    this->speedID = speedID;
    this->fecID = fecID;

    this->wheelCircMM = wheelCircMM;

    this->usingPowerCurve = usingPowerCurve;
    this->companyID = companyID;
    this->brandID = brandID;
}

//////////////////////////////////////////////////////////////////////////////////////////////
//UserStudio::UserStudio( const UserStudio& other ) {

//    this->displayName = other.displayName;
//    this->FTP = other.FTP;
//    this->LTHR = other.LTHR;

//    this->hrID = other.hrID;
//    this->powerID = other.powerID;
//    this->cadenceID = other.cadenceID;
//    this->speedID = other.speedID;
//    this->fecID = other.fecID;

//    this->wheelCircMM = other.wheelCircMM;

//    this->usingPowerCurve = other.usingPowerCurve;
//    this->companyID = other.companyID;
//    this->brandID = other.brandID;

//    this->powerCurve = other.powerCurve;


//}

