#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H

///
/// Single source of the configuration defaults: written to QSettings on first
/// run by ConfigBootstrap and used as the fallback values when reading.
///
namespace ConfigDefaults {

/// Bump when a future change needs a one-off settings upgrade step.
constexpr int ConfigVersion = 1;

// "logging" group (file_path default is resolved at runtime).
constexpr int  LogLevelValue      = 2;     // LogLevel::Info
constexpr bool LogFileEnabled     = true;

// "account" group.
constexpr int    FTP                       = 150;
constexpr int    LTHR                      = 150;
constexpr double WeightKg                  = 70.0;
constexpr int    NbSecShowInterval         = 5;
constexpr int    NbSecShowIntervalBefore   = 4;
constexpr int    ErgSmoothingDurationS     = 5;
constexpr bool   SensorDropoutEnabled      = true;
constexpr int    SensorDropoutTimeoutS     = 5;
constexpr int    BatteryWarningThreshold   = 20;

} // namespace ConfigDefaults

#endif // CONFIG_DEFAULTS_H
