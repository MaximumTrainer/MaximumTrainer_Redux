#ifndef GROUPRIDECOORDINATOR_H
#define GROUPRIDECOORDINATOR_H

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <functional>
#include <optional>

#include "groupprotocol.h"

class GroupSession;

/// Drives one rider's side of a group ride from inside a workout: waits for
/// the scheduled start (with a countdown), starts the ride, and sends this
/// rider's telemetry once a second. Widget-free; the workout dialog reacts to
/// its signals and supplies the ride state through a snapshot callback.
class GroupRideCoordinator : public QObject
{
    Q_OBJECT

public:
    /// The workout's state at the moment telemetry is sent.
    struct RideSnapshot {
        bool started = false;
        bool paused = false;
        bool over = false;
        double elapsedSeconds = 0.0;
        int intervalIndex = 0;
        int targetPowerWatts = 0;   // <= 0: no power target
        int ftpWatts = 0;
    };

    static constexpr int kStartDelayMs = 10000;
    static constexpr qint64 kSensorFreshMs = 3000;
    static constexpr int kCountdownTickMs = 250;

    GroupRideCoordinator(GroupSession *session, std::function<RideSnapshot()> snapshot,
                         QObject *parent = nullptr);

    /// Marks this rider ready and catches up with a start that is already
    /// scheduled. Call once the signals below are connected.
    void begin();

    bool awaitingStart() const { return m_awaitingStart; }
    /// Local wall-clock start time, or 0 while no start is scheduled.
    qint64 scheduledStartLocalMs() const { return m_startLocalMs; }
    bool isHost() const;

    /// Host: schedule the start kStartDelayMs from now, unless already scheduled.
    void requestStart();

    void recordPower(int watts);
    void recordHeartRate(int bpm);
    void recordCadence(int rpm);

signals:
    void startScheduled(qint64 localStartMs);
    /// The scheduled start has arrived (or had already passed): ride now.
    void rideStarted();
    void telemetrySent(const GroupProtocol::RiderTelemetry &telemetry);
    /// Something the countdown / roster message shows has changed.
    void statusChanged();

private:
    struct Sample { int value = 0; qint64 atMs = 0; };

    void onStartScheduled(qint64 localStartMs);
    void beginRide();
    void sendTelemetry();
    static void record(Sample &sample, int value);

    QPointer<GroupSession> m_session;
    std::function<RideSnapshot()> m_snapshot;
    QTimer m_startTimer;
    QTimer m_countdownTimer;
    QTimer m_telemetryTimer;
    bool m_awaitingStart = true;
    qint64 m_startLocalMs = 0;
    Sample m_power;
    Sample m_heartRate;
    Sample m_cadence;
};

#endif // GROUPRIDECOORDINATOR_H
