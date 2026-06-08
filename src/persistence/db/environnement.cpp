#include "environnement.h"

#include "credential_store.h"
#include <QUrl>
#include <QUrlQuery>

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
    return getURLEnvironnement() + urlLoginEn;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlProfileOutsideMt() {
    return getURLEnvironnement() + urlProfilEnOutsideMt;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlChooseSub() {
    return getURLEnvironnement() + urlChooseSubEn;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlNews() {
    return getURLEnvironnement() + urlNewsEn;
}




///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlZones() {
    return getURLEnvironnement() + urlZonesEn;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlStudio() {
    return getURLEnvironnement() + urlStudioEn;
}




///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlAchievement() {
    return getURLEnvironnement() + urlAchievEn;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlPlans() {
    return getURLEnvironnement() + urlPlanEn;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlSettings() {
    return getURLEnvironnement() + urlSettingsEn;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlWorkout() {
    return getURLEnvironnement() + urlWorkoutEn;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlSupport() {
    return getURLEnvironnement() + urlSupportEn;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getURLStravaAuthorize() {
    // Redirects to the github.io callback page with ?code=... on success
    // (?error=access_denied if the user declines). DialogInfoWebView catches
    // that redirect, extracts the code, and exchanges it via the Strava Worker.
    QUrl url(QStringLiteral("https://www.strava.com/oauth/authorize"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"),       CLIENT_ID_STRAVA);
    query.addQueryItem(QStringLiteral("response_type"),   QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("redirect_uri"),    getWasmOAuthRedirectUri());
    query.addQueryItem(QStringLiteral("approval_prompt"), QStringLiteral("auto"));
    query.addQueryItem(QStringLiteral("scope"),           QStringLiteral("activity:write"));
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/// Build the full Intervals.icu OAuth2 authorization URL.
///
/// Both desktop and WASM builds use the same redirect_uri — the GitHub Pages
/// callback page (Environnement::getWasmOAuthRedirectUri).  On WASM the page
/// is loaded in a popup that posts the authorization code back via
/// window.opener.postMessage.  On desktop the embedded QWebEngineView
/// (IntervalsIcuOAuthWidget / DialogInfoWebView) detects the redirect to
/// oauth_callback.html and extracts the code from its query string.
///
/// In both cases the subsequent /oauth/token POST goes through the
/// Cloudflare Worker proxy (see URL_TOKEN_ICV in environnement.h).
///
/// @param state  A per-request random token for CSRF protection.  The caller
///               must store this value and validate it matches the state
///               parameter on the redirect callback.
QString Environnement::getURLIntervalsIcuAuthorize(const QString &state) {
    return getURLIntervalsIcuAuthorizeWasm(state);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/// Build the Intervals.icu OAuth2 authorization URL for the WASM popup flow.
/// Uses a GitHub Pages callback page as the redirect_uri.  The callback page
/// sends the authorization code back to the main window via window.opener.postMessage.
/// @param state  A per-request random token for CSRF protection.
QString Environnement::getURLIntervalsIcuAuthorizeWasm(const QString &state) {
    QUrl url(urlIntervalsIcuOAuthAuthorize);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), intervalsIcuOAuthScope);
    query.addQueryItem(QStringLiteral("client_id"), getIntervalsIcuClientId());
    query.addQueryItem(QStringLiteral("redirect_uri"), getWasmOAuthRedirectUri());
    if (!state.isEmpty())
        query.addQueryItem(QStringLiteral("state"), state);
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/// Return the WASM-specific OAuth2 redirect_uri.
/// This page must be registered as an allowed redirect URI with Intervals.icu OAuth client 259.
QString Environnement::getWasmOAuthRedirectUri() {
    return QStringLiteral(
        "https://maximumtrainer.github.io/MaximumTrainer_Redux/app/oauth_callback.html");
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
    return getURLEnvironnement() + urlWorkoutCreatorEn;
}




///////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getUrlDownload() {
    return getURLEnvironnement() + urlDownloadEn;
}

