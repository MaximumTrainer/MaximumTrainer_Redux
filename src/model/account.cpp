#include "account.h"
#include "credential_store.h"
#include "config_defaults.h"
#include "setting_keys.h"
#include "startup_trace.h"


Account::~Account() {
}


Account::Account(QObject *parent) : QObject(parent)  {

    // -------------- Set Default values
    id = -1;
    subscription_type_id = 1;

    email = "email";
    password = "pw";
    session_mt_id = "session_mt_id";
    session_mt_expire = QDateTime::currentDateTime().addDays(1);

    first_name = "first_name";
    last_name = "last_name";
    display_name = "display_name";

    FTP = ConfigDefaults::FTP;
    LTHR = ConfigDefaults::LTHR;
    weight_kg = ConfigDefaults::WeightKg;
    weight_in_kg = true;

    bike_weight_kg = 9;


    userCda = 0.35;

    namespace Keys = SettingKeys::Account;
    QSettings settings;
    nb_sec_show_interval        = AppSettings::read(settings, Keys::NbSecShowInterval);
    nb_sec_show_interval_before = AppSettings::read(settings, Keys::NbSecShowIntervalBefore);
    erg_smoothing_duration_s    = AppSettings::read(settings, Keys::ErgSmoothingDurationS);
    // Athlete profile — edited in Preferences and persisted locally so FTP /
    // LTHR / weight survive a restart (and offline use), overriding the
    // hardcoded defaults above. A live server value may still override at runtime.
    FTP       = AppSettings::read(settings, Keys::Ftp);
    LTHR      = AppSettings::read(settings, Keys::Lthr);
    weight_kg = AppSettings::read(settings, Keys::WeightKg);
    intervals_icu_api_key     = AppSettings::read(settings, Keys::IntervalsIcuApiKey);
    intervals_icu_athlete_id  = AppSettings::read(settings, Keys::IntervalsIcuAthleteId);
    intervals_icu_auto_upload = AppSettings::read(settings, Keys::IntervalsIcuAutoUpload);
    // OAuth2 tokens — loaded from the platform credential store (encrypted).
    // Migration: if the CredentialStore is empty but an old plain-QSettings value
    // exists (written by a previous version), copy it to CredentialStore and
    // remove the plain-text entry so the token is protected going forward.
    {
        StartupStep step("Account: CredentialStore load (intervals_icu)");
        intervals_icu_access_token  = CredentialStore::load("intervals_icu", "access_token");
    }
    if (intervals_icu_access_token.isEmpty()) {
        const QString legacy = AppSettings::read(settings, Keys::LegacyIntervalsIcuAccessToken);
        if (!legacy.isEmpty()) {
            intervals_icu_access_token = legacy;
            CredentialStore::store("intervals_icu", "access_token", legacy);
            AppSettings::remove(settings, Keys::LegacyIntervalsIcuAccessToken);
        }
    }
    // Sensor dropout auto-pause
    sensor_dropout_enabled    = AppSettings::read(settings, Keys::SensorDropoutEnabled);
    sensor_dropout_timeout_s  = qBound(2, AppSettings::read(settings, Keys::SensorDropoutTimeoutS), 30);
    battery_warning_threshold = qBound(5, AppSettings::read(settings, Keys::BatteryWarningThreshold), 50);

    // Load encrypted third-party service credentials from the platform credential store.
    {
        StartupStep step("Account: CredentialStore load (strava)");
        strava_access_token         = CredentialStore::load("strava",        "access_token");
        strava_refresh_token        = CredentialStore::load("strava",        "refresh_token");
    }


    // -----------------------------------  Settings ----------------------------------------------------------------------
    nb_user_studio = 3;
    enable_studio_mode = false;
    use_pm_for_cadence = false;
    use_pm_for_speed = false;


    force_workout_window_on_top = false;
    show_included_workout = true;
    rowing_mode = false;
    distance_in_km = true;
    strava_auto_upload = false;
    strava_token_expires_at = 0;
    // intervals_icu_auto_upload is loaded from QSettings above; don't reset it here
    control_trainer_resistance = true;
    virtual_shifting = false;   // opt-in; off keeps real-gear / free-ride behaviour
    use_zwift_click  = false;   // opt-in; read the Zwift Click v2 right controller (direct BLE)
    // Clamp the loaded value to the valid range (0–30 s)
    erg_smoothing_duration_s = qBound(0, erg_smoothing_duration_s, 30);
    /* ----- */

    last_index_selected_config_workout = 0;
    last_tab_sub_config_selected = 0;
    tab_display[0] = "Timer";
    tab_display[1] = "Power";
    tab_display[2] = "Cadence";
    tab_display[3] = "PowerBal";
    tab_display[4] = "Hr";
    tab_display[5] = "Speed";
    tab_display[6] = "InfoWorkout";
    tab_display[7] = "Oxygen";


    start_trigger = 0;
    value_cadence_start = 40;
    value_power_start = 120;
    value_speed_start = 20;


    show_hr_widget = true;
    show_power_widget = true;
    show_power_balance_widget = true;
    show_cadence_widget = true;
    show_speed_widget = true;
    show_calories_widget = true;
    show_oxygen_widget = true;
    show_trainer_speed = true;

    display_hr = 1;
    display_power = 2;
    display_power_balance = 1;
    display_cadence = 1;
    display_video = 0;

    show_timer_on_top = false;
    show_interval_remaining = true;
    show_workout_remaining = false;
    show_elapsed = true;
    show_current_target = true;

    offset_power = 0;


    show_seperator_interval = true;
    show_grid = false;
    show_hr_target = true;
    show_power_target = true;
    show_cadence_target = true;
    show_speed_target = true;
    show_hr_curve = true;
    show_power_curve = true;
    show_cadence_curve = true;
    show_speed_curve = true;

    /* ----- */
    sound_player_vol = 100;
    enable_sound = true;
    sound_interval = true;
    sound_pause_resume_workout = true;
    sound_achievement = true;
    sound_end_workout = true;

    sound_alert_power_under_target = false;
    sound_alert_power_above_target = false;
    sound_alert_cadence_under_target = false;
    sound_alert_cadence_above_target = false;

    interval_summary_enabled    = true;
    interval_summary_duration_s = 5;
    interval_summary_enabled    = AppSettings::read(settings, Keys::IntervalSummaryEnabled);
    interval_summary_duration_s = qBound(2, AppSettings::read(settings, Keys::IntervalSummaryDurationS), 15);
    app_theme = qBound(0, AppSettings::read(settings, Keys::AppTheme), 2);

    // One-time migration: force the Dark theme on the first launch of the
    // release that introduces theming, to showcase the feature. The flag
    // ensures this runs exactly once — afterwards the user's own choice
    // (changed in Preferences) is always respected. AppTheme::Mode: Dark=1.
    if (!AppSettings::read(settings, Keys::ForcedDarkDefaultApplied)) {
        app_theme = 1; // Dark
        AppSettings::write(settings, Keys::AppTheme, app_theme);
        AppSettings::write(settings, Keys::ForcedDarkDefaultApplied, true);
    }

    strava_token_expires_at = AppSettings::readWithFallback(settings, Keys::StravaTokenExpiresAt,
                                                            SettingKeys::Legacy::DisplayPrefsStravaTokenExpiresAt);

    //-------------------------- not in DB ----------------------
    isOffline = false;
#ifdef Q_OS_WIN32
    os = "win";
#endif
#ifdef Q_OS_MAC
    os = "mac";
#endif

    email_clean = "user1";
    hashWorkoutDone = QSet<QString>();

    // Override the display/sound defaults above with any locally-persisted
    // values (the server-side putAccount endpoint is defunct, so these are the
    // authoritative store now).
    loadDisplayPrefs();
}


