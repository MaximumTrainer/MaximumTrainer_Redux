#include "environnement.h"

#include "credential_store.h"
#include <QUrl>
#include <QUrlQuery>

Environnement::Environnement()
{  
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



//////////////////////////////////////////////////////////////////////////////////////////////////////
QString Environnement::getURLStravaAuthorize(const QString &redirectUri) {
    // Opened in the system browser; Strava redirects to redirectUri with
    // ?code=... on success (?error=access_denied if declined). For the desktop
    // flow redirectUri is a localhost loopback URL (StravaOAuthFlow), so the
    // Strava app's Authorization Callback Domain must be `localhost`.
    QUrl url(QStringLiteral("https://www.strava.com/oauth/authorize"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("client_id"),       CLIENT_ID_STRAVA);
    query.addQueryItem(QStringLiteral("response_type"),   QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("redirect_uri"),    redirectUri);
    query.addQueryItem(QStringLiteral("approval_prompt"), QStringLiteral("auto"));
    query.addQueryItem(QStringLiteral("scope"),           QStringLiteral("activity:write"));
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/// Build the full Intervals.icu OAuth2 authorization URL.
///
/// On WASM the GitHub Pages callback page (getWasmOAuthRedirectUri) is the
/// redirect_uri: it is loaded in a popup that posts the authorization code
/// back via window.opener.postMessage.  On desktop, IntervalsIcuOAuthFlow
/// passes its localhost loopback listener as the redirect_uri instead
/// (intervals.icu always allows http://localhost/).
///
/// The subsequent token POST goes through the Cloudflare Worker to
/// /api/oauth/token on all platforms (see URL_TOKEN_ICV).
///
/// @param state        A per-request random token for CSRF protection.  The
///                     caller must store this value and validate it matches
///                     the state parameter on the redirect callback.
/// @param redirectUri  The redirect_uri to register for this request.
QString Environnement::getURLIntervalsIcuAuthorize(const QString &state,
                                                   const QString &redirectUri) {
    QUrl url(urlIntervalsIcuOAuthAuthorize);
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    query.addQueryItem(QStringLiteral("scope"), intervalsIcuOAuthScope);
    query.addQueryItem(QStringLiteral("client_id"), getIntervalsIcuClientId());
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri);
    if (!state.isEmpty())
        query.addQueryItem(QStringLiteral("state"), state);
    url.setQuery(query);
    return url.toString(QUrl::FullyEncoded);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
/// Build the Intervals.icu OAuth2 authorization URL for the WASM popup flow.
/// Uses a GitHub Pages callback page as the redirect_uri.  The callback page
/// sends the authorization code back to the main window via window.opener.postMessage.
/// @param state  A per-request random token for CSRF protection.
QString Environnement::getURLIntervalsIcuAuthorizeWasm(const QString &state) {
    return getURLIntervalsIcuAuthorize(state, getWasmOAuthRedirectUri());
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

