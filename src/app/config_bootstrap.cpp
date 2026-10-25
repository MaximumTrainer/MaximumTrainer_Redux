#include "config_bootstrap.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

#include "config_defaults.h"
#include "logger.h"

namespace ConfigBootstrap {

void applyAppIdentity(const QString &version)
{
    QCoreApplication::setOrganizationName(QStringLiteral("MaximumTrainer"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("maximumtrainer.com"));
    QCoreApplication::setApplicationName(QStringLiteral("MaximumTrainer_Redux"));
    QCoreApplication::setApplicationVersion(version);
}

QString resultName(Result result)
{
    switch (result) {
    case Result::Created:    return QStringLiteral("created");
    case Result::Upgraded:   return QStringLiteral("upgraded");
    case Result::Existing:   return QStringLiteral("existing");
    case Result::Unreadable: return QStringLiteral("unreadable");
    }
    return QString();
}

static QString statusName(QSettings::Status status)
{
    switch (status) {
    case QSettings::NoError:     return QStringLiteral("NoError");
    case QSettings::AccessError: return QStringLiteral("AccessError");
    case QSettings::FormatError: return QStringLiteral("FormatError");
    }
    return QStringLiteral("unknown");
}

static QString location(const QSettings &settings)
{
    return QDir::toNativeSeparators(settings.fileName());
}

static bool hasUserKeys(const QSettings &settings)
{
    const QStringList keys = settings.allKeys();
    for (const QString &key : keys)
        if (key != QLatin1String(MigrationMarkerKey))
            return true;
    return false;
}

// Copies the legacy scope once; returns the number of keys copied, or -1 when
// the migration was skipped.
static int migrateLegacy(QSettings &settings, QSettings *legacy)
{
    if (settings.contains(QLatin1String(MigrationMarkerKey))) {
        LOG_INFO("Config", QStringLiteral("Legacy settings migration skipped (already migrated)"));
        return -1;
    }
    if (!legacy) {
        LOG_INFO("Config", QStringLiteral("Legacy settings migration skipped (no legacy scope)"));
        return -1;
    }

    const QStringList keys = legacy->allKeys();
    if (legacy->status() != QSettings::NoError) {
        LOG_WARN("Config", QStringLiteral("Legacy settings at %1 unreadable (%2) — migration skipped, will retry next start")
                               .arg(location(*legacy), statusName(legacy->status())));
        return -1;
    }

    for (const QString &key : keys)
        settings.setValue(key, legacy->value(key));
    settings.setValue(QLatin1String(MigrationMarkerKey), true);
    LOG_INFO("Config", QStringLiteral("Legacy settings migration ran: copied %1 key(s) from %2")
                           .arg(keys.size()).arg(location(*legacy)));
    return keys.size();
}

static void setIfMissing(QSettings &settings, const QString &key, const QVariant &value)
{
    if (!settings.contains(key))
        settings.setValue(key, value);
}

static void writeDefaults(QSettings &settings, const QString &logFilePath)
{
    using namespace ConfigDefaults;
    setIfMissing(settings, QStringLiteral("logging/level"),        LogLevelValue);
    setIfMissing(settings, QStringLiteral("logging/file_enabled"), LogFileEnabled);
    setIfMissing(settings, QStringLiteral("logging/file_path"),    logFilePath);

    setIfMissing(settings, QStringLiteral("account/FTP"),                         FTP);
    setIfMissing(settings, QStringLiteral("account/LTHR"),                        LTHR);
    setIfMissing(settings, QStringLiteral("account/weight_kg"),                   WeightKg);
    setIfMissing(settings, QStringLiteral("account/nb_sec_show_interval"),        NbSecShowInterval);
    setIfMissing(settings, QStringLiteral("account/nb_sec_show_interval_before"), NbSecShowIntervalBefore);
    setIfMissing(settings, QStringLiteral("account/erg_smoothing_duration_s"),    ErgSmoothingDurationS);
    setIfMissing(settings, QStringLiteral("account/sensor_dropout_enabled"),      SensorDropoutEnabled);
    setIfMissing(settings, QStringLiteral("account/sensor_dropout_timeout_s"),    SensorDropoutTimeoutS);
    setIfMissing(settings, QStringLiteral("account/battery_warning_threshold"),   BatteryWarningThreshold);

    settings.setValue(QLatin1String(ConfigVersionKey), ConfigVersion);
}

Outcome run(QSettings &settings, QSettings *legacy, const QString &logFilePath)
{
    Outcome outcome;

    // Without this, macOS allKeys() also returns the global preferences domain
    // (AppleLocale etc.), so a fresh install never looks empty and the legacy
    // migration would copy those system keys.
    settings.setFallbacksEnabled(false);
    if (legacy)
        legacy->setFallbacksEnabled(false);

    // allKeys() forces the store to be read so status() reflects it.
    const bool hadUserKeys = hasUserKeys(settings);
    if (settings.status() != QSettings::NoError) {
        LOG_ERROR("Config", QStringLiteral("Settings store at %1 is unreadable (%2) — continuing with built-in defaults; the store is left untouched")
                                .arg(location(settings), statusName(settings.status())));
        outcome.result = Result::Unreadable;
        return outcome;
    }

    const int copied = migrateLegacy(settings, legacy);
    outcome.migrationRan = copied >= 0;
    outcome.migratedKeys = qMax(copied, 0);

    if (settings.contains(QLatin1String(ConfigVersionKey))) {
        outcome.result = Result::Existing;
        LOG_INFO("Config", QStringLiteral("Configuration found at %1 (config_version %2)")
                               .arg(location(settings),
                                    settings.value(QLatin1String(ConfigVersionKey)).toString()));
    } else if (!hadUserKeys) {
        writeDefaults(settings, logFilePath);
        outcome.result = Result::Created;
        LOG_INFO("Config", QStringLiteral("no configuration found — default configuration created at %1")
                               .arg(location(settings)));
    } else {
        settings.setValue(QLatin1String(ConfigVersionKey), ConfigDefaults::ConfigVersion);
        outcome.result = Result::Upgraded;
        LOG_INFO("Config", QStringLiteral("Existing configuration at %1 has no config_version — set to %2")
                               .arg(location(settings)).arg(ConfigDefaults::ConfigVersion));
    }

    settings.sync();
    if (settings.status() != QSettings::NoError)
        LOG_WARN("Config", QStringLiteral("Could not save settings to %1 (%2)")
                               .arg(location(settings), statusName(settings.status())));
    return outcome;
}

Outcome run(const QString &logFilePath)
{
    QSettings settings;
    QSettings legacy{QString::fromLatin1(LegacyOrganization), QString::fromLatin1(LegacyApplication)};
    return run(settings, &legacy, logFilePath);
}

} // namespace ConfigBootstrap
