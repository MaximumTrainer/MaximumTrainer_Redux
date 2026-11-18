#ifndef WORKOUTRACEVIEW_H
#define WORKOUTRACEVIEW_H

#include <QObject>
#include <QString>
#include <QVariantList>

class QLayout;
class QQuickWidget;
class QWidget;
class RetroRaceController;
class Workout;

/// The workout dialog's "Game" content view: owns the retro race controller
/// and its QML view, and turns the workout's progress into race updates.
class WorkoutRaceView : public QObject
{
    Q_OBJECT

public:
    struct Setup {
        double riderMassKg = 0.0;
        double cdA = 0.0;
        bool ergMode = false;
        QString intervalMessage;
    };
    struct WorkoutState {
        bool started = false;
        bool paused = false;
        bool over = false;
    };

    /// Builds the controller and the view and adds the view to @p contentLayout.
    WorkoutRaceView(const Workout &workout, const Setup &setup, QLayout *contentLayout,
                    const WorkoutState &state, QWidget *viewParent);

    RetroRaceController *controller() const { return m_controller; }
    QQuickWidget *view() const { return m_view; }

    /// Once per workout second. @p secondsLeftInInterval is read before the
    /// dialog's timers tick down, so it is one second high.
    void onWorkoutSecond(const Workout &workout, int currentInterval, int elapsedSeconds,
                         double secondsLeftInInterval, int ftpWatts);

    /// The workout's target power sampled into 120 heights normalised to 0..1,
    /// for the race's "what's next" strip.
    static QVariantList workoutProfile(const Workout &workout);

private:
    RetroRaceController *m_controller = nullptr;
    QQuickWidget *m_view = nullptr;
};

#endif // WORKOUTRACEVIEW_H
