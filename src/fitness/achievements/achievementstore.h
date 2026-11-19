#ifndef ACHIEVEMENTSTORE_H
#define ACHIEVEMENTSTORE_H

#include <QDateTime>
#include <QMap>
#include <QString>

/// Earned achievements, saved locally in the app settings with the date earned.
namespace AchievementStore {

QMap<QString, QDateTime> unlocked();
bool isUnlocked(const QString &key);
/// Records \a key as earned at \a when; false if it was already earned.
bool unlock(const QString &key, const QDateTime &when = QDateTime::currentDateTime());

} // namespace AchievementStore

#endif // ACHIEVEMENTSTORE_H
