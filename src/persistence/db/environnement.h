#ifndef ENVIRONNEMENT_H
#define ENVIRONNEMENT_H

#include <QtCore>


const static QString current_version = "v0.0.0";  // overridden at build time via APP_VERSION
const static QString date_released = "16/03/2019";  //(dd/mm/yyyy)

/// GitHub Releases — used for version check and update dialog
const static QString urlGitHubReleasesApi  = "https://api.github.com/repos/MaximumTrainer/MaximumTrainer_Redux/releases/latest";
const static QString urlGitHubReleasesPage = "https://github.com/MaximumTrainer/MaximumTrainer_Redux/releases/latest";

/// Intervals.icu REST API base URL
/// Endpoints:
///   GET /api/v1/athlete/{id}                             — basic profile (name, weight, FTP, LTHR)
///   GET /api/v1/athlete/{id}/settings                   — detailed training zones
///   GET /api/v1/athlete/{id}/workouts/{workoutId}.zwo   — download workout as ZWO file
/// Authentication: HTTP Basic Auth with username "API_KEY" and the user's API key
///                 as the password.
const static QString urlIntervalsIcuApi = "https://intervals.icu/api/v1/athlete/";
const static QString urlIntervalsIcu = "https://intervals.icu/";

/// Intervals.icu athlete calendar web URL.
/// Use QString::arg(athleteId) to substitute the athlete ID placeholder.
/// Example: urlIntervalsIcuCalendar.arg("i12345")
///   → "https://intervals.icu/athlete/i12345/calendar"
const static QString urlIntervalsIcuCalendar = "https://intervals.icu/athlete/%1/calendar";

/// Intervals.icu OAuth2 Authorization Code Flow
/// Client ID 259 is registered for MaximumTrainer.
///
/// Required scopes:
///   ACTIVITY:WRITE
///   WELLNESS:READ
///   SETTINGS:WRITE
///   CALENDAR:WRITE
///   LIBRARY:READ
///
/// OAuth2 authorize endpoint + canonical comma-separated scope string.
/// Query parameters are appended safely via QUrlQuery in
/// Environnement::getURLIntervalsIcuAuthorizeWasm().
const static QString urlIntervalsIcuOAuthAuthorize =
    QStringLiteral("https://intervals.icu/oauth/authorize");
const static QString intervalsIcuOAuthScope =
    QStringLiteral("ACTIVITY:WRITE,WELLNESS:READ,SETTINGS:WRITE,CALENDAR:WRITE,LIBRARY:READ");

/// Intervals.icu OAuth2 client_id, injected at build time via the
/// INTERVALS_OAUTH_CLIENT_ID environment variable (see MaximumTrainer.pro);
/// defaults to "259" (existing public client).  The client_secret is
/// deliberately NOT in the app: the token Worker injects it server-side
/// (see URL_TOKEN_ICV).
#ifndef INTERVALS_OAUTH_CLIENT_ID
#define INTERVALS_OAUTH_CLIENT_ID "259"
#endif
const static QString CLIENT_ID_ICV = QStringLiteral(INTERVALS_OAUTH_CLIENT_ID);

/// Cloudflare CORS proxy that fronts intervals.icu for both desktop and WASM
/// builds.  All OAuth token exchange / refresh POSTs (and, in WASM, all API
/// requests via the index.html XHR/fetch interceptor) go through this proxy.
/// See workers/intervals-cors-proxy/ and docs/app/index.html.
const static QString INTERVALS_PROXY_BASE =
    QStringLiteral("https://mt-intervals-proxy.intervals-login.workers.dev");

/// Header name + value used by the native desktop client to identify itself
/// to the Cloudflare proxy's allow-list when routing token requests through
/// it.  The desktop build is not a browser and is not subject to CORS, so it
/// does NOT send an Origin header; instead it sends `X-MT-Client: desktop`,
/// which the worker checks against its ALLOWED_CLIENTS list
/// (workers/intervals-cors-proxy/worker.js).  This is not a security
/// boundary — any HTTP client can forge the header — it just keeps the proxy
/// from acting as a fully open relay.
const static QString INTERVALS_PROXY_CLIENT_HEADER =
    QStringLiteral("X-MT-Client");
