#include "workoutqueue.h"
#include "setting_keys.h"

#include <QSettings>
#include <utility>

WorkoutQueue::WorkoutQueue(QObject *parent) : QObject(parent)
{
    load();
}

void WorkoutQueue::addWorkout(const QString &filePath, const QString &name)
{
    m_filePaths.append(filePath);
    m_names.append(name);
    save();
    emit queueChanged();
}

void WorkoutQueue::removeAt(int index)
{
    if (index < 0 || index >= m_filePaths.size()) return;
    m_filePaths.removeAt(index);
    m_names.removeAt(index);
    save();
    emit queueChanged();
}

void WorkoutQueue::moveUp(int index)
{
    if (index <= 0 || index >= m_filePaths.size()) return;
    m_filePaths.swapItemsAt(index, index - 1);
    m_names.swapItemsAt(index, index - 1);
    save();
    emit queueChanged();
}

void WorkoutQueue::moveDown(int index)
{
    if (index < 0 || index >= m_filePaths.size() - 1) return;
    m_filePaths.swapItemsAt(index, index + 1);
    m_names.swapItemsAt(index, index + 1);
    save();
    emit queueChanged();
}

void WorkoutQueue::clear()
{
    m_filePaths.clear();
    m_names.clear();
    save();
    emit queueChanged();
}

QString WorkoutQueue::dequeueFilePath()
{
    if (m_filePaths.isEmpty()) return {};
    QString path = m_filePaths.takeFirst();
    if (!m_names.isEmpty()) m_names.takeFirst();
    save();
    emit queueChanged();
    return path;
}

QString WorkoutQueue::dequeueName()
{
    if (m_names.isEmpty()) return {};
    QString name = m_names.takeFirst();
    if (!m_filePaths.isEmpty()) m_filePaths.takeFirst();
    save();
    emit queueChanged();
    return name;
}

void WorkoutQueue::save() const
{
    QSettings settings;
    AppSettings::write(settings, SettingKeys::WorkoutQueue::FilePaths, m_filePaths);
    AppSettings::write(settings, SettingKeys::WorkoutQueue::Names,     m_names);
}

void WorkoutQueue::load()
{
    const QSettings settings;
    m_filePaths = AppSettings::read(settings, SettingKeys::WorkoutQueue::FilePaths);
    m_names     = AppSettings::read(settings, SettingKeys::WorkoutQueue::Names);
    // Defensive: keep lists in sync
    while (m_names.size() < m_filePaths.size()) m_names.append(QString());
    while (m_names.size() > m_filePaths.size()) m_names.removeLast();
}
