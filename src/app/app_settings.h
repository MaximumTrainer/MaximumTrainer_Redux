#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <QLatin1String>
#include <QSettings>
#include <QVariant>

///
/// A persisted setting: its full QSettings path ("group/name") and the value
/// returned when it has never been written. Declared once in setting_keys.h, so
/// a key cannot be misspelled at a call site and its default lives in one place.
///
template <typename T>
struct SettingKey {
    const char *path;
    T defaultValue;

    QString pathString() const { return QLatin1String(path); }
};

namespace AppSettings {

template <typename T>
T read(const QSettings &settings, const SettingKey<T> &key)
{
    return settings.value(key.pathString(), QVariant::fromValue(key.defaultValue)).template value<T>();
}

/// For keys whose default is only known at runtime (e.g. the home directory).
template <typename T>
T read(const QSettings &settings, const SettingKey<T> &key, const T &runtimeDefault)
{
    return settings.value(key.pathString(), QVariant::fromValue(runtimeDefault)).template value<T>();
}

template <typename T>
T read(const SettingKey<T> &key)
{
    const QSettings settings;
    return read(settings, key);
}

template <typename T>
T read(const SettingKey<T> &key, const T &runtimeDefault)
{
    const QSettings settings;
    return read(settings, key, runtimeDefault);
}

template <typename T>
void write(QSettings &settings, const SettingKey<T> &key, const T &value)
{
    settings.setValue(key.pathString(), QVariant::fromValue(value));
}

template <typename T>
void write(const SettingKey<T> &key, const T &value)
{
    QSettings settings;
    write(settings, key, value);
}

template <typename T>
bool contains(const QSettings &settings, const SettingKey<T> &key)
{
    return settings.contains(key.pathString());
}

template <typename T>
void remove(QSettings &settings, const SettingKey<T> &key)
{
    settings.remove(key.pathString());
}

template <typename T>
void remove(const SettingKey<T> &key)
{
    QSettings settings;
    remove(settings, key);
}

/// Reads @p key, falling back to @p legacyKey (a previous location of the same
/// setting) when @p key has never been written.
template <typename T>
T readWithFallback(const QSettings &settings, const SettingKey<T> &key, const SettingKey<T> &legacyKey)
{
    return contains(settings, key) ? read(settings, key) : read(settings, legacyKey);
}

/// Writes the key's default only when nothing is stored yet (first-run seeding).
template <typename T>
void writeDefaultIfMissing(QSettings &settings, const SettingKey<T> &key)
{
    if (!contains(settings, key))
        write(settings, key, key.defaultValue);
}

} // namespace AppSettings

#endif // APP_SETTINGS_H
