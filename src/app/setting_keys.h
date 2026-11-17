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
