#ifndef WORKOUTQUEUE_H
#define WORKOUTQUEUE_H

#include <QObject>
#include <QStringList>

/// Manages a persistent ordered list of workout file paths to execute sequentially.
/// The queue is serialised to QSettings under the key "workoutQueue/filePaths".
class WorkoutQueue : public QObject
{
    Q_OBJECT
public:
    explicit WorkoutQueue(QObject *parent = nullptr);

    void addWorkout(const QString &filePath, const QString &name);
    void removeAt(int index);
    void moveUp(int index);
    void moveDown(int index);
    void clear();

    bool isEmpty() const { return m_filePaths.isEmpty(); }
    int  count()   const { return m_filePaths.size(); }

    QString filePath(int index) const { return m_filePaths.value(index); }
    QString name(int index)     const { return m_names.value(index); }

    QStringList filePaths() const { return m_filePaths; }
    QStringList names()     const { return m_names; }

    /// Consume (remove and return) the first workout file path in the queue.
    /// Returns an empty string if the queue is empty.
    QString dequeueFilePath();

    /// Return the name of the first workout in the queue without removing it.
    /// Returns an empty string if the queue is empty.
    QString dequeueName();

    void save() const;
    void load();

signals:
    void queueChanged();

private:
    QStringList m_filePaths;
    QStringList m_names;
};

#endif // WORKOUTQUEUE_H