void Account::saveNbSecShowInterval(int nbSec) {

    nb_sec_show_interval = nbSec;

    AppSettings::write(SettingKeys::Account::NbSecShowInterval, nb_sec_show_interval);
}

void Account::saveNbSecShowIntervalBefore(int nbSec) {

    nb_sec_show_interval_before = nbSec;

    AppSettings::write(SettingKeys::Account::NbSecShowIntervalBefore, nb_sec_show_interval_before);
}

void Account::saveErgSmoothingDuration(int seconds)
{
    erg_smoothing_duration_s = qBound(0, seconds, 30);

    AppSettings::write(SettingKeys::Account::ErgSmoothingDurationS, erg_smoothing_duration_s);
}

void Account::saveProfileFields(int ftp, int lthr, double weightKg)
{
    FTP       = ftp;
    LTHR      = lthr;
    weight_kg = weightKg;

    QSettings settings;
    AppSettings::write(settings, SettingKeys::Account::Ftp,      FTP);
    AppSettings::write(settings, SettingKeys::Account::Lthr,     LTHR);
    AppSettings::write(settings, SettingKeys::Account::WeightKg, weight_kg);
}

// Group/key prefix for the locally-persisted display & sound preferences.
// Kept distinct from the legacy server fields so it is self-contained.
void Account::loadDisplayPrefs() {

    QSettings settings;
    settings.beginGroup("displayPrefs");

    // Widget visibility
    show_hr_widget            = settings.value("show_hr_widget",            show_hr_widget).toBool();
    show_power_widget         = settings.value("show_power_widget",         show_power_widget).toBool();
    show_power_balance_widget = settings.value("show_power_balance_widget", show_power_balance_widget).toBool();
    show_cadence_widget       = settings.value("show_cadence_widget",       show_cadence_widget).toBool();
    show_speed_widget         = settings.value("show_speed_widget",         show_speed_widget).toBool();
    show_calories_widget      = settings.value("show_calories_widget",      show_calories_widget).toBool();
    show_oxygen_widget        = settings.value("show_oxygen_widget",        show_oxygen_widget).toBool();
    show_trainer_speed        = settings.value("show_trainer_speed",        show_trainer_speed).toBool();

    // Per-metric display mode
    display_hr            = settings.value("display_hr",            display_hr).toInt();
    display_power         = settings.value("display_power",         display_power).toInt();
    display_power_balance = settings.value("display_power_balance", display_power_balance).toInt();
    display_cadence       = settings.value("display_cadence",       display_cadence).toInt();

    // The "Minimalist" display mode (value 0) was removed; migrate any saved
    // Minimalist preference to Detailed (value 1). Idempotent: the editor only
    // ever writes 1+ now, so 0 can only be a legacy value.
    if (display_hr == 0)      display_hr = 1;
    if (display_power == 0)   display_power = 1;
    if (display_cadence == 0) display_cadence = 1;
    display_video         = settings.value("display_video",         display_video).toInt();
    offset_power          = settings.value("offset_power",          offset_power).toInt();

    // Start-workout trigger and its per-mode threshold values.
    start_trigger       = settings.value("start_trigger",       start_trigger).toInt();
    value_cadence_start = settings.value("value_cadence_start", value_cadence_start).toInt();
    value_power_start   = settings.value("value_power_start",   value_power_start).toInt();
    value_speed_start   = settings.value("value_speed_start",   value_speed_start).toInt();

    // The last-selected config tab/sub-tab.
    last_index_selected_config_workout = settings.value("last_index_selected_config_workout", last_index_selected_config_workout).toInt();
    last_tab_sub_config_selected       = settings.value("last_tab_sub_config_selected",       last_tab_sub_config_selected).toInt();

    // General / trainer / pairing / upload preferences.
    control_trainer_resistance   = settings.value("control_trainer_resistance",   control_trainer_resistance).toBool();
    virtual_shifting             = settings.value("virtual_shifting",             virtual_shifting).toBool();
    use_zwift_click              = settings.value("use_zwift_click",              use_zwift_click).toBool();
    enable_studio_mode           = settings.value("enable_studio_mode",           enable_studio_mode).toBool();
    distance_in_km               = settings.value("distance_in_km",               distance_in_km).toBool();
    force_workout_window_on_top  = settings.value("force_workout_window_on_top",  force_workout_window_on_top).toBool();
    strava_auto_upload           = settings.value("strava_auto_upload",           strava_auto_upload).toBool();

    // Studio / power-meter / virtual-speed / included-content preferences.
    nb_user_studio        = settings.value("nb_user_studio",        nb_user_studio).toInt();
    use_pm_for_cadence    = settings.value("use_pm_for_cadence",    use_pm_for_cadence).toBool();
    use_pm_for_speed      = settings.value("use_pm_for_speed",      use_pm_for_speed).toBool();
    show_included_workout = settings.value("show_included_workout", show_included_workout).toBool();
    rowing_mode = settings.value("rowing_mode", rowing_mode).toBool();

    // Timer display
    show_timer_on_top       = settings.value("show_timer_on_top",       show_timer_on_top).toBool();
    show_interval_remaining = settings.value("show_interval_remaining", show_interval_remaining).toBool();
    show_workout_remaining  = settings.value("show_workout_remaining",  show_workout_remaining).toBool();
    show_elapsed            = settings.value("show_elapsed",            show_elapsed).toBool();
    show_current_target     = settings.value("show_current_target",     show_current_target).toBool();

    // Plot target/curve toggles
    show_seperator_interval = settings.value("show_seperator_interval", show_seperator_interval).toBool();
    show_grid               = settings.value("show_grid",               show_grid).toBool();
    show_hr_target          = settings.value("show_hr_target",          show_hr_target).toBool();
    show_power_target       = settings.value("show_power_target",       show_power_target).toBool();
    show_cadence_target     = settings.value("show_cadence_target",     show_cadence_target).toBool();
    show_speed_target       = settings.value("show_speed_target",       show_speed_target).toBool();
    show_hr_curve           = settings.value("show_hr_curve",           show_hr_curve).toBool();
    show_power_curve        = settings.value("show_power_curve",        show_power_curve).toBool();
    show_cadence_curve      = settings.value("show_cadence_curve",      show_cadence_curve).toBool();
    show_speed_curve        = settings.value("show_speed_curve",        show_speed_curve).toBool();

    // Sound
    sound_player_vol               = settings.value("sound_player_vol",               sound_player_vol).toInt();
    enable_sound                   = settings.value("enable_sound",                   enable_sound).toBool();
    sound_interval                 = settings.value("sound_interval",                 sound_interval).toBool();
    sound_pause_resume_workout     = settings.value("sound_pause_resume_workout",     sound_pause_resume_workout).toBool();
    sound_achievement              = settings.value("sound_achievement",              sound_achievement).toBool();
    sound_end_workout              = settings.value("sound_end_workout",              sound_end_workout).toBool();
    sound_alert_power_under_target = settings.value("sound_alert_power_under_target", sound_alert_power_under_target).toBool();
    sound_alert_power_above_target = settings.value("sound_alert_power_above_target", sound_alert_power_above_target).toBool();
    sound_alert_cadence_under_target = settings.value("sound_alert_cadence_under_target", sound_alert_cadence_under_target).toBool();
    sound_alert_cadence_above_target = settings.value("sound_alert_cadence_above_target", sound_alert_cadence_above_target).toBool();

    settings.endGroup();
}

