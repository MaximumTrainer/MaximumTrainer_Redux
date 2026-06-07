#include "account.h"
#include "credential_store.h"
#include <QDebug>


Account::~Account() {
    qDebug() << "Destructor account";
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

    FTP = 150;
    LTHR = 150;
    minutes_rode = 0;
    weight_kg = 70;
    weight_in_kg = true;
    height_cm = 170;

    wheel_circ = 2100;
    bike_weight_kg = 9;
    bike_type = 2;


    userCda = 0.35;

    //-- no more DB settings, move them here later
    QSettings settings;
    settings.beginGroup("account");
    nb_sec_show_interval = settings.value("nb_sec_show_interval", 5 ).toInt();
    nb_sec_show_interval_before = settings.value("nb_sec_show_interval_before", 4 ).toInt();
    erg_smoothing_duration_s = settings.value("erg_smoothing_duration_s", 5).toInt();
    // Athlete profile — edited in Preferences and persisted locally so FTP /
    // LTHR / weight survive a restart (and offline use), overriding the
    // hardcoded defaults above. A live server value may still override at runtime.
    FTP       = settings.value("FTP",       FTP).toInt();
    LTHR      = settings.value("LTHR",      LTHR).toInt();
    weight_kg = settings.value("weight_kg", weight_kg).toDouble();
    intervals_icu_api_key     = settings.value("intervals_icu_api_key", "").toString();
    intervals_icu_athlete_id  = settings.value("intervals_icu_athlete_id", "").toString();
    intervals_icu_auto_upload = settings.value("intervals_icu_auto_upload", false).toBool();
    // OAuth2 tokens — loaded from the platform credential store (encrypted).
    // Migration: if the CredentialStore is empty but an old plain-QSettings value
    // exists (written by a previous version), copy it to CredentialStore and
    // remove the plain-text entry so the token is protected going forward.
    intervals_icu_access_token  = CredentialStore::load("intervals_icu", "access_token");
    intervals_icu_refresh_token = CredentialStore::load("intervals_icu", "refresh_token");
    if (intervals_icu_access_token.isEmpty()) {
        const QString legacy = settings.value("intervals_icu_access_token", "").toString();
        if (!legacy.isEmpty()) {
            intervals_icu_access_token = legacy;
            CredentialStore::store("intervals_icu", "access_token", legacy);
            settings.remove("intervals_icu_access_token");
        }
    }
    if (intervals_icu_refresh_token.isEmpty()) {
        const QString legacy = settings.value("intervals_icu_refresh_token", "").toString();
        if (!legacy.isEmpty()) {
            intervals_icu_refresh_token = legacy;
            CredentialStore::store("intervals_icu", "refresh_token", legacy);
            settings.remove("intervals_icu_refresh_token");
        }
    }
    // Sensor dropout auto-pause
    sensor_dropout_enabled   = settings.value("sensor_dropout_enabled",   true).toBool();
    sensor_dropout_timeout_s = qBound(2, settings.value("sensor_dropout_timeout_s", 5).toInt(), 30);
    battery_warning_threshold = qBound(5, settings.value("battery_warning_threshold", 20).toInt(), 50);
    settings.endGroup();

    // Load encrypted third-party service credentials from the platform credential store.
    strava_access_token         = CredentialStore::load("strava",        "access_token");
    strava_refresh_token        = CredentialStore::load("strava",        "refresh_token");
    training_peaks_access_token  = CredentialStore::load("trainingpeaks", "access_token");
    training_peaks_refresh_token = CredentialStore::load("trainingpeaks", "refresh_token");
    selfloops_user              = CredentialStore::load("selfloops",     "email");
    selfloops_pw                = CredentialStore::load("selfloops",     "password");




    // -----------------------------------  Settings ----------------------------------------------------------------------
    nb_user_studio = 3;
    enable_studio_mode = false;
    use_pm_for_cadence = false;
    use_pm_for_speed = false;


    force_workout_window_on_top = false;
    show_included_workout = true;
    distance_in_km = true;
    strava_private_upload = false;
    training_peaks_public_upload = false;
    // intervals_icu_auto_upload is loaded from QSettings above; don't reset it here
    control_trainer_resistance = true;
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
    use_virtual_speed = true;
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
    font_size_timer = 26;

    averaging_power = 2;
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
    {
        QSettings s;
        s.beginGroup("account");
        interval_summary_enabled    = s.value("interval_summary_enabled",    true).toBool();
        interval_summary_duration_s = qBound(2, s.value("interval_summary_duration_s", 5).toInt(), 15);
        app_theme = qBound(0, s.value("app_theme", 2).toInt(), 2); // default: System

        // One-time migration: force the Dark theme on the first launch of the
        // release that introduces theming, to showcase the feature. The flag
        // ensures this runs exactly once — afterwards the user's own choice
        // (changed in Preferences) is always respected. AppTheme::Mode: Dark=1.
        if (!s.value("forced_dark_default_applied", false).toBool()) {
            app_theme = 1; // Dark
            s.setValue("app_theme", app_theme);
            s.setValue("forced_dark_default_applied", true);
        }
        s.endGroup();
    }

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
    //------------------------------

    // Override the display/sound defaults above with any locally-persisted
    // values (the server-side putAccount endpoint is defunct, so these are the
    // authoritative store now).
    loadDisplayPrefs();
}


