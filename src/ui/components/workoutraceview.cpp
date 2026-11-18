#include "workoutraceview.h"

#include <QLayout>
#include <QQmlContext>
#include <QQuickWidget>
#include <QVector>

#include "interval.h"
#include "retroracecontroller.h"
#include "util.h"
#include "workout.h"

WorkoutRaceView::WorkoutRaceView(const Workout &workout, const Setup &setup, QLayout *contentLayout,
                                 const WorkoutState &state, QWidget *viewParent)
    : QObject(viewParent)
{
    m_controller = new RetroRaceController(this);
    m_controller->setRiderParams(setup.riderMassKg, setup.cdA);
    m_controller->setLiveMode(true);
    // In ERG the partner is a cooperative team-mate (draft + leash); in slope/
    // free-ride it stays a straight effort-vs-effort race.
    m_controller->setErgMode(setup.ergMode);
    m_controller->setWorkoutName(workout.getName());   // enables the chooser
    m_controller->setWorkoutProfile(workoutProfile(workout));
    m_controller->setIntervalMessage(setup.intervalMessage);

    m_view = new QQuickWidget(viewParent);
    // If this view ever needs a native window, confine it to the view rather
    // than promoting the dialog's own window (which would tear the dialog down
    // and rebuild it). The main guard against that, though, is building this
    // view at show time while hidden (see WorkoutDialog::showEvent) so the
    // realization is a one-time, invisible event and never happens on a
    // mid-workout switch.
    m_view->setAttribute(Qt::WA_DontCreateNativeAncestors, true);
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_view->rootContext()->setContextProperty(QStringLiteral("race"), m_controller);
    m_view->setSource(QUrl(QStringLiteral("qrc:/game/qml/RetroRace.qml")));
    // Share the content slot with the video / web players.
    if (contentLayout)
        contentLayout->addWidget(m_view);
    m_controller->start();   // armed; rolls on the workout start
    // If the workout is already running/over (Game selected mid-workout),
    // bring the race into the right state immediately.
    if (state.over)
        m_controller->finishRace();
    else if (state.started && !state.paused)
        m_controller->beginRace();
}

void WorkoutRaceView::onWorkoutSecond(const Workout &workout, int currentInterval, int elapsedSeconds,
                                      double secondsLeftInInterval, int ftpWatts)
{
    const double totalSeconds = Util::convertQTimeToSecD(workout.getDurationQTime());
    if (totalSeconds > 0)
        m_controller->setWorkoutProgress(qBound(0.0, elapsedSeconds / totalSeconds, 1.0));
    m_controller->setWorkoutElapsedSec(elapsedSeconds);
    // The dialog's timers are decremented just AFTER this call, so its values
    // read 1 s high vs the on-screen graph & timer widgets: subtract 1 so the
    // game's interval/finish countdowns stay in sync.
    if (totalSeconds > 0)
        m_controller->setFinishIn(qMax(0.0, totalSeconds - elapsedSeconds - 1.0));

    // Preview the upcoming interval's target on the road ahead.
    const double secsToNext = qMax(0.0, secondsLeftInInterval - 1.0);
    double nextW = -1.0, nextCad = -1.0;
    if (currentInterval + 1 < workout.getNbInterval()) {
        const Interval nxt = workout.getInterval(currentInterval + 1);
        if (nxt.getPowerStepType() != Interval::NONE && ftpWatts > 0)
            nextW = qRound(nxt.getFTP_start() * ftpWatts);
        if (nxt.getCadenceStepType() != Interval::NONE)
            nextCad = nxt.getCadence_start();
    }
    m_controller->setNextInterval(nextW, nextCad, secsToNext);
}

QVariantList WorkoutRaceView::workoutProfile(const Workout &workout)
{
    const QList<Interval> &ivs = workout.getLstInterval();
    QVector<double> startSec, durSec, fStart, fEnd;
    double acc = 0.0;
    for (const Interval &iv : ivs) {
        const double d = Util::convertQTimeToSecD(iv.getDurationQTime());
        startSec.append(acc);
        durSec.append(d);
        const double s = qMax(0.0, iv.getFTP_start());
        double e = iv.getFTP_end();
        e = (e <= 0.0) ? s : e;
        fStart.append(s);
        fEnd.append(e);
        acc += d;
    }
    const double totalSec = (acc > 0.0) ? acc : 1.0;

    const int N = 120;
    QVector<double> raw(N, 0.0);
    double maxF = 0.01;
    for (int i = 0; i < N; ++i) {
        const double t = (i + 0.5) / N * totalSec;
        for (int k = 0; k < durSec.size(); ++k) {
            if (t < startSec[k] + durSec[k] || k == durSec.size() - 1) {
                const double f = (durSec[k] > 0.0)
                               ? qBound(0.0, (t - startSec[k]) / durSec[k], 1.0) : 0.0;
                raw[i] = fStart[k] + f * (fEnd[k] - fStart[k]);
                break;
            }
        }
        maxF = qMax(maxF, raw[i]);
    }

    QVariantList profile;
    for (double v : raw)
        profile.append(v / maxF);
    return profile;
}
