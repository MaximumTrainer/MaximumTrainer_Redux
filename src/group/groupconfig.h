#ifndef GROUPCONFIG_H
#define GROUPCONFIG_H

#include <QSettings>
#include <QUrl>

namespace GroupConfig {

/// Relay used for group workouts. MT_GROUP_RELAY_URL (env) wins for testing,
/// then the `group_relay_url` setting, so a club can point at its own relay.
inline QUrl relayBaseUrl() {
    const QByteArray fromEnvironment = qgetenv("MT_GROUP_RELAY_URL");
    if (!fromEnvironment.isEmpty()) return QUrl(QString::fromUtf8(fromEnvironment));
    QSettings settings;
    const QString fromSettings = settings.value(QStringLiteral("group_relay_url")).toString();
    if (!fromSettings.isEmpty()) return QUrl(fromSettings);
    return QUrl(QStringLiteral("wss://mt-group-relay.intervals-login.workers.dev"));
}

} // namespace GroupConfig

#endif // GROUPCONFIG_H
