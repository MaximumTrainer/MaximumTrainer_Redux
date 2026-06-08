#include "zoneobject.h"
#include <QDebug>
#include "myconstants.h"


ZoneObject::ZoneObject(QObject *parent) :
    QObject(parent)
{
    this->account = qApp->property("Account").value<Account*>();
}


///---------------------------------------------------------------------------
void ZoneObject::updateFTP(int value) {

    qDebug() << "UPDATE FTP ZONEOBJECT";

    account->FTP = value;
    emit signal_updateFTP();
}


///---------------------------------------------------------------------------
void ZoneObject::updateLTHR(int value) {

    qDebug() << "update LTHR ZONEOBJECT";

    account->LTHR = value;
    emit signal_updateLTHR();
}

///---------------------------------------------------------------------------
void ZoneObject::updateUserWeight(double weight, bool isKg) {

    qDebug() << "ZONEOBJECT "<< weight << " in KG?" << isKg;

    double weightKg = weight;
    if (!isKg) {
        weightKg = weight/constants::GLOBAL_CONST_CONVERT_KG_TO_LBS;
    }

    account->weight_in_kg = isKg;
    account->weight_kg = weightKg;
}

///---------------------------------------------------------------------------
 void ZoneObject::updateUserHeight(int height) {

      qDebug() << "updateUserHeight" << height;
     account->height_cm = height;


 }
 ///---------------------------------------------------------------------------
  void ZoneObject::updateCDA(double cda) {

      qDebug() << "updateCDA" << cda;

      account->userCda = cda;
  }


  ///---------------------------------------------------------------------------
 void ZoneObject::updateWheelSize(int size_mm) {

     qDebug() << "updateWheelSize" << size_mm;
     account->wheel_circ = size_mm;
 }

 ///---------------------------------------------------------------------------
 void ZoneObject::updateBikeWeight(double weight_kg) {

     qDebug() << "updateBikeWeight" << weight_kg;
     account->bike_weight_kg = weight_kg;
 }

 ///---------------------------------------------------------------------------
 void ZoneObject::updateBiketype(int bikeType) {

      qDebug() << "updateBiketype" << bikeType;
     account->bike_type = bikeType;
 }

 ////////////////////////////////////////////////////////////////////////////////////
 void ZoneObject::updateDisplayName(int playerId, QString name) {

     qDebug() << "playerID" << playerId << "name" << name;
     account->display_name = name;


 }