void Account::saveNbSecShowInterval(int nbSec) {

    nb_sec_show_interval = nbSec;

    QSettings settings;

    settings.beginGroup("account");
    settings.setValue("nb_sec_show_interval", nb_sec_show_interval);

    settings.endGroup();
}

void Account::saveNbSecShowIntervalBefore(int nbSec) {

    nb_sec_show_interval_before = nbSec;

    QSettings settings;

    settings.beginGroup("account");
    settings.setValue("nb_sec_show_interval_before", nb_sec_show_interval_before);

    settings.endGroup();
}

void Account::saveErgSmoothingDuration(int seconds)
{
    erg_smoothing_duration_s = qBound(0, seconds, 30);

    QSettings settings;
    settings.beginGroup("account");
    settings.setValue("erg_smoothing_duration_s", erg_smoothing_duration_s);
    settings.endGroup();
}

void Account::saveProfileFields(int ftp, int lthr, double weightKg)
{
    FTP       = ftp;
    LTHR      = lthr;
    weight_kg = weightKg;

    QSettings settings;
    settings.beginGroup("account");
    settings.setValue("FTP",       FTP);
    settings.setValue("LTHR",      LTHR);
    settings.setValue("weight_kg", weight_kg);
    settings.endGroup();
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
    display_video         = settings.value("display_video",         display_video).toInt();
    averaging_power       = settings.value("averaging_power",       averaging_power).toInt();
    offset_power          = settings.value("offset_power",          offset_power).toInt();

    // Start-workout trigger and its per-mode threshold values.
    start_trigger       = settings.value("start_trigger",       start_trigger).toInt();
    value_cadence_start = settings.value("value_cadence_start", value_cadence_start).toInt();
    value_power_start   = settings.value("value_power_start",   value_power_start).toInt();
    value_speed_start   = settings.value("value_speed_start",   value_speed_start).toInt();

    // Workout timer font size and the last-selected config tab/sub-tab.
    font_size_timer                    = settings.value("font_size_timer",                    font_size_timer).toInt();
    last_index_selected_config_workout = settings.value("last_index_selected_config_workout", last_index_selected_config_workout).toInt();
    last_tab_sub_config_selected       = settings.value("last_tab_sub_config_selected",       last_tab_sub_config_selected).toInt();

    // General / trainer / pairing / upload preferences.
    control_trainer_resistance   = settings.value("control_trainer_resistance",   control_trainer_resistance).toBool();
    enable_studio_mode           = settings.value("enable_studio_mode",           enable_studio_mode).toBool();
    distance_in_km               = settings.value("distance_in_km",               distance_in_km).toBool();
    force_workout_window_on_top  = settings.value("force_workout_window_on_top",  force_workout_window_on_top).toBool();
    strava_private_upload        = settings.value("strava_private_upload",        strava_private_upload).toBool();
    training_peaks_public_upload = settings.value("training_peaks_public_upload", training_peaks_public_upload).toBool();

    // Studio / power-meter / virtual-speed / included-content preferences.
    nb_user_studio        = settings.value("nb_user_studio",        nb_user_studio).toInt();
    use_pm_for_cadence    = settings.value("use_pm_for_cadence",    use_pm_for_cadence).toBool();
    use_pm_for_speed      = settings.value("use_pm_for_speed",      use_pm_for_speed).toBool();
    use_virtual_speed     = settings.value("use_virtual_speed",     use_virtual_speed).toBool();
    show_included_workout = settings.value("show_included_workout", show_included_workout).toBool();

    // Timer display
    show_timer_on_top       = settings.value("show_timer_on_top",       show_timer_on_top).toBool();
    show_interval_remaining = settings.value("show_interval_remaining", show_interval_remaining).toBool();
    show_workout_remaining  = settings.value("show_workout_remaining",  show_workout_remaining).toBool();
    show_elapsed            = settings.value("show_elapsed",            show_elapsed).toBool();

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
    settings.setValue("averaging_power",       averaging_power);
    settings.setValue("offset_power",          offset_power);

    // Start-workout trigger and its per-mode threshold values. Formerly loaded
    // from the (now-defunct) server account endpoint; persisted locally so the
    // choice survives a restart.
    settings.setValue("start_trigger",       start_trigger);
    settings.setValue("value_cadence_start", value_cadence_start);
    settings.setValue("value_power_start",   value_power_start);
    settings.setValue("value_speed_start",   value_speed_start);

    // Workout timer font size and the last-selected config tab/sub-tab.
    settings.setValue("font_size_timer",                    font_size_timer);
    settings.setValue("last_index_selected_config_workout", last_index_selected_config_workout);
    settings.setValue("last_tab_sub_config_selected",       last_tab_sub_config_selected);

    // General / trainer / pairing / upload preferences. Formerly server-saved;
    // persisted locally so they survive a restart.
    settings.setValue("control_trainer_resistance",  control_trainer_resistance);
    settings.setValue("enable_studio_mode",          enable_studio_mode);
    settings.setValue("distance_in_km",              distance_in_km);
    settings.setValue("force_workout_window_on_top", force_workout_window_on_top);
    settings.setValue("strava_private_upload",       strava_private_upload);
    settings.setValue("training_peaks_public_upload", training_peaks_public_upload);

    // Studio / power-meter / virtual-speed / included-content preferences.
    // Formerly server-saved; persisted locally so they survive a restart.
    settings.setValue("nb_user_studio",        nb_user_studio);
    settings.setValue("use_pm_for_cadence",    use_pm_for_cadence);
    settings.setValue("use_pm_for_speed",      use_pm_for_speed);
    settings.setValue("use_virtual_speed",     use_virtual_speed);
    settings.setValue("show_included_workout", show_included_workout);

    settings.setValue("show_timer_on_top",       show_timer_on_top);
    settings.setValue("show_interval_remaining", show_interval_remaining);
    settings.setValue("show_workout_remaining",  show_workout_remaining);
    settings.setValue("show_elapsed",            show_elapsed);

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

    settings.beginGroup("account");
    settings.setValue("intervals_icu_api_key",    intervals_icu_api_key);
    settings.setValue("intervals_icu_athlete_id", intervals_icu_athlete_id);
    settings.setValue("intervals_icu_auto_upload", intervals_icu_auto_upload);
    settings.endGroup();

    // OAuth2 tokens — persisted in the platform credential store (encrypted).
    CredentialStore::store("intervals_icu", "access_token",  intervals_icu_access_token);
    CredentialStore::store("intervals_icu", "refresh_token", intervals_icu_refresh_token);
}

