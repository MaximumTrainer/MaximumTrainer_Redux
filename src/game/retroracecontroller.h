#ifndef RETRORACECONTROLLER_H
#define RETRORACECONTROLLER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantList>

#include <memory>

#include "cyclingphysics.h"

/*
 * RetroRaceController
 *
 * Drives the retro ghost-race scene (RetroRace.qml). Both riders are power →
 * speed → distance through CyclingPhysics, so it is a fair effort-vs-effort
 * race:
 *   - opponent: a PowerSource — your recorded ride, or a computer pacer when you
 *     have no history for this workout (useComputerPacer()).
 *   - player:   live trainer power via setLivePowerWatts(); with no hardware the
 *     spike synthesises a pace a bit stronger than the opponent.
 *
 * Exposed to QML as the context property `race`.
 */
class RetroRaceController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double  elapsedSec      READ elapsedSec      NOTIFY updated)
    Q_PROPERTY(double  playerDistanceM READ playerDistanceM NOTIFY updated)
    Q_PROPERTY(double  oppDistanceM    READ oppDistanceM    NOTIFY updated)
    Q_PROPERTY(double  gapMeters       READ gapMeters       NOTIFY updated)
    Q_PROPERTY(double  playerSpeedKmh  READ playerSpeedKmh  NOTIFY updated)
    Q_PROPERTY(double  oppSpeedKmh     READ oppSpeedKmh     NOTIFY updated)
    Q_PROPERTY(double  playerPowerW    READ playerPowerW    NOTIFY updated)
    Q_PROPERTY(double  oppPowerW       READ oppPowerW       NOTIFY updated)
    Q_PROPERTY(double  playerCadence   READ playerCadence   NOTIFY updated)
    Q_PROPERTY(double  oppCadence      READ oppCadence      NOTIFY updated)
    Q_PROPERTY(double  playerHr        READ playerHr        NOTIFY updated)
    // Workout targets (value ± range); <=0 target means "no target set".
    Q_PROPERTY(double  targetPower        READ targetPower        NOTIFY targetsChanged)
    Q_PROPERTY(double  targetPowerRange   READ targetPowerRange   NOTIFY targetsChanged)
    Q_PROPERTY(double  targetCadence      READ targetCadence      NOTIFY targetsChanged)
    Q_PROPERTY(double  targetCadenceRange READ targetCadenceRange NOTIFY targetsChanged)
    Q_PROPERTY(double  targetHr           READ targetHr           NOTIFY targetsChanged)
    Q_PROPERTY(double  targetHrRange      READ targetHrRange      NOTIFY targetsChanged)
    // Workout profile (normalised 0..1 target-power samples) + time progress, so
    // the game can show a built-in "what's next" strip and stand alone.
    Q_PROPERTY(QVariantList workoutProfile  READ workoutProfile  NOTIFY profileChanged)
    Q_PROPERTY(double       workoutProgress READ workoutProgress NOTIFY updated)
    Q_PROPERTY(int          workoutElapsedSec READ workoutElapsedSec NOTIFY updated)
    Q_PROPERTY(bool         gameFullscreen  READ gameFullscreen  NOTIFY fullscreenChanged)
    // The upcoming interval's targets + seconds until it starts (road preview).
    Q_PROPERTY(double nextTargetW   READ nextTargetW   NOTIFY nextChanged)
    Q_PROPERTY(double nextTargetCad READ nextTargetCad NOTIFY nextChanged)
    Q_PROPERTY(double nextSecs      READ nextSecs      NOTIFY nextChanged)
    Q_PROPERTY(QString intervalMessage READ intervalMessage NOTIFY nextChanged)
    // Whole-ride metrics (from DataWorkout) for the info panel.
    Q_PROPERTY(double np      READ np      NOTIFY metricsChanged)
    Q_PROPERTY(double ifactor READ ifactor NOTIFY metricsChanged)
    Q_PROPERTY(double tss     READ tss     NOTIFY metricsChanged)
    // Accumulated crank revolutions — drives the pedalling animation at real rpm.
    Q_PROPERTY(double  playerCrankRev  READ playerCrankRev  NOTIFY updated)
    Q_PROPERTY(double  oppCrankRev     READ oppCrankRev     NOTIFY updated)
    // Warped scroll distance for scenery: advances faster than real distance as
    // speed rises, so the world rushes by harder at 30+ km/h (pure visual feel).
    Q_PROPERTY(double  visualDist      READ visualDist      NOTIFY updated)
    Q_PROPERTY(bool    running         READ running         NOTIFY updated)
    Q_PROPERTY(bool    started         READ started         NOTIFY raceStateChanged)
    Q_PROPERTY(bool    finished        READ finished        NOTIFY raceStateChanged)
    // Player distances at which a new workout interval began (road markers).
    Q_PROPERTY(QVariantList intervalMarks READ intervalMarks NOTIFY intervalMarksChanged)
    Q_PROPERTY(QString oppName         READ oppName         NOTIFY opponentChanged)
    Q_PROPERTY(bool    oppIsBot        READ oppIsBot        NOTIFY opponentChanged)
    Q_PROPERTY(double  routeLengthM    READ routeLengthM    NOTIFY opponentChanged)
    Q_PROPERTY(bool    oppChosen       READ oppChosen       NOTIFY opponentChanged)
    Q_PROPERTY(bool    hasLastRide     READ hasLastRide     NOTIFY opponentChanged)

