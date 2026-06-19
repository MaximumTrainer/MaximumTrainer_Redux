#include "retroracecontroller.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtGlobal>

namespace {
// ERG team-race tuning. In ERG the rider can't out-power the target, so catching
// back relies on drafting; the partner is leashed so it never drops you.
constexpr double kDraftRangeM     = 12.0;  // slipstream reaches this far back
constexpr double kDraftMaxCdaCut  = 0.35;  // up to 35% less drag right on the wheel
constexpr double kDraftMinGapM    = 1.0;   // no draft within this of the wheel, so the
                                           // slipstream can't fling you straight past
constexpr double kPartnerLeashM   = 15.0;  // partner never pulls more than this ahead
constexpr double kPacerDraftMinM  = 5.0;   // once the pacer is dropped this far back it
                                           // drafts too, clawing back so you can't shake it

// A ghost shorter than this is not a usable opponent: racing it would cross the
// finish within the first tick and pop the celebration instantly. Reject it so
// we fall back to the pacer instead. (A real "race your last ride" recording is
// the full workout length — only aborted / corrupt .fit files fall under these.)
constexpr double kMinGhostSec  = 20.0;
constexpr double kMinGhostDistM = 50.0;

// The History folder the app actually writes rides to. On Windows with OneDrive
// the Documents folder is redirected (e.g. .../OneDrive/Documents/...), so a
// hardcoded homePath()/Documents path misses every saved ride. QStandardPaths
// resolves the real (redirected) location — matching Util::getSystemPath* — and
// we avoid including the heavy util.h here (this module also builds for WASM).
QString ghostHistoryDir()
{
    const QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString base = !docs.isEmpty() ? docs : (QDir::homePath() + QLatin1String("/Documents"));
    return base + QLatin1String("/MaximumTrainer/History");
}
}

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
        const QFileInfoList fits = QDir(ghostHistoryDir()).entryInfoList(
            QStringList() << QStringLiteral("*.fit"), QDir::Files, QDir::Time);
        if (!fits.isEmpty())
            path = fits.first().absoluteFilePath();
    }
    if (path.isEmpty())
        return false;

    GhostReplay replay = GhostReplay::fromFitFile(path);
    if (!replay.isValid())
        return false;
    // Guard against a degenerate ride (a couple of near-identical records): it
    // would "finish" instantly and show the win/celebration the moment the race
    // begins. Treat it as no-ghost so the caller falls back to the pacer.
    if (replay.totalTimeSec() < kMinGhostSec || replay.totalDistanceM() < kMinGhostDistM)
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
    // entryInfoList(QDir::Time) is newest-first, so the first match is the most
    // recent ride of this workout.
    const QFileInfoList fits = QDir(ghostHistoryDir()).entryInfoList(
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
    m_oppName  = QStringLiteral("Pace Partner");
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
    const QFileInfoList fits = QDir(ghostHistoryDir()).entryInfoList(
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
    m_finishSecs     = -1.0;
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
    // Pacer draft-back: once the bot pacer has been dropped beyond kPacerDraftMinM
    // it gets its own slipstream cut so it can claw the gap back, keeping the race
    // engaging — you can pull a small lead but never simply ride away from it. A
    // recorded ghost (your past self) stays honest and gets no help.
    double oppCda = m_cda;
    if (m_oppIsBot) {
        const double oppBehindM = m_playerDistM - m_oppDistM;   // >0 = pacer behind you
        if (oppBehindM > kPacerDraftMinM) {
            const double f = qBound(0.0, (oppBehindM - kPacerDraftMinM) / kDraftRangeM, 1.0);
            oppCda = m_cda * (1.0 - kDraftMaxCdaCut * f);
        }
    }
    m_oppV      = CyclingPhysics::stepSpeed(m_oppV, m_oppPowerW, m_weightKg, oppCda, dt);
    m_oppDistM += m_oppV * dt;
    m_oppCadence  = m_opponent->cadenceAt(m_elapsedSec);
    m_oppCrankRev += (m_oppCadence / 60.0) * dt;

    // Player: live trainer power if fed, else a synthetic demo pace derived from
    // the opponent so the two trade places convincingly without hardware.
    m_playerPowerW = (m_livePowerW >= 0.0)
                   ? m_livePowerW
                   : m_oppPowerW * m_playerForm;

    // Drafting: when you're behind, the partner's slipstream lowers your drag so
    // the SAME watts carry you faster until you're back on the wheel — it never
    // touches your power, only your speed. This is the honest way to claw back a
    // gap in ERG (where you can't out-power the target) and, just as importantly,
    // during a free-ride test interval — slope mode is forced there so you can
    // find your true FTP, and surging past the pacer to catch it would corrupt
    // the result. So draft applies whenever you're chasing the cooperative bot
    // pacer (any mode), plus the ERG ghost race; a slope-mode ghost stays a pure
    // effort-vs-effort contest with no draft.
    double playerCda = m_cda;
    m_drafting    = false;
    m_draftFactor = 0.0;
    if (m_ergMode || m_oppIsBot) {
        const double behindM = m_oppDistM - m_playerDistM;   // >0 = player behind
        // Only engage past a 1 m dead-zone: right on the wheel the slipstream is
        // strongest, and letting it apply there carries you straight past the
        // pacer, so you keep overtaking and dropping back. Holding draft to >1 m
        // lets you tuck in and settle on the wheel instead.
        if (behindM > kDraftMinGapM) {
            m_draftFactor = qBound(0.0, (kDraftRangeM - behindM) / kDraftRangeM, 1.0);
            playerCda     = m_cda * (1.0 - kDraftMaxCdaCut * m_draftFactor);
            m_drafting    = m_draftFactor > 0.05;
        }
    }
    m_playerV     = CyclingPhysics::stepSpeed(m_playerV, m_playerPowerW, m_weightKg, playerCda, dt);
    m_playerDistM += m_playerV * dt;

    // Partner leash: a cooperative pace partner never drops you. If it would pull
    // more than kPartnerLeashM ahead it soft-pedals (eases) so you can always get
    // back into draft range — including in a slope-mode test, where without the
    // leash you could fall past draft range and never claw back. Only the bot
    // partner waits — a recorded ghost (your past self) stays honest.
    m_partnerEasing = false;
    if (m_oppIsBot) {
        const double leadM = m_oppDistM - m_playerDistM;
        if (leadM > kPartnerLeashM) {
            m_oppDistM      = m_playerDistM + kPartnerLeashM;
            m_oppV          = qMin(m_oppV, m_playerV);   // stop pulling away
            m_oppPowerW    *= 0.7;                        // shown as easing on the HUD
            m_partnerEasing = true;
        }
    }

    // Scenery scroll is amplified with speed for a stronger sense of speed:
    // ~1× at/under 20 km/h, ramping to ~2.4× at high speed.
    const double kmh  = m_playerV * 3.6;
    const double gain = qBound(1.0, 1.0 + (kmh - 20.0) * 0.055, 2.4);
    m_visualSpeed = m_playerV * gain;
    m_visualDist += m_visualSpeed * dt;
    // Legs: live cadence when a sensor feeds it.  With live power but no
    // cadence feed (yet), infer pedalling from power so the legs don't spin
    // while the rider is stopped (0 W).  Only the pure demo — no hardware at
    // all — mirrors the opponent's cadence.
    if (m_liveCadenceRpm >= 0.0)
        m_playerCadence = m_liveCadenceRpm;
    else if (m_livePowerW >= 0.0)
        m_playerCadence = (m_playerPowerW > 5.0) ? m_oppCadence : 0.0;
    else
        m_playerCadence = m_oppCadence;
    m_playerCrankRev += (m_playerCadence / 60.0) * dt;

    emit updated();

    const double total = m_opponent->totalTimeSec();
    if (total > 1.0 && m_elapsedSec >= total)
        finishRace();   // ghost route completed → celebrate
}