void Account::saveStravaCredentials()
{
    CredentialStore::store("strava", "access_token",  strava_access_token);
    CredentialStore::store("strava", "refresh_token", strava_refresh_token);
}

void Account::saveTrainingPeaksCredentials()
{
    CredentialStore::store("trainingpeaks", "access_token",  training_peaks_access_token);
    CredentialStore::store("trainingpeaks", "refresh_token", training_peaks_refresh_token);
}

void Account::saveSelfloopsCredentials()
{
    CredentialStore::store("selfloops", "email",    selfloops_user);
    CredentialStore::store("selfloops", "password", selfloops_pw);
}

void Account::saveSensorDropoutSettings()
{
    QSettings settings;
    settings.beginGroup("account");
    settings.setValue("sensor_dropout_enabled",   sensor_dropout_enabled);
    settings.setValue("sensor_dropout_timeout_s", qBound(2, sensor_dropout_timeout_s, 30));
    settings.endGroup();
}

void Account::saveBatteryWarningThreshold()
{
    QSettings settings;
    settings.beginGroup("account");
    settings.setValue("battery_warning_threshold", qBound(5, battery_warning_threshold, 50));
    settings.endGroup();
}

void Account::saveIntervalSummarySettings()
{
    QSettings settings;
    settings.beginGroup("account");
    settings.setValue("interval_summary_enabled",    interval_summary_enabled);
    settings.setValue("interval_summary_duration_s", qBound(2, interval_summary_duration_s, 15));
    settings.endGroup();
}

void Account::saveAppTheme()
{
    QSettings s;
    s.beginGroup("account");
    s.setValue("app_theme", qBound(0, app_theme, 2));
    s.endGroup();
}




