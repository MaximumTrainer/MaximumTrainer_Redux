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
    AppSettings::remove(settings, Keys::IntervalsIcuApiKey);  // retired manual API key
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

    erg_smoothing_duration_s = qBound(0, erg_smoothing_duration_s, 30);

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

    // Display, sound and trainer preferences (defaults in SettingKeys::DisplayPrefs).
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

    namespace Keys = SettingKeys::DisplayPrefs;
    QSettings settings;

    // Riders from before virtual shifting and button start became the defaults
    // keep the old ones. Written at once, so a new install stays new next launch.
    const bool existingInstall = AppSettings::contains(settings, SettingKeys::MainWindow::Size)
                              || AppSettings::contains(settings, Keys::ShowHrWidget);
    if (!AppSettings::contains(settings, Keys::VirtualShifting))
        AppSettings::write(settings, Keys::VirtualShifting, existingInstall ? false : Keys::VirtualShifting.defaultValue);
    if (!AppSettings::contains(settings, Keys::StartTrigger))
        AppSettings::write(settings, Keys::StartTrigger, existingInstall ? 0 : Keys::StartTrigger.defaultValue);

    // Widget visibility
    show_hr_widget            = AppSettings::read(settings, Keys::ShowHrWidget);
    show_power_widget         = AppSettings::read(settings, Keys::ShowPowerWidget);
    show_power_balance_widget = AppSettings::read(settings, Keys::ShowPowerBalanceWidget);
    show_cadence_widget       = AppSettings::read(settings, Keys::ShowCadenceWidget);
    show_speed_widget         = AppSettings::read(settings, Keys::ShowSpeedWidget);
    show_calories_widget      = AppSettings::read(settings, Keys::ShowCaloriesWidget);
    show_oxygen_widget        = AppSettings::read(settings, Keys::ShowOxygenWidget);
    show_trainer_speed        = AppSettings::read(settings, Keys::ShowTrainerSpeed);

    // Per-metric display mode
    display_hr            = AppSettings::read(settings, Keys::DisplayHr);
    display_power         = AppSettings::read(settings, Keys::DisplayPower);
    display_power_balance = AppSettings::read(settings, Keys::DisplayPowerBalance);
    display_cadence       = AppSettings::read(settings, Keys::DisplayCadence);

    // The "Minimalist" display mode (value 0) was removed; migrate any saved
    // Minimalist preference to Detailed (value 1). Idempotent: the editor only
    // ever writes 1+ now, so 0 can only be a legacy value.
    if (display_hr == 0)      display_hr = 1;
    if (display_power == 0)   display_power = 1;
    if (display_cadence == 0) display_cadence = 1;
    display_video         = AppSettings::read(settings, Keys::DisplayVideo);
    AppSettings::remove(settings, Keys::OffsetPower);  // retired power offset

    // Start-workout trigger and its per-mode threshold values.
    start_trigger       = AppSettings::read(settings, Keys::StartTrigger);
    value_cadence_start = AppSettings::read(settings, Keys::ValueCadenceStart);
    value_power_start   = AppSettings::read(settings, Keys::ValuePowerStart);
    value_speed_start   = AppSettings::read(settings, Keys::ValueSpeedStart);

    // The last-selected config page.
    last_index_selected_config_workout = AppSettings::read(settings, Keys::LastIndexSelectedConfigWorkout);
    AppSettings::remove(settings, Keys::LastTabSubConfigSelected);  // settings dialog has no sub-tabs

    // General / trainer / pairing / upload preferences.
    control_trainer_resistance   = AppSettings::read(settings, Keys::ControlTrainerResistance);
    virtual_shifting             = AppSettings::read(settings, Keys::VirtualShifting);
    use_zwift_click              = AppSettings::read(settings, Keys::UseZwiftClick);
    use_simulated_sensors        = AppSettings::read(settings, Keys::UseSimulatedSensors);
    distance_in_km               = AppSettings::read(settings, Keys::DistanceInKm);
    force_workout_window_on_top  = AppSettings::read(settings, Keys::ForceWorkoutWindowOnTop);
    strava_auto_upload           = AppSettings::read(settings, Keys::StravaAutoUpload);

    // Power-meter / virtual-speed / included-content preferences.
    use_pm_for_cadence    = AppSettings::read(settings, Keys::UsePmForCadence);
    use_pm_for_speed      = AppSettings::read(settings, Keys::UsePmForSpeed);
    show_included_workout = AppSettings::read(settings, Keys::ShowIncludedWorkout);
    rowing_mode = AppSettings::read(settings, Keys::RowingMode);

    // Timer display
    AppSettings::remove(settings, Keys::ShowTimerOnTop);  // timers are always on the top bar
    AppSettings::remove(settings, SettingKeys::WorkoutQueue::RetiredGroup);  // workout queue removed
    AppSettings::remove(settings, SettingKeys::PlanAdherence::RetiredGroup);  // Plan Adherence tab removed
    show_interval_remaining = AppSettings::read(settings, Keys::ShowIntervalRemaining);
    show_workout_remaining  = AppSettings::read(settings, Keys::ShowWorkoutRemaining);
    show_elapsed            = AppSettings::read(settings, Keys::ShowElapsed);
    show_current_target     = AppSettings::read(settings, Keys::ShowCurrentTarget);

    // Plot target/curve toggles
    show_seperator_interval = AppSettings::read(settings, Keys::ShowSeperatorInterval);
    show_grid               = AppSettings::read(settings, Keys::ShowGrid);
    show_hr_target          = AppSettings::read(settings, Keys::ShowHrTarget);
    show_power_target       = AppSettings::read(settings, Keys::ShowPowerTarget);
    show_cadence_target     = AppSettings::read(settings, Keys::ShowCadenceTarget);
    show_speed_target       = AppSettings::read(settings, Keys::ShowSpeedTarget);
    show_hr_curve           = AppSettings::read(settings, Keys::ShowHrCurve);
    show_power_curve        = AppSettings::read(settings, Keys::ShowPowerCurve);
    show_cadence_curve      = AppSettings::read(settings, Keys::ShowCadenceCurve);
    show_speed_curve        = AppSettings::read(settings, Keys::ShowSpeedCurve);

    // Sound
    sound_player_vol               = AppSettings::read(settings, Keys::SoundPlayerVol);
    enable_sound                   = AppSettings::read(settings, Keys::EnableSound);
    sound_interval                 = AppSettings::read(settings, Keys::SoundInterval);
    sound_pause_resume_workout     = AppSettings::read(settings, Keys::SoundPauseResumeWorkout);
    sound_achievement              = AppSettings::read(settings, Keys::SoundAchievement);
    sound_end_workout              = AppSettings::read(settings, Keys::SoundEndWorkout);
    sound_alert_power_under_target = AppSettings::read(settings, Keys::SoundAlertPowerUnderTarget);
    sound_alert_power_above_target = AppSettings::read(settings, Keys::SoundAlertPowerAboveTarget);
    sound_alert_cadence_under_target = AppSettings::read(settings, Keys::SoundAlertCadenceUnderTarget);
    sound_alert_cadence_above_target = AppSettings::read(settings, Keys::SoundAlertCadenceAboveTarget);

}

