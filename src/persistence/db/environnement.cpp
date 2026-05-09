#include "environnement.h"

#include "settings.h"
#include "credential_store.h"

Environnement::Environnement()
{  
}




//////////////////////////////////////////////////////////////
QString Environnement::getURLEnvironnement() {

    if (current_env == "dev"){
        return dev;
    }
    else if (current_env == "prod") {
        return prod;
    }
    else {
        return dev;
    }
}
//////////////////////////////////////////////////////////////
QString Environnement::getURLEnvironnementWS() {

    if (current_env == "dev"){
        return dev;
    }
    else if (current_env == "prod") {
        return prod + indexPage;
    }
    else {
        return dev;
    }
}

//////////////////////////////////////////////////////////////
QString Environnement::getVersion() {
#ifdef APP_VERSION
    // trimmed() removes any trailing \r or \n that qmake's $$system() may
    // embed on Windows when capturing the output of `git describe`.
    return QString(APP_VERSION).trimmed();
#else
    return current_version;
#endif
}
//////////////////////////////////////////////////////////////
QString Environnement::getDateBuilded() {

    return date_released;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlLogin() {


    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlLoginEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlLoginFr;
    }
    else {
        return getURLEnvironnement() + urlLoginEn;
    }

}


///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlProfileOutsideMt() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlProfilEnOutsideMt;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlProfilFrOutsideMt;
    }
    else {
        return getURLEnvironnement() + urlProfilEnOutsideMt;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlChooseSub() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlChooseSubEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlChooseSubFr;
    }
    else {
        return getURLEnvironnement() + urlChooseSubEn;
    }
}



///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlNews() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlNewsEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlNewsFr;
    }
    else {
        return getURLEnvironnement() + urlNewsEn;
    }
}




///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlZones() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlZonesEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlZonesFr;
    }
    else {
        return getURLEnvironnement() + urlZonesEn;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlStudio() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlStudioEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlStudioFr;
    }
    else {
        return getURLEnvironnement() + urlStudioEn;
    }
}




///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlAchievement() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlAchievEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlAchievFr;
    }
    else {
        return getURLEnvironnement() + urlAchievEn;
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlPlans() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlPlanEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlPlanFr;
    }
    else {
        return getURLEnvironnement() + urlPlanEn;
    }

}

///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlSettings() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlSettingsEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlSettingsFr;
    }
    else {
        return getURLEnvironnement() + urlSettingsEn;
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlWorkout() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlWorkoutEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlWorkoutFr;
    }
    else {
        return getURLEnvironnement() + urlWorkoutEn;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlSupport() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlSupportEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlSupportFr;
    }
    else {
        return getURLEnvironnement() + urlSupportEn;
    }

}

//////////////////////////////////////////////////////////////////////////////////////////////////////
QString  Environnement::getURLGoogleMap() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlGoogleMapEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlGoogleMapEn;
    }
    else {
        return getURLEnvironnement() + urlGoogleMapEn;
    }

}


//////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getURLStravaAuthorize() {

    QString myURL = urlStravaAuthorize;
    //Will redirect to this url after authorization
    //On success, a code will be included in the query string.
    //If access is denied, error=access_denied will be included in the query string. In both cases, if provided, the state argument will also be included.
    myURL += "&redirect_uri=" + getURLEnvironnement() + "strava_token_exchange";

    return (myURL);

}

//////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getURLTrainingPeaksAuthorize() {

    QString myURL = urlTrainingPeaksAuthorize;
    myURL += "&redirect_uri=" + getURLEnvironnement() + "trainingpeaks_token_exchange";
    return (myURL);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/// Build the full Intervals.icu OAuth2 authorization URL, including the
/// redirect_uri that points back to the MaximumTrainer.com backend proxy so
/// it can exchange the authorization code for an access token securely.
/// @param state  A per-request random token for CSRF protection.  The caller
///               must store this value and validate it matches the state
///               parameter on the redirect callback.
QString Environnement::getURLIntervalsIcuAuthorize(const QString &state) {

    QString myURL = urlIntervalsIcuOAuthAuthorize;
    myURL += "&client_id=" + getIntervalsIcuClientId();
    myURL += "&redirect_uri=" + getURLEnvironnement() + "intervals_icu_token_exchange";
    if (!state.isEmpty())
        myURL += "&state=" + state;
    return myURL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getIntervalsIcuClientId() {
    const QString stored = CredentialStore::load("intervals_icu_app", "client_id");
    return stored.isEmpty() ? CLIENT_ID_ICV : stored;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getIntervalsIcuClientSecret() {
    const QString stored = CredentialStore::load("intervals_icu_app", "client_secret");
    return stored.isEmpty() ? CLIENT_SECRET_ICV : stored;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlIntervalsIcuRegister() {
    return urlIntervalsIcuRegister;
}




///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlWorkoutCreator() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlWorkoutCreatorEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlWorkoutCreatorFr;
    }
    else {
        return getURLEnvironnement() + urlWorkoutCreatorEn;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlCourseCreator() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlCourseCreatorEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlCourseCreatorFr;
    }
    else {
        return getURLEnvironnement() + urlCourseCreatorEn;
    }
}



///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlDownload() {

    Settings *settings = qApp->property("User_Settings").value<Settings*>();

    if (settings->language == "en") {
        return getURLEnvironnement() + urlDownloadEn;
    }
    else if (settings->language == "fr") {
        return getURLEnvironnement() + urlDownloadFr;
    }
    else {
        return getURLEnvironnement() + urlDownloadEn;
    }

}


