#ifndef SETTING_KEYS_H
#define SETTING_KEYS_H

#include "app_settings.h"
#include "config_defaults.h"

#include <QByteArray>
#include <QPoint>
#include <QSize>
#include <QString>
#include <QStringList>

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
/// Retired manual API key (sign-in uses OAuth); removed at load.
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
inline constexpr SettingKey<int>  DisplayPowerBalance            {"displayPrefs/display_power_balance",               0}; // 0: when balance data is available, 1: only for intervals with a balance target
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

namespace MainWindow {
inline constexpr SettingKey<QSize> Size      {"MainWindow/size",      QSize(1150, 700)};
inline constexpr SettingKey<QPoint> Pos      {"MainWindow/pos",       QPoint(200, 200)};
inline constexpr SettingKey<bool> Maximized  {"MainWindow/maximized", false};
} // namespace MainWindow

namespace WorkoutDialog {
inline constexpr SettingKey<QSize> Size      {"WorkoutDialog/size",       QSize(1350, 800)};
inline constexpr SettingKey<QPoint> Pos      {"WorkoutDialog/pos",        QPoint(100, 100)};
inline constexpr SettingKey<int>  Splitter0  {"WorkoutDialog/splitter0",  2};
inline constexpr SettingKey<int>  Splitter1  {"WorkoutDialog/splitter1",  35};
inline constexpr SettingKey<int>  Splitter2  {"WorkoutDialog/splitter2",  39};
inline constexpr SettingKey<int>  Splitter3  {"WorkoutDialog/splitter3",  24};
inline constexpr SettingKey<bool> Fullscreen {"WorkoutDialog/fullscreen", false};
} // namespace WorkoutDialog

namespace ActivityDetailDialog {
inline const SettingKey<QByteArray> Geometry {"activityDetailDialog/geometry", QByteArray()};
} // namespace ActivityDetailDialog

namespace WorkoutFilter {
inline const SettingKey<QString> Name    {"Filter/nameFilter",    QString()};
inline const SettingKey<QString> Plan    {"Filter/planFilter",    QString()};
inline const SettingKey<QString> Creator {"Filter/creatorFilter", QString()};
inline constexpr SettingKey<int> Type    {"Filter/typeFilter",    -1};
} // namespace WorkoutFilter

namespace WorkoutQueue {
inline const SettingKey<QStringList> FilePaths {"workoutQueue/filePaths", QStringList()};
inline const SettingKey<QStringList> Names     {"workoutQueue/names",     QStringList()};
} // namespace WorkoutQueue

/// Last directory used by a file dialog; read with the home directory as default.
namespace LastPath {
inline const SettingKey<QString> WorkoutImport {"Importer/loadPath",        QString()};
inline const SettingKey<QString> Video         {"videoPlayer/loadPath",     QString()};
inline const SettingKey<QString> ReportExport  {"reports/savePathExport",   QString()};
} // namespace LastPath

namespace Media {
inline constexpr SettingKey<int> VideoVolume  {"videoPlayer/soundVolume", 100};
inline constexpr SettingKey<int> RadioVolume  {"radioPlayer/volume",      100};
inline const SettingKey<QString> LastRadioStation {"radioPlayer/lastStation", QString()};
/// Hash of the bundled radio list last seen, to pick up a changed default list.
inline const SettingKey<QString> RadioDefaultsHash {"radios/defaultsHash", QString()};
inline const SettingKey<QString> WorkoutBrowserHomePage {"webBrowserWorkout/defaultUrl", QStringLiteral("https://www.youtube.com")};
/// One-time reset of the home page away from DRM services (see readWorkoutBrowserHomePage).
inline constexpr SettingKey<bool> WorkoutBrowserYoutubeResetDone {"webBrowserWorkout/youtubeResetDone", false};
} // namespace Media

namespace Login {
inline constexpr SettingKey<bool> WorkOffline     {"login/workOffline", false};
inline const SettingKey<QString> LastLoggedUsername {"SettingsGeneral/lastLoggedUsername", QString()};
inline const SettingKey<QString> LastLoggedKey      {"SettingsGeneral/lastLoggedKey",      QString()};
/// ISO-8601 time of the last FTP/LTHR/weight sync from intervals.icu.
inline const SettingKey<QString> IntervalsIcuProfileSyncedAt {"intervalsIcu/profileSyncedAt", QString()};
} // namespace Login

namespace Group {
/// Overrides the default group-workout relay (MT_GROUP_RELAY_URL wins over it).
inline const SettingKey<QString> RelayUrl {"group_relay_url", QString()};
} // namespace Group

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

namespace AppSettings {
/// The workout web browser's home page. The old Netflix default needs Widevine
/// DRM, which the embedded view cannot bundle, so it never played; it is reset
/// to YouTube once, after which the user's own choice is kept.
inline QString readWorkoutBrowserHomePage()
{
    QSettings settings;
    if (!read(settings, SettingKeys::Media::WorkoutBrowserYoutubeResetDone)) {
        write(settings, SettingKeys::Media::WorkoutBrowserHomePage, SettingKeys::Media::WorkoutBrowserHomePage.defaultValue);
        write(settings, SettingKeys::Media::WorkoutBrowserYoutubeResetDone, true);
    }
    return read(settings, SettingKeys::Media::WorkoutBrowserHomePage);
}
} // namespace AppSettings

#endif // SETTING_KEYS_H