void Account::saveDisplayPrefs() {

    namespace Keys = SettingKeys::DisplayPrefs;
    QSettings settings;

    AppSettings::write(settings, Keys::ShowHrWidget, show_hr_widget);
    AppSettings::write(settings, Keys::ShowPowerWidget, show_power_widget);
    AppSettings::write(settings, Keys::ShowPowerBalanceWidget, show_power_balance_widget);
    AppSettings::write(settings, Keys::ShowCadenceWidget, show_cadence_widget);
    AppSettings::write(settings, Keys::ShowSpeedWidget, show_speed_widget);
    AppSettings::write(settings, Keys::ShowCaloriesWidget, show_calories_widget);
    AppSettings::write(settings, Keys::ShowOxygenWidget, show_oxygen_widget);
    AppSettings::write(settings, Keys::ShowTrainerSpeed, show_trainer_speed);

    AppSettings::write(settings, Keys::DisplayHr, display_hr);
    AppSettings::write(settings, Keys::DisplayPower, display_power);
    AppSettings::write(settings, Keys::DisplayPowerBalance, display_power_balance);
    AppSettings::write(settings, Keys::DisplayCadence, display_cadence);
    AppSettings::write(settings, Keys::DisplayVideo, display_video);

    // Start-workout trigger and its per-mode threshold values. Formerly loaded
    // from the (now-defunct) server account endpoint; persisted locally so the
    // choice survives a restart.
    AppSettings::write(settings, Keys::StartTrigger, start_trigger);
    AppSettings::write(settings, Keys::ValueCadenceStart, value_cadence_start);
    AppSettings::write(settings, Keys::ValuePowerStart, value_power_start);
    AppSettings::write(settings, Keys::ValueSpeedStart, value_speed_start);

    // The last-selected config page.
    AppSettings::write(settings, Keys::LastIndexSelectedConfigWorkout, last_index_selected_config_workout);

    // General / trainer / pairing / upload preferences. Formerly server-saved;
    // persisted locally so they survive a restart.
    AppSettings::write(settings, Keys::ControlTrainerResistance, control_trainer_resistance);
    AppSettings::write(settings, Keys::VirtualShifting, virtual_shifting);
    AppSettings::write(settings, Keys::UseZwiftClick, use_zwift_click);
    AppSettings::write(settings, Keys::UseSimulatedSensors, use_simulated_sensors);
    AppSettings::write(settings, Keys::DistanceInKm, distance_in_km);
    AppSettings::write(settings, Keys::ForceWorkoutWindowOnTop, force_workout_window_on_top);
    AppSettings::write(settings, Keys::StravaAutoUpload, strava_auto_upload);

    // Power-meter / virtual-speed / included-content preferences.
    // Formerly server-saved; persisted locally so they survive a restart.
    AppSettings::write(settings, Keys::UsePmForCadence, use_pm_for_cadence);
    AppSettings::write(settings, Keys::UsePmForSpeed, use_pm_for_speed);
    AppSettings::write(settings, Keys::ShowIncludedWorkout, show_included_workout);
    AppSettings::write(settings, Keys::RowingMode, rowing_mode);

    AppSettings::write(settings, Keys::ShowIntervalRemaining, show_interval_remaining);
    AppSettings::write(settings, Keys::ShowWorkoutRemaining, show_workout_remaining);
    AppSettings::write(settings, Keys::ShowElapsed, show_elapsed);
    AppSettings::write(settings, Keys::ShowCurrentTarget, show_current_target);

    AppSettings::write(settings, Keys::ShowSeperatorInterval, show_seperator_interval);
    AppSettings::write(settings, Keys::ShowGrid, show_grid);
    AppSettings::write(settings, Keys::ShowHrTarget, show_hr_target);
    AppSettings::write(settings, Keys::ShowPowerTarget, show_power_target);
    AppSettings::write(settings, Keys::ShowCadenceTarget, show_cadence_target);
    AppSettings::write(settings, Keys::ShowSpeedTarget, show_speed_target);
    AppSettings::write(settings, Keys::ShowHrCurve, show_hr_curve);
    AppSettings::write(settings, Keys::ShowPowerCurve, show_power_curve);
    AppSettings::write(settings, Keys::ShowCadenceCurve, show_cadence_curve);
    AppSettings::write(settings, Keys::ShowSpeedCurve, show_speed_curve);

    AppSettings::write(settings, Keys::SoundPlayerVol, sound_player_vol);
    AppSettings::write(settings, Keys::EnableSound, enable_sound);
    AppSettings::write(settings, Keys::SoundInterval, sound_interval);
    AppSettings::write(settings, Keys::SoundPauseResumeWorkout, sound_pause_resume_workout);
    AppSettings::write(settings, Keys::SoundAchievement, sound_achievement);
    AppSettings::write(settings, Keys::SoundEndWorkout, sound_end_workout);
    AppSettings::write(settings, Keys::SoundAlertPowerUnderTarget, sound_alert_power_under_target);
    AppSettings::write(settings, Keys::SoundAlertPowerAboveTarget, sound_alert_power_above_target);
    AppSettings::write(settings, Keys::SoundAlertCadenceUnderTarget, sound_alert_cadence_under_target);
    AppSettings::write(settings, Keys::SoundAlertCadenceAboveTarget, sound_alert_cadence_above_target);

}

void Account::saveIntervalsIcuCredentials() {

    QSettings settings;
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

    // The athlete id is what auto-logs the app in.
    intervals_icu_api_key.clear();
    intervals_icu_athlete_id.clear();

    QSettings settings;
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