public:
    explicit RetroRaceController(QObject *parent = nullptr);

    /// Load the opponent from a past ride (.fit). Empty path → most recent ride
    /// in the local History folder. Returns true if a usable ghost was loaded.
    bool loadGhost(const QString &fitPath);
    /// Load the most recent past ride of a named workout (matched on the History
    /// filename). Returns true if a usable ghost was found.
    bool loadGhostForWorkout(const QString &workoutName);
    /// Use a constant-power "computer" opponent (the no-history fallback).
    void useComputerPacer(double watts);
    /// Use a pacer that follows the workout's target watts; feed the current
    /// interval target via setPacerTargetWatts() (so it ramps with the workout).
    void useWorkoutPacer();
    void setPacerTargetWatts(double watts);

    /// Remember which workout this is (for the "race your last ride" option) and
    /// precompute whether a matching past ride exists.
    void setWorkoutName(const QString &name);

    /// Opponent chosen by the user from the pre-start menu (QML-invokable).
    Q_INVOKABLE bool chooseGhost();    // race your last performance
    Q_INVOKABLE void choosePacer();    // race the workout pacer

    /// Rider physics inputs (from the Account). Non-positive values are ignored.
    void setRiderParams(double weightKg, double cda)
    {
        if (weightKg > 0.0) m_weightKg = weightKg;
        if (cda      > 0.0) m_cda      = cda;
    }
    /// Live racing (real time, power fed via setLivePowerWatts) vs the spike's
    /// time-compressed synthetic demo.
    void setLiveMode(bool live) { m_timeScale = live ? 1.0 : 10.0; }

    double  elapsedSec() const      { return m_elapsedSec; }
    double  playerDistanceM() const { return m_playerDistM; }
    double  oppDistanceM() const    { return m_oppDistM; }
    double  gapMeters() const       { return m_playerDistM - m_oppDistM; }
    double  playerSpeedKmh() const  { return m_playerV * 3.6; }
    double  oppSpeedKmh() const     { return m_oppV * 3.6; }
    double  playerPowerW() const    { return m_playerPowerW; }
    double  oppPowerW() const       { return m_oppPowerW; }
    double  playerCadence() const   { return m_playerCadence; }
    double  oppCadence() const      { return m_oppCadence; }
    double  playerHr() const        { return m_playerHr; }
    double  targetPower() const        { return m_tPower; }
    double  targetPowerRange() const   { return m_tPowerRange; }
    double  targetCadence() const      { return m_tCadence; }
    double  targetCadenceRange() const { return m_tCadenceRange; }
    double  targetHr() const           { return m_tHr; }
    double  targetHrRange() const      { return m_tHrRange; }
    QVariantList workoutProfile() const { return m_workoutProfile; }
    double  workoutProgress() const     { return m_workoutProgress; }
    int     workoutElapsedSec() const   { return m_workoutElapsedSec; }
    bool    gameFullscreen() const      { return m_gameFullscreen; }
    double  nextTargetW() const         { return m_nextW; }
    double  nextTargetCad() const       { return m_nextCad; }
    double  nextSecs() const            { return m_nextSecs; }
    QString intervalMessage() const     { return m_intervalMessage; }
    void    setIntervalMessage(const QString &m) { m_intervalMessage = m; emit nextChanged(); }
    double  np() const      { return m_np; }
    double  ifactor() const { return m_if; }
    double  tss() const     { return m_tss; }
    void    setNp(double v)  { m_np = v;  emit metricsChanged(); }
    void    setIf(double v)  { m_if = v;  emit metricsChanged(); }
    void    setTss(double v) { m_tss = v; emit metricsChanged(); }
    double  playerCrankRev() const  { return m_playerCrankRev; }
    double  oppCrankRev() const     { return m_oppCrankRev; }
    double  visualDist() const      { return m_visualDist; }
    bool    running() const         { return m_timer.isActive(); }
    bool    started() const         { return m_started; }
    bool    finished() const        { return m_finished; }
    QVariantList intervalMarks() const { return m_intervalMarks; }
    QString oppName() const         { return m_oppName; }
    bool    oppIsBot() const        { return m_oppIsBot; }
    double  routeLengthM() const    { return m_opponent ? m_opponent->totalDistanceM() : 0.0; }
    bool    oppChosen() const       { return m_oppChosen; }
    bool    hasLastRide() const     { return m_hasLastRide; }

