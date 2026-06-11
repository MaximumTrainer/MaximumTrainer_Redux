#include "retroracecontroller.h"

#include <QDir>
#include <QFileInfo>

RetroRaceController::RetroRaceController(QObject *parent)
    : QObject(parent)
{
    m_timer.setInterval(33);   // ~30 fps
    connect(&m_timer, &QTimer::timeout, this, &RetroRaceController::tick);
}

bool RetroRaceController::loadGhost(const QString &fitPath)
{
    QString path = fitPath;
    if (path.isEmpty()) {
        const QString histDir = QDir::homePath()
            + QLatin1String("/Documents/MaximumTrainer/History");
        const QFileInfoList fits = QDir(histDir).entryInfoList(
            QStringList() << QStringLiteral("*.fit"), QDir::Files, QDir::Time);
        if (!fits.isEmpty())
            path = fits.first().absoluteFilePath();
    }
    if (path.isEmpty())
        return false;

    GhostReplay replay = GhostReplay::fromFitFile(path);
    if (!replay.isValid())
        return false;

    m_oppName    = QFileInfo(path).completeBaseName();
    m_oppIsBot   = false;
    m_pacerLive  = nullptr;
    m_opponent   = std::make_unique<RecordedPowerSource>(std::move(replay));
    emit opponentChanged();
    return true;
}

bool RetroRaceController::loadGhostForWorkout(const QString &workoutName)
{
    const QString histDir = QDir::homePath()
        + QLatin1String("/Documents/MaximumTrainer/History");
    // entryInfoList(QDir::Time) is newest-first, so the first match is the most
    // recent ride of this workout.
    const QFileInfoList fits = QDir(histDir).entryInfoList(
        QStringList() << QStringLiteral("*.fit"), QDir::Files, QDir::Time);
    for (const QFileInfo &fi : fits) {
        if (workoutName.isEmpty()
            || fi.completeBaseName().contains(workoutName, Qt::CaseInsensitive)) {
            if (loadGhost(fi.absoluteFilePath()))
                return true;
        }
    }
    return false;
}

void RetroRaceController::useComputerPacer(double watts)
{
    m_oppIsBot  = true;
    m_oppName   = QStringLiteral("Pacer Bot · %1 W").arg(int(watts));
    m_pacerLive = nullptr;
    m_opponent  = std::make_unique<ConstantPowerSource>(watts);
    emit opponentChanged();
}

void RetroRaceController::useWorkoutPacer()
{
    m_oppIsBot = true;
    m_oppName  = QStringLiteral("Workout Pacer");
    auto src   = std::make_unique<LivePowerSource>(150.0);
    m_pacerLive = src.get();
    m_opponent  = std::move(src);
    emit opponentChanged();
}

void RetroRaceController::setPacerTargetWatts(double watts)
{
    // No-op unless the workout pacer is the active opponent.
    if (m_pacerLive && watts > 0.0)
        m_pacerLive->setWatts(watts);
}

void RetroRaceController::setWorkoutName(const QString &name)
{
    m_workoutName = name;

    // Does a past ride of this workout exist? (filename match only — cheap; the
    // actual decode happens if the user picks it.)
    m_hasLastRide = false;
    const QString histDir = QDir::homePath()
        + QLatin1String("/Documents/MaximumTrainer/History");
    const QFileInfoList fits = QDir(histDir).entryInfoList(
        QStringList() << QStringLiteral("*.fit"), QDir::Files, QDir::Time);
    for (const QFileInfo &fi : fits) {
        if (!name.isEmpty() && fi.completeBaseName().contains(name, Qt::CaseInsensitive)) {
            m_hasLastRide = true;
            break;
        }
    }
    emit opponentChanged();
}

bool RetroRaceController::chooseGhost()
{
    if (loadGhostForWorkout(m_workoutName)) {
        m_oppChosen = true;
        emit opponentChanged();
        return true;
    }
    choosePacer();   // couldn't load — fall back so the race is still playable
    return false;
}

void RetroRaceController::choosePacer()
{
    useWorkoutPacer();
    m_oppChosen = true;
    emit opponentChanged();
}

