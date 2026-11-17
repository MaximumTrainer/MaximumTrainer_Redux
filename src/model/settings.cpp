#include "settings.h"
#include "setting_keys.h"

#include <QSettings>


Settings::~Settings() {
}


/// Constructor - load settings
Settings::Settings(QObject *parent) : QObject(parent)  {


    QSettings settings;
    lastLoggedUsername = AppSettings::read(settings, SettingKeys::Login::LastLoggedUsername);
    lastLoggedKey      = AppSettings::read(settings, SettingKeys::Login::LastLoggedKey);

    settings.beginGroup("SettingsGeneral");
    // Keys of the removed remembered-usernames list; "username"+i was pointer
    // arithmetic, hence the truncated names.
    for (const char *staleKey : {"rememberMyPassword", "username", "sername", "ername", "rname", "name"})
        settings.remove(staleKey);
    settings.endGroup();


}


void Settings::saveGeneralSettings() {

    QSettings settings;
    AppSettings::write(settings, SettingKeys::Login::LastLoggedUsername, lastLoggedUsername);
    AppSettings::write(settings, SettingKeys::Login::LastLoggedKey,      lastLoggedKey);


}

