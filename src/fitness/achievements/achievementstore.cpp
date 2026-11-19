#include "achievementstore.h"

#include <QSettings>

namespace AchievementStore {

namespace {
const QString kGroup = QStringLiteral("achievements");
}

QMap<QString, QDateTime> unlocked()
{
    QMap<QString, QDateTime> earned;
    QSettings settings;
    settings.beginGroup(kGroup);
    for (const QString &key : settings.childKeys())
        earned.insert(key, QDateTime::fromString(settings.value(key).toString(), Qt::ISODate));
    settings.endGroup();
    return earned;
}

bool isUnlocked(const QString &key)
{
    QSettings settings;
    return settings.contains(kGroup + QLatin1Char('/') + key);
}

bool unlock(const QString &key, const QDateTime &when)
{
    if (isUnlocked(key))
        return false;
    QSettings settings;
    settings.setValue(kGroup + QLatin1Char('/') + key, when.toString(Qt::ISODate));
    return true;
}

} // namespace AchievementStore