void RetroRaceController::start()
{
    m_elapsedSec  = 0.0;
    m_playerV     = 0.0;
    m_oppV        = 0.0;
    m_playerDistM = 0.0;
    m_oppDistM    = 0.0;
    m_playerPowerW = 0.0;
    m_oppPowerW    = 0.0;
    m_playerCadence  = 0.0;
    m_oppCadence     = 0.0;
    m_playerCrankRev = 0.0;
    m_oppCrankRev    = 0.0;
    m_visualDist     = 0.0;
    m_started  = false;
    m_finished = false;
    m_intervalMarks.clear();
    emit intervalMarksChanged();
    m_clock.restart();
    m_timer.start();   // running so legs can free-spin; no movement until beginRace()
    emit raceStateChanged();
    emit updated();
}

void RetroRaceController::beginRace()
{
    if (m_started || m_finished)
        return;
    if (!m_opponent)         // gun fired before a pick → default to the pacer
        choosePacer();
    m_started = true;
    m_clock.restart();          // don't fold the pre-start idle into the first dt
    if (!m_timer.isActive())
        m_timer.start();
    emit raceStateChanged();
    emit updated();
}

void RetroRaceController::finishRace()
{
    if (m_finished)
        return;
    m_finished = true;
    m_started  = false;
    m_timer.stop();
    emit raceStateChanged();
    emit updated();
}

void RetroRaceController::markIntervalBoundary()
{
    if (!m_started || m_finished)
        return;
    // Anchored in warped scenery distance so the marker stays locked to the road
    // decor (which also scrolls by visualDist).
    m_intervalMarks.append(m_visualDist);
    while (!m_intervalMarks.isEmpty()
           && m_visualDist - m_intervalMarks.first().toDouble() > 600.0)
        m_intervalMarks.removeFirst();
    emit intervalMarksChanged();
}

void RetroRaceController::setRacePaused(bool paused)
{
    if (m_finished)
        return;
    if (paused && m_timer.isActive())
        m_timer.stop();
    else if (!paused && !m_timer.isActive()) {
        m_clock.restart();
        m_timer.start();
    }
    emit updated();
}

void RetroRaceController::togglePause()
{
    if (m_timer.isActive()) {
        m_timer.stop();
    } else {
        m_clock.restart();
        m_timer.start();
    }
    emit updated();
}

void RetroRaceController::tick()
{
    const double dt = (m_clock.restart() / 1000.0) * m_timeScale;
    if (dt <= 0.0)
        return;

    // Before the gun (incl. while still choosing an opponent): riders sit at the
    // start line, but the player's legs free-spin from live cadence so you can
    // warm up / spin up on the line.
    if (!m_started) {
        if (m_liveCadenceRpm >= 0.0) {
            m_playerCadence = m_liveCadenceRpm;
            m_playerCrankRev += (m_playerCadence / 60.0) * dt;
        }
        emit updated();
        return;
    }
    if (!m_opponent) {
        emit updated();
        return;
    }

    m_elapsedSec += dt;

    // Opponent: its power source → speed → distance, plus cadence for the legs.
    m_oppPowerW = m_opponent->powerAt(m_elapsedSec);
    m_oppV      = CyclingPhysics::stepSpeed(m_oppV, m_oppPowerW, m_weightKg, m_cda, dt);
    m_oppDistM += m_oppV * dt;
    m_oppCadence  = m_opponent->cadenceAt(m_elapsedSec);
    m_oppCrankRev += (m_oppCadence / 60.0) * dt;

    // Player: live trainer power if fed, else a synthetic demo pace derived from
    // the opponent so the two trade places convincingly without hardware.
    m_playerPowerW = (m_livePowerW >= 0.0)
                   ? m_livePowerW
                   : m_oppPowerW * m_playerForm;
    m_playerV     = CyclingPhysics::stepSpeed(m_playerV, m_playerPowerW, m_weightKg, m_cda, dt);
    m_playerDistM += m_playerV * dt;

    // Scenery scroll is amplified with speed for a stronger sense of speed:
    // ~1× at/under 20 km/h, ramping to ~2.4× at high speed.
    const double kmh  = m_playerV * 3.6;
    const double gain = qBound(1.0, 1.0 + (kmh - 20.0) * 0.055, 2.4);
    m_visualSpeed = m_playerV * gain;
    m_visualDist += m_visualSpeed * dt;
    // Live cadence drives the legs; otherwise mirror the opponent's pace.
    m_playerCadence = (m_liveCadenceRpm >= 0.0) ? m_liveCadenceRpm : m_oppCadence;
    m_playerCrankRev += (m_playerCadence / 60.0) * dt;

    emit updated();

    const double total = m_opponent->totalTimeSec();
    if (total > 0.0 && m_elapsedSec >= total)
        finishRace();   // ghost route completed → celebrate
}