void Account::saveDisplayPrefs() {

    QSettings settings;
    settings.beginGroup("displayPrefs");

    settings.setValue("show_hr_widget",            show_hr_widget);
    settings.setValue("show_power_widget",         show_power_widget);
    settings.setValue("show_power_balance_widget", show_power_balance_widget);
    settings.setValue("show_cadence_widget",       show_cadence_widget);
    settings.setValue("show_speed_widget",         show_speed_widget);
    settings.setValue("show_calories_widget",      show_calories_widget);
    settings.setValue("show_oxygen_widget",        show_oxygen_widget);
    settings.setValue("show_trainer_speed",        show_trainer_speed);

    settings.setValue("display_hr",            display_hr);
    settings.setValue("display_power",         display_power);
    settings.setValue("display_power_balance", display_power_balance);
    settings.setValue("display_cadence",       display_cadence);
    settings.setValue("display_video",         display_video);
    settings.setValue("offset_power",          offset_power);

    // Start-workout trigger and its per-mode threshold values. Formerly loaded
    // from the (now-defunct) server account endpoint; persisted locally so the
    // choice survives a restart.
    settings.setValue("start_trigger",       start_trigger);
    settings.setValue("value_cadence_start", value_cadence_start);
    settings.setValue("value_power_start",   value_power_start);
    settings.setValue("value_speed_start",   value_speed_start);

    // The last-selected config tab/sub-tab.
    settings.setValue("last_index_selected_config_workout", last_index_selected_config_workout);
    settings.setValue("last_tab_sub_config_selected",       last_tab_sub_config_selected);

    // General / trainer / pairing / upload preferences. Formerly server-saved;
    // persisted locally so they survive a restart.
    settings.setValue("control_trainer_resistance",  control_trainer_resistance);
    settings.setValue("virtual_shifting",            virtual_shifting);
    settings.setValue("use_zwift_click",             use_zwift_click);
    settings.setValue("enable_studio_mode",          enable_studio_mode);
    settings.setValue("distance_in_km",              distance_in_km);
    settings.setValue("force_workout_window_on_top", force_workout_window_on_top);
    settings.setValue("strava_auto_upload",          strava_auto_upload);

    // Studio / power-meter / virtual-speed / included-content preferences.
    // Formerly server-saved; persisted locally so they survive a restart.
    settings.setValue("nb_user_studio",        nb_user_studio);
    settings.setValue("use_pm_for_cadence",    use_pm_for_cadence);
    settings.setValue("use_pm_for_speed",      use_pm_for_speed);
    settings.setValue("show_included_workout", show_included_workout);
    settings.setValue("rowing_mode", rowing_mode);

    settings.setValue("show_timer_on_top",       show_timer_on_top);
    settings.setValue("show_interval_remaining", show_interval_remaining);
    settings.setValue("show_workout_remaining",  show_workout_remaining);
    settings.setValue("show_elapsed",            show_elapsed);
    settings.setValue("show_current_target",     show_current_target);

    settings.setValue("show_seperator_interval", show_seperator_interval);
    settings.setValue("show_grid",               show_grid);
    settings.setValue("show_hr_target",          show_hr_target);
    settings.setValue("show_power_target",       show_power_target);
    settings.setValue("show_cadence_target",     show_cadence_target);
    settings.setValue("show_speed_target",       show_speed_target);
    settings.setValue("show_hr_curve",           show_hr_curve);
    settings.setValue("show_power_curve",        show_power_curve);
    settings.setValue("show_cadence_curve",      show_cadence_curve);
    settings.setValue("show_speed_curve",        show_speed_curve);

    settings.setValue("sound_player_vol",               sound_player_vol);
    settings.setValue("enable_sound",                   enable_sound);
    settings.setValue("sound_interval",                 sound_interval);
    settings.setValue("sound_pause_resume_workout",     sound_pause_resume_workout);
    settings.setValue("sound_achievement",              sound_achievement);
    settings.setValue("sound_end_workout",              sound_end_workout);
    settings.setValue("sound_alert_power_under_target", sound_alert_power_under_target);
    settings.setValue("sound_alert_power_above_target", sound_alert_power_above_target);
    settings.setValue("sound_alert_cadence_under_target", sound_alert_cadence_under_target);
    settings.setValue("sound_alert_cadence_above_target", sound_alert_cadence_above_target);

    settings.endGroup();
}

