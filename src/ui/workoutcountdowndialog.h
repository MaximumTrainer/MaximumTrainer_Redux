#ifndef WORKOUTCOUNTDOWNDIALOG_H
#define WORKOUTCOUNTDOWNDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

/// Displays a countdown before auto-advancing to the next queued workout.
/// The user can start immediately ("Start Now") or cancel the remaining queue.
class WorkoutCountdownDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Result { StartNow, Cancelled };

    explicit WorkoutCountdownDialog(const QString &nextWorkoutName,
                                    int countdownSeconds = 60,
                                    QWidget *parent = nullptr);

    Result dialogResult() const { return m_result; }

private slots:
    void tick();
    void onStartNow();
    void onCancel();

private:
    QLabel      *m_label     = nullptr;
    QPushButton *m_startBtn  = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QTimer      *m_timer     = nullptr;

    int     m_remaining;
    QString m_workoutName;
    Result  m_result = Result::Cancelled;

    void updateLabel();
};

#endif // WORKOUTCOUNTDOWNDIALOG_H
