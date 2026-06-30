#include "userstudio.h"

#include <QSettings>

#include "myconstants.h"

namespace {
QString riderGroup(int riderIndex) {   // riderIndex is 1-based
    return QStringLiteral("studioRiders/rider%1").arg(riderIndex);
}
}

QVector<UserStudio> UserStudio::loadStudioConfig()
{
    QVector<UserStudio> riders;
    QSettings settings;
    for (int i = 1; i <= constants::nbMaxUserStudio; ++i) {
        const QString g = riderGroup(i);
        const QString name = settings.value(g + QStringLiteral("/name")).toString();
        const int ftp      = settings.value(g + QStringLiteral("/ftp"),  -1).toInt();
        const int lthr     = settings.value(g + QStringLiteral("/lthr"), -1).toInt();
        riders.append(UserStudio(name, ftp, lthr, -1, -1, -1, -1, -1, 2100, 0, 0));
    }
    return riders;
}

void UserStudio::saveStudioConfig(const QVector<UserStudio> &riders)
{
    QSettings settings;
    for (int i = 0; i < riders.size() && i < constants::nbMaxUserStudio; ++i) {
        const UserStudio &u = riders.at(i);
        const QString g = riderGroup(i + 1);
        // Drop fully-empty riders so the config stays tidy; keep any with data.
        if (u.getDisplayName().trimmed().isEmpty() && u.getFTP() <= 0 && u.getLTHR() <= 0) {
            settings.remove(g);
        } else {
            settings.setValue(g + QStringLiteral("/name"), u.getDisplayName());
            settings.setValue(g + QStringLiteral("/ftp"),  u.getFTP());
            settings.setValue(g + QStringLiteral("/lthr"), u.getLTHR());
        }
    }
}






UserStudio::UserStudio(QString displayName, int FTP, int LTHR, int hrID, int powerID, int cadenceID, int speedID, int fecID,
                       int wheelCircMM, int companyID, int brandID) {

    this->displayName = displayName;
    this->FTP = FTP;
    this->LTHR = LTHR;

    this->hrID = hrID;
    this->powerID = powerID;
    this->cadenceID = cadenceID;
    this->speedID = speedID;
    this->fecID = fecID;

    this->wheelCircMM = wheelCircMM;

    this->companyID = companyID;
    this->brandID = brandID;
}