void Account::saveIntervalsIcuCredentials() {

    QSettings settings;
    AppSettings::write(settings, SettingKeys::Account::IntervalsIcuApiKey,     intervals_icu_api_key);
    AppSettings::write(settings, SettingKeys::Account::IntervalsIcuAthleteId,  intervals_icu_athlete_id);
    AppSettings::write(settings, SettingKeys::Account::IntervalsIcuAutoUpload, intervals_icu_auto_upload);

    // OAuth2 tokens — persisted in the platform credential store (encrypted).
    CredentialStore::store("intervals_icu", "access_token",  intervals_icu_access_token);
}

void Account::logout()
{
    // OAuth tokens (encrypted store).
    CredentialStore::remove("intervals_icu", "access_token");
    CredentialStore::remove("intervals_icu", "refresh_token");
    intervals_icu_access_token.clear();

    // Manual API key + athlete id (the athlete id is what auto-logs the app in).
    intervals_icu_api_key.clear();
    intervals_icu_athlete_id.clear();

    QSettings settings;
    AppSettings::remove(settings, SettingKeys::Account::IntervalsIcuApiKey);
    AppSettings::remove(settings, SettingKeys::Account::IntervalsIcuAthleteId);
}

void Account::saveStravaCredentials()
{
    CredentialStore::store("strava", "access_token",  strava_access_token);
    CredentialStore::store("strava", "refresh_token", strava_refresh_token);
    // Persist the expiry too so refresh-if-expired survives a restart.
    AppSettings::write(SettingKeys::Account::StravaTokenExpiresAt, qlonglong(strava_token_expires_at));
}

void Account::saveSensorDropoutSettings()
{
    QSettings settings;
    AppSettings::write(settings, SettingKeys::Account::SensorDropoutEnabled,  sensor_dropout_enabled);
    AppSettings::write(settings, SettingKeys::Account::SensorDropoutTimeoutS, qBound(2, sensor_dropout_timeout_s, 30));
}

void Account::saveBatteryWarningThreshold()
{
    AppSettings::write(SettingKeys::Account::BatteryWarningThreshold, qBound(5, battery_warning_threshold, 50));
}

void Account::saveIntervalSummarySettings()
{
    QSettings settings;
    AppSettings::write(settings, SettingKeys::Account::IntervalSummaryEnabled,   interval_summary_enabled);
    AppSettings::write(settings, SettingKeys::Account::IntervalSummaryDurationS, qBound(2, interval_summary_duration_s, 15));
}

void Account::saveAppTheme()
{
    AppSettings::write(SettingKeys::Account::AppTheme, qBound(0, app_theme, 2));
}

