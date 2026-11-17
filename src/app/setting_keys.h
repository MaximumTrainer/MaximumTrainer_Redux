#ifndef SETTING_KEYS_H
#define SETTING_KEYS_H

#include "app_settings.h"
#include "config_defaults.h"

#include <QString>

///
/// Every persisted setting, by QSettings group. Paths are on-disk names: never
/// rename one without a migration, or existing users lose the stored value.
///
namespace SettingKeys {

namespace Account {
inline constexpr SettingKey<int>    Ftp                      {"account/FTP",                         ConfigDefaults::FTP};
inline constexpr SettingKey<int>    Lthr                     {"account/LTHR",                        ConfigDefaults::LTHR};
inline constexpr SettingKey<double> WeightKg                 {"account/weight_kg",                   ConfigDefaults::WeightKg};
inline constexpr SettingKey<int>    NbSecShowInterval        {"account/nb_sec_show_interval",        ConfigDefaults::NbSecShowInterval};
inline constexpr SettingKey<int>    NbSecShowIntervalBefore  {"account/nb_sec_show_interval_before", ConfigDefaults::NbSecShowIntervalBefore};
inline constexpr SettingKey<int>    ErgSmoothingDurationS    {"account/erg_smoothing_duration_s",    ConfigDefaults::ErgSmoothingDurationS};
inline constexpr SettingKey<bool>   SensorDropoutEnabled     {"account/sensor_dropout_enabled",      ConfigDefaults::SensorDropoutEnabled};
inline constexpr SettingKey<int>    SensorDropoutTimeoutS    {"account/sensor_dropout_timeout_s",    ConfigDefaults::SensorDropoutTimeoutS};
inline constexpr SettingKey<int>    BatteryWarningThreshold  {"account/battery_warning_threshold",   ConfigDefaults::BatteryWarningThreshold};
inline constexpr SettingKey<bool>   IntervalSummaryEnabled   {"account/interval_summary_enabled",    true};
inline constexpr SettingKey<int>    IntervalSummaryDurationS {"account/interval_summary_duration_s", 5};
inline constexpr SettingKey<int>    AppTheme                 {"account/app_theme",                   2}; // AppTheme::Mode::System
inline constexpr SettingKey<bool>   ForcedDarkDefaultApplied {"account/forced_dark_default_applied", false};
inline constexpr SettingKey<bool>   IntervalsIcuAutoUpload   {"account/intervals_icu_auto_upload",   false};
inline constexpr SettingKey<qlonglong> StravaTokenExpiresAt  {"account/strava_token_expires_at",     0};
inline const SettingKey<QString>    IntervalsIcuApiKey       {"account/intervals_icu_api_key",       QString()};
inline const SettingKey<QString>    IntervalsIcuAthleteId    {"account/intervals_icu_athlete_id",    QString()};
/// Plain-text token from versions before the credential store; read once to migrate, then removed.
inline const SettingKey<QString>    LegacyIntervalsIcuAccessToken {"account/intervals_icu_access_token", QString()};
} // namespace Account

namespace DisplayPrefs {
inline constexpr SettingKey<bool> ShowHrWidget                   {"displayPrefs/show_hr_widget",                      true};
inline constexpr SettingKey<bool> ShowPowerWidget                {"displayPrefs/show_power_widget",                   true};
inline constexpr SettingKey<bool> ShowPowerBalanceWidget         {"displayPrefs/show_power_balance_widget",           true};
inline constexpr SettingKey<bool> ShowCadenceWidget              {"displayPrefs/show_cadence_widget",                 true};
inline constexpr SettingKey<bool> ShowSpeedWidget                {"displayPrefs/show_speed_widget",                   true};
inline constexpr SettingKey<bool> ShowCaloriesWidget             {"displayPrefs/show_calories_widget",                true};
inline constexpr SettingKey<bool> ShowOxygenWidget               {"displayPrefs/show_oxygen_widget",                  true};
inline constexpr SettingKey<bool> ShowTrainerSpeed               {"displayPrefs/show_trainer_speed",                  true};
inline constexpr SettingKey<int>  DisplayHr                      {"displayPrefs/display_hr",                          1};
inline constexpr SettingKey<int>  DisplayPower                   {"displayPrefs/display_power",                       2};
inline constexpr SettingKey<int>  DisplayPowerBalance            {"displayPrefs/display_power_balance",               1};
inline constexpr SettingKey<int>  DisplayCadence                 {"displayPrefs/display_cadence",                     1};
inline constexpr SettingKey<int>  DisplayVideo                   {"displayPrefs/display_video",                       0};
inline constexpr SettingKey<int>  OffsetPower                    {"displayPrefs/offset_power",                        0};
inline constexpr SettingKey<int>  StartTrigger                   {"displayPrefs/start_trigger",                       0};
inline constexpr SettingKey<int>  ValueCadenceStart              {"displayPrefs/value_cadence_start",                 40};
inline constexpr SettingKey<int>  ValuePowerStart                {"displayPrefs/value_power_start",                   120};
inline constexpr SettingKey<int>  ValueSpeedStart                {"displayPrefs/value_speed_start",                   20};
inline constexpr SettingKey<int>  LastIndexSelectedConfigWorkout {"displayPrefs/last_index_selected_config_workout",  0};
inline constexpr SettingKey<int>  LastTabSubConfigSelected       {"displayPrefs/last_tab_sub_config_selected",        0};
inline constexpr SettingKey<bool> ControlTrainerResistance       {"displayPrefs/control_trainer_resistance",          true};
inline constexpr SettingKey<bool> VirtualShifting                {"displayPrefs/virtual_shifting",                    false};
inline constexpr SettingKey<bool> UseZwiftClick                  {"displayPrefs/use_zwift_click",                     false};
inline constexpr SettingKey<bool> EnableStudioMode               {"displayPrefs/enable_studio_mode",                  false};
inline constexpr SettingKey<bool> DistanceInKm                   {"displayPrefs/distance_in_km",                      true};
inline constexpr SettingKey<bool> ForceWorkoutWindowOnTop        {"displayPrefs/force_workout_window_on_top",         false};
inline constexpr SettingKey<bool> StravaAutoUpload               {"displayPrefs/strava_auto_upload",                  false};
inline constexpr SettingKey<int>  NbUserStudio                   {"displayPrefs/nb_user_studio",                      3};
inline constexpr SettingKey<bool> UsePmForCadence                {"displayPrefs/use_pm_for_cadence",                  false};
inline constexpr SettingKey<bool> UsePmForSpeed                  {"displayPrefs/use_pm_for_speed",                    false};
inline constexpr SettingKey<bool> ShowIncludedWorkout            {"displayPrefs/show_included_workout",               true};
inline constexpr SettingKey<bool> RowingMode                     {"displayPrefs/rowing_mode",                         false};
inline constexpr SettingKey<bool> ShowTimerOnTop                 {"displayPrefs/show_timer_on_top",                   false};
inline constexpr SettingKey<bool> ShowIntervalRemaining          {"displayPrefs/show_interval_remaining",             true};
inline constexpr SettingKey<bool> ShowWorkoutRemaining           {"displayPrefs/show_workout_remaining",              false};
inline constexpr SettingKey<bool> ShowElapsed                    {"displayPrefs/show_elapsed",                        true};
inline constexpr SettingKey<bool> ShowCurrentTarget              {"displayPrefs/show_current_target",                 true};
inline constexpr SettingKey<bool> ShowSeperatorInterval          {"displayPrefs/show_seperator_interval",             true};
inline constexpr SettingKey<bool> ShowGrid                       {"displayPrefs/show_grid",                           false};
inline constexpr SettingKey<bool> ShowHrTarget                   {"displayPrefs/show_hr_target",                      true};
inline constexpr SettingKey<bool> ShowPowerTarget                {"displayPrefs/show_power_target",                   true};
inline constexpr SettingKey<bool> ShowCadenceTarget              {"displayPrefs/show_cadence_target",                 true};
inline constexpr SettingKey<bool> ShowSpeedTarget                {"displayPrefs/show_speed_target",                   true};
inline constexpr SettingKey<bool> ShowHrCurve                    {"displayPrefs/show_hr_curve",                       true};
inline constexpr SettingKey<bool> ShowPowerCurve                 {"displayPrefs/show_power_curve",                    true};
inline constexpr SettingKey<bool> ShowCadenceCurve               {"displayPrefs/show_cadence_curve",                  true};
inline constexpr SettingKey<bool> ShowSpeedCurve                 {"displayPrefs/show_speed_curve",                    true};
inline constexpr SettingKey<int>  SoundPlayerVol                 {"displayPrefs/sound_player_vol",                    100};
inline constexpr SettingKey<bool> EnableSound                    {"displayPrefs/enable_sound",                        true};
inline constexpr SettingKey<bool> SoundInterval                  {"displayPrefs/sound_interval",                      true};
inline constexpr SettingKey<bool> SoundPauseResumeWorkout        {"displayPrefs/sound_pause_resume_workout",          true};
inline constexpr SettingKey<bool> SoundAchievement               {"displayPrefs/sound_achievement",                   true};
inline constexpr SettingKey<bool> SoundEndWorkout                {"displayPrefs/sound_end_workout",                   true};
inline constexpr SettingKey<bool> SoundAlertPowerUnderTarget     {"displayPrefs/sound_alert_power_under_target",      false};
inline constexpr SettingKey<bool> SoundAlertPowerAboveTarget     {"displayPrefs/sound_alert_power_above_target",      false};
inline constexpr SettingKey<bool> SoundAlertCadenceUnderTarget   {"displayPrefs/sound_alert_cadence_under_target",    false};
inline constexpr SettingKey<bool> SoundAlertCadenceAboveTarget   {"displayPrefs/sound_alert_cadence_above_target",    false};
} // namespace DisplayPrefs

namespace Legacy {
/// Strava token expiry was also written under displayPrefs/ (and only read from
/// there); read as a fallback so the value survives the move to account/.
inline constexpr SettingKey<qlonglong> DisplayPrefsStravaTokenExpiresAt {"displayPrefs/strava_token_expires_at", 0};
} // namespace Legacy

namespace Logging {
inline constexpr SettingKey<int>  Level       {"logging/level",        ConfigDefaults::LogLevelValue};
inline constexpr SettingKey<bool> FileEnabled {"logging/file_enabled", ConfigDefaults::LogFileEnabled};
/// Default resolved at runtime (per-platform log directory).
inline const SettingKey<QString>  FilePath    {"logging/file_path",    QString()};
} // namespace Logging

} // namespace SettingKeys

#endif // SETTING_KEYS_H