public slots:
    void start();          // arm: riders idle at the start line, legs free-spin
    void beginRace();      // the gun — riders start moving (call on workout start)
    void finishRace();     // freeze + trigger the finish/celebration overlay
    void setRacePaused(bool paused);
    void markIntervalBoundary();   // drop a road marker at the player's position
    void togglePause();
    /// Feed live trainer power (watts). Negative → use the synthetic demo pace.
    void setLivePowerWatts(double watts) { m_livePowerW = watts; }
    /// Feed live trainer cadence (rpm) for the leg animation. Negative → ignore.
    void setLiveCadenceRpm(double rpm) { m_liveCadenceRpm = rpm; }
    /// Feed live heart rate (bpm) for the HUD.
    void setLiveHr(double bpm) { m_playerHr = bpm; emit updated(); }
    /// Push the current workout targets (value ± range; <=0 = no target).
    void setTargetPower(double watts, double range)   { m_tPower = watts; m_tPowerRange = range; emit targetsChanged(); }
    void setTargetCadence(double rpm, double range)   { m_tCadence = rpm; m_tCadenceRange = range; emit targetsChanged(); }
    void setTargetHr(double bpm, double range)        { m_tHr = bpm; m_tHrRange = range; emit targetsChanged(); }
    void setWorkoutProfile(const QVariantList &p)     { m_workoutProfile = p; emit profileChanged(); }
    void setWorkoutProgress(double frac)              { m_workoutProgress = frac; emit updated(); }
    void setWorkoutElapsedSec(int s)                  { m_workoutElapsedSec = s; emit updated(); }
    void setGameFullscreen(bool fs)                   { if (m_gameFullscreen != fs) { m_gameFullscreen = fs; emit fullscreenChanged(); } }
    /// QML asks the host (workout dialog) to collapse/restore the data panes.
    Q_INVOKABLE void requestFullscreenToggle()        { emit fullscreenToggleRequested(); }
    /// Upcoming interval preview: target watts, cadence (<=0 = none) and seconds
    /// until it starts (<0 = no next interval).
    void setNextInterval(double watts, double cad, double secs)
    { m_nextW = watts; m_nextCad = cad; m_nextSecs = secs; emit nextChanged(); }

signals:
    void updated();
    void opponentChanged();
    void raceStateChanged();
    void intervalMarksChanged();
    void targetsChanged();
    void profileChanged();
    void fullscreenChanged();
    void fullscreenToggleRequested();
    void nextChanged();
    void metricsChanged();

private:
    void tick();

    std::unique_ptr<PowerSource> m_opponent;
    LivePowerSource *m_pacerLive = nullptr;   // non-owning; valid only for the workout pacer
    QString m_oppName;
    QString m_workoutName;
    bool    m_oppIsBot     = false;
    bool    m_oppChosen    = false;
    bool    m_hasLastRide  = false;

    QTimer        m_timer;
    QElapsedTimer m_clock;

    // Rider physics inputs (spike defaults; wire to the Account later).
    double m_weightKg = 80.0;
    double m_cda      = 0.35;

    // Spike-only: compress time so a long ride is watchable, and pace the demo
    // player a touch stronger than the opponent. Both go away with live power.
    double m_timeScale  = 10.0;
    double m_playerForm = 1.08;

    double m_elapsedSec  = 0.0;
    double m_playerV     = 0.0;   // m/s
    double m_oppV        = 0.0;   // m/s
    double m_playerDistM = 0.0;
    double m_oppDistM    = 0.0;
    double m_playerPowerW = 0.0;
    double m_oppPowerW    = 0.0;
    double m_livePowerW   = -1.0; // <0 = synthetic demo source
    double m_playerCadence  = 0.0;
    double m_oppCadence     = 0.0;
    double m_playerCrankRev = 0.0;
    double m_oppCrankRev    = 0.0;
    double m_visualDist     = 0.0;   // warped scroll distance (speed-amplified)
    double m_liveCadenceRpm = -1.0; // <0 = derive cadence from the opponent
    double m_playerHr       = 0.0;
    double m_tPower = -1.0, m_tPowerRange = 0.0;
    double m_tCadence = -1.0, m_tCadenceRange = 0.0;
    double m_tHr = -1.0, m_tHrRange = 0.0;
    bool   m_started  = false;      // gun fired (riders rolling)
    bool   m_finished = false;
    QVariantList m_intervalMarks;   // player distances where intervals began
    QVariantList m_workoutProfile;  // normalised target-power samples (0..1)
    double m_workoutProgress = 0.0; // 0..1 through the workout
    int    m_workoutElapsedSec = 0;
    bool   m_gameFullscreen  = false;
    double m_nextW = -1.0, m_nextCad = -1.0, m_nextSecs = -1.0;
    QString m_intervalMessage;
    double  m_np = 0.0, m_if = 0.0, m_tss = 0.0;
};

#endif // RETRORACECONTROLLER_H
