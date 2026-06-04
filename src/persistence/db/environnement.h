#ifndef ENVIRONNEMENT_H
#define ENVIRONNEMENT_H

#include <QtCore>


//// TO CHANGE DEV TO PROD
const static QString current_env = "prod";
//const static QString current_env = "dev";
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
/// Required scopes and their purpose:
///   ACTIVITY:READ   — fetch completed activities for history and data sync
///   WELLNESS:READ   — fetch wellness metrics (weight, HRV, sleep, etc.)
///   CALENDAR:READ   — fetch planned workouts from the athlete calendar
///   SETTINGS:READ   — read training zones (HR zones, power zones)
///
/// The redirect_uri is appended at runtime by Environnement::getURLIntervalsIcuAuthorize().
/// Note: client_id is substituted at runtime via getIntervalsIcuClientId() so the
/// compile-time constant here is only a documentation reference; the actual URL is
/// built dynamically in getURLIntervalsIcuAuthorize().
const static QString urlIntervalsIcuOAuthAuthorize(
    "https://intervals.icu/oauth/authorize?"
    "response_type=code"
    "&scope=ACTIVITY:READ+WELLNESS:READ+CALENDAR:READ+SETTINGS:READ");

/// Intervals.icu OAuth2 client credentials.
/// client_id and client_secret are injected at build time via environment variables
/// INTERVALS_OAUTH_CLIENT_ID and INTERVALS_OAUTH_CLIENT_SECRET (see PowerVelo.pro).
/// The macros default to "259" (existing public client) and "" (no secret) when
/// the environment variables are not set.
#ifndef INTERVALS_OAUTH_CLIENT_ID
#define INTERVALS_OAUTH_CLIENT_ID "259"
#endif
#ifndef INTERVALS_OAUTH_CLIENT_SECRET
#define INTERVALS_OAUTH_CLIENT_SECRET ""
#endif
const static QString CLIENT_ID_ICV     = QStringLiteral(INTERVALS_OAUTH_CLIENT_ID);
const static QString CLIENT_SECRET_ICV = QStringLiteral(INTERVALS_OAUTH_CLIENT_SECRET);
const static QString URL_TOKEN_ICV  = "https://intervals.icu/oauth/token";

/// Sentinel athlete ID meaning "the currently authenticated OAuth2 user".
/// Pass this to Bearer-token API calls when the real athlete ID is not yet known.
const static QString INTERVALS_ICU_CURRENT_USER_ID = "0";

/// Registration page for users who do not yet have an Intervals.icu account.
const static QString urlIntervalsIcuRegister = "https://intervals.icu/register";



const static QString dev = "http://localhost/index.php/";
const static QString prod = "https://maximumtrainer.com/";
const static QString indexPage = "index.php/";



/// GoogleMap
const static QString urlGoogleMapEn = "google-map";


const static QString urlStravaAuthorize("https://www.strava.com/oauth/authorize?"
                                        "client_id=7252"
                                        "&response_type=code"
                                        "&scope=write"
                                        "&state=mystate"
                                        "&approval_prompt=force");



///TrainingPeaks - These are the API URLs that you will use for development:
//https://oauth.trainingpeaks.com/oauth/token
//https://oauth.trainingpeaks.com/oauth/authorize
//https://api.trainingpeaks.com/v1/file

const static QString urlTrainingPeaksAuthorize("https://oauth.trainingpeaks.com/oauth/authorize?"
                                               "client_id=maximumtrainer"
                                               "&response_type=code"
                                               "&scope=file:write");

const static QString CLIENT_ID_TP = "maximumtrainer";
// CLIENT_SECRET_TP is injected at build time via the TP_CLIENT_SECRET env var
// (see PowerVelo.pro).  The macro expands to an empty string when the secret
// has not been configured, which disables the token-refresh flow.
#ifndef TP_CLIENT_SECRET
#define TP_CLIENT_SECRET ""
#endif
const static QString CLIENT_SECRET_TP = QStringLiteral(TP_CLIENT_SECRET);
const static QString URL_TOKEN_TP = "https://oauth.trainingpeaks.com/oauth/token/";
const static QString URL_POST_FILE_TP = "https://api.trainingpeaks.com/v1/file/";



/// Login
const static QString urlLoginEn = "login/insideMT";

/// Profile
const static QString urlProfilEnOutsideMt = "myprofile";

/// Choose-Subscription
const static QString urlChooseSubEn = "choose-subscription";

/// News
const static QString urlNewsEn = "news";

/// Zones
const static QString urlZonesEn = "training-zones";

/// Studio
const static QString urlStudioEn = "studio";

/// Achievement
const static QString urlAchievEn = "achievement/insideMT";

/// Settings
const static QString urlSettingsEn = "settings";

/// Workout
const static QString urlWorkoutEn = "workouts";

/// Workout-creator
const static QString urlWorkoutCreatorEn = "workout-creator";

/// Course-creator
const static QString urlCourseCreatorEn = "course-creator";

/// Training-Plans
const static QString urlPlanEn = "training-plans/insideMT";

/// Help
const static QString urlSupportEn = "support/insideMT";

/// Download
const static QString urlDownloadEn = "download-mt";





class Environnement
{
public:
    Environnement();



    /// Public method -----------------------
    static QString getURLEnvironnement();
    static QString getURLEnvironnementWS();
    static QString getVersion();
    static QString getDateBuilded();


    static QString getURLGoogleMap();
    static QString getURLStravaAuthorize();
    static QString getURLTrainingPeaksAuthorize();
    /// Build the Intervals.icu OAuth2 authorization URL (desktop — redirect to maximumtrainer.com backend).
    /// @param state  Per-request CSRF token; pass an empty string to omit.
    static QString getURLIntervalsIcuAuthorize(const QString &state = QString());
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
    /// Return the Intervals.icu OAuth2 client_secret.
    /// Checks CredentialStore("intervals_icu_app","client_secret") first; falls back
    /// to the build-time constant (INTERVALS_OAUTH_CLIENT_SECRET / "").
    static QString getIntervalsIcuClientSecret();


    static QString getUrlLogin();
    static QString getUrlProfileOutsideMt();
    static QString getUrlChooseSub();
    static QString getUrlNews();

    static QString getUrlZones();
    static QString getUrlStudio();
    static QString getUrlAchievement();
    static QString getUrlSettings();

    static QString getUrlWorkout();
    static QString getUrlWorkoutCreator();
    static QString getUrlCourseCreator();
    static QString getUrlDownload();
    static QString getUrlPlans();

    static QString getUrlSupport();

};

#endif // ENVIRONNEMENT_H
