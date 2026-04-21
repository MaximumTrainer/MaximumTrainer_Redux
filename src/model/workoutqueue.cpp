#include "workoutqueue.h"

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

void WorkoutQueue::moveItem(int fromIndex, int toIndex)
{
    if (fromIndex == toIndex) return;
    if (fromIndex < 0 || fromIndex >= m_filePaths.size()) return;
    if (toIndex   < 0 || toIndex   >= m_filePaths.size()) return;
    m_filePaths.move(fromIndex, toIndex);
    m_names.move(fromIndex, toIndex);
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
    QSettings s;
    s.beginGroup("workoutQueue");
    s.setValue("filePaths", m_filePaths);
    s.setValue("names",     m_names);
    s.endGroup();
}

void WorkoutQueue::load()
{
    QSettings s;
    s.beginGroup("workoutQueue");
    m_filePaths = s.value("filePaths").toStringList();
    m_names     = s.value("names").toStringList();
    s.endGroup();
    // Defensive: keep lists in sync
    while (m_names.size() < m_filePaths.size()) m_names.append(QString());
    while (m_names.size() > m_filePaths.size()) m_names.removeLast();
}
