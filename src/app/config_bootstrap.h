#ifndef CONFIG_BOOTSTRAP_H
#define CONFIG_BOOTSTRAP_H

#include <QString>

class QSettings;

///
/// First-run handling for the QSettings store, run once at startup after the
/// application identity is set and before anything else reads the settings:
///
///   1. Check the store is readable (QSettings::status()).  An unreadable store
///      is logged at Error and left untouched; the app runs on the in-memory
///      defaults.
///   2. Copy the legacy "Max++ inc./MaximumTrainer" scope once.
///   3. First run (no keys before migration, no config_version): write the
///      defaults from ConfigDefaults, never overwriting an existing key.
///      Existing install without config_version: add config_version only.
///
namespace ConfigBootstrap {

inline constexpr char LegacyOrganization[] = "Max++ inc.";
inline constexpr char LegacyApplication[]  = "MaximumTrainer";
inline constexpr char MigrationMarkerKey[] = "migrated_from_max_inc";
inline constexpr char ConfigVersionKey[]   = "config_version";

enum class Result {
    Created,     // no configuration found; defaults written
    Upgraded,    // existing configuration; config_version added
    Existing,    // configuration already versioned; nothing written
    Unreadable   // store could not be read; nothing written
};

struct Outcome {
    Result result = Result::Existing;
    bool migrationRan = false;
    int migratedKeys = 0;
};

/// Set organisation name/domain, application name and version.  Safe to call
/// more than once and before QCoreApplication exists.
void applyAppIdentity(const QString &version);

/// Run steps 1–3 on explicit stores (used by tests).  @p legacy may be null.
Outcome run(QSettings &settings, QSettings *legacy, const QString &logFilePath);

/// Run steps 1–3 on the default QSettings scope and the legacy scope.
Outcome run(const QString &logFilePath);

QString resultName(Result result);

} // namespace ConfigBootstrap

#endif // CONFIG_BOOTSTRAP_H