const static QString INTERVALS_PROXY_DESKTOP_CLIENT_VALUE =
    QStringLiteral("desktop");

/// OAuth2 token endpoint, used for both the authorization-code exchange and
/// refresh-token requests in ExtRequest::intervalsIcuOAuthExchange/Refresh.
///
/// Single source of truth for ALL platforms (same architecture as the Strava
/// token Worker): every build posts to the Cloudflare Worker, which proxies
/// to https://intervals.icu/api/oauth/token — NOT /oauth/token, which only
/// serves the authorize flow (POST there returns 405/404).
///
/// intervals.icu REQUIRES the client_secret on token requests (422 without
/// it); client 259 is confidential.  The Worker injects the secret
/// server-side (CLIENT_SECRETS KV keyed by client_id), so no app binary
/// carries it.
const static QString URL_TOKEN_ICV = INTERVALS_PROXY_BASE + "/proxy/api/oauth/token";

/// Sentinel athlete ID meaning "the currently authenticated OAuth2 user".
/// Pass this to Bearer-token API calls when the real athlete ID is not yet known.
const static QString INTERVALS_ICU_CURRENT_USER_ID = "0";

/// Registration page for users who do not yet have an Intervals.icu account.
const static QString urlIntervalsIcuRegister = "https://intervals.icu/register";



/// Strava OAuth2 (app 7252). The client_secret is deliberately NOT here — it
/// lives in the Strava token Worker (workers/strava-token-proxy), which the app
/// calls at URL_TOKEN_STRAVA to exchange the authorization code and to refresh.
/// The redirect_uri reuses the github.io OAuth callback page
/// (getWasmOAuthRedirectUri); the app's Authorization Callback Domain in Strava
/// settings must be maximumtrainer.github.io. Scope is activity:write (upload).
const static QString CLIENT_ID_STRAVA = QStringLiteral("7252");
/// NOTE: the Cloudflare account's workers.dev subdomain is "intervals-login"
/// (the old "maximumtrainer" subdomain is dead and 404s every worker URL —
/// that silently broke Strava token refresh/exchange until it was caught).
const static QString STRAVA_TOKEN_PROXY_BASE =
    QStringLiteral("https://mt-strava-token.intervals-login.workers.dev");
const static QString URL_TOKEN_STRAVA = STRAVA_TOKEN_PROXY_BASE + "/strava/oauth/token";




class Environnement
{
public:
    Environnement();



    /// Public method -----------------------
    static QString getVersion();
    static QString getDateBuilded();


    static QString getURLStravaAuthorize(const QString &redirectUri);
    /// Build the Intervals.icu OAuth2 authorization URL.
    /// @param state        Per-request CSRF token; pass an empty string to omit.
    /// @param redirectUri  Redirect URI for this request (localhost loopback on
    ///                     desktop, GitHub Pages callback page on WASM).
    static QString getURLIntervalsIcuAuthorize(const QString &state,
                                               const QString &redirectUri);
    /// Build the Intervals.icu OAuth2 authorization URL for the WASM popup flow.
    /// Uses a GitHub Pages callback page as the redirect_uri so the popup can
    /// post the authorization code back to the main WASM window via postMessage.
    /// @param state  Per-request CSRF token; pass an empty string to omit.
    static QString getURLIntervalsIcuAuthorizeWasm(const QString &state = QString());
    /// Return the WASM-specific OAuth2 redirect_uri (GitHub Pages callback page).
    /// Must be registered as an allowed redirect URI with Intervals.icu OAuth client 259.
    static QString getWasmOAuthRedirectUri();
    static QString getUrlIntervalsIcuRegister();
    /// Return the Intervals.icu OAuth2 client_id.
    /// Checks CredentialStore("intervals_icu_app","client_id") first; falls back
    /// to the build-time constant (INTERVALS_OAUTH_CLIENT_ID / "259").
    static QString getIntervalsIcuClientId();

};

#endif // ENVIRONNEMENT_H
