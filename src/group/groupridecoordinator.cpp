#include "groupridecoordinator.h"

#include <QDateTime>
#include <QDebug>

#include "groupsession.h"

GroupRideCoordinator::GroupRideCoordinator(GroupSession *session,
                                           std::function<RideSnapshot()> snapshot,
                                           QObject *parent)
    : QObject(parent), m_session(session), m_snapshot(std::move(snapshot))
{
    m_startTimer.setSingleShot(true);
    m_startTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_startTimer, &QTimer::timeout, this, &GroupRideCoordinator::beginRide);

    m_countdownTimer.setInterval(kCountdownTickMs);
    connect(&m_countdownTimer, &QTimer::timeout, this, &GroupRideCoordinator::statusChanged);

    m_telemetryTimer.setInterval(GroupProtocol::kTelemetryIntervalMs);
    connect(&m_telemetryTimer, &QTimer::timeout, this, &GroupRideCoordinator::sendTelemetry);

    if (!session)
        return;
    connect(session, &GroupSession::startScheduled, this, &GroupRideCoordinator::onStartScheduled);
    connect(session, &GroupSession::rosterChanged, this, &GroupRideCoordinator::statusChanged);
    connect(session, &GroupSession::relayError, this, [](const QString &errorCode) {
        qWarning() << "Group relay error:" << errorCode;
    });
}

void GroupRideCoordinator::begin()
{
    if (!m_session)
        return;
    m_session->setReady(true, GroupProtocol::SharedSensors{true, true, true});
    if (const std::optional<qint64> alreadyScheduled = m_session->scheduledStartLocalMs())
        onStartScheduled(*alreadyScheduled);
}

bool GroupRideCoordinator::isHost() const
{
    return m_session && m_session->isHost();
}

void GroupRideCoordinator::requestStart()
{
    if (m_session && m_startLocalMs == 0)
        m_session->startIn(kStartDelayMs);
}

void GroupRideCoordinator::record(Sample &sample, int value)
{
    sample = { value, QDateTime::currentMSecsSinceEpoch() };
}

void GroupRideCoordinator::recordPower(int watts) { record(m_power, watts); }
void GroupRideCoordinator::recordHeartRate(int bpm) { record(m_heartRate, bpm); }
void GroupRideCoordinator::recordCadence(int rpm) { record(m_cadence, rpm); }

void GroupRideCoordinator::onStartScheduled(qint64 localStartMs)
{
    if (!m_awaitingStart)
        return;
    m_startLocalMs = localStartMs;
    emit startScheduled(localStartMs);

    const qint64 delayMs = localStartMs - QDateTime::currentMSecsSinceEpoch();
    if (delayMs <= 0) {
        // Joined after the start: ride from the beginning of the workout.
        beginRide();
        return;
    }
    m_startTimer.start(static_cast<int>(delayMs));
    m_countdownTimer.start();
    emit statusChanged();
}

void GroupRideCoordinator::beginRide()
{
    if (!m_awaitingStart)
        return;
    m_awaitingStart = false;
    m_startTimer.stop();
    m_countdownTimer.stop();
    emit rideStarted();
    m_telemetryTimer.start();
    sendTelemetry();
}

void GroupRideCoordinator::sendTelemetry()
{
    const RideSnapshot ride = m_snapshot ? m_snapshot() : RideSnapshot{};
    if (!ride.started)
        return;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const auto fresh = [nowMs](const Sample &sample) -> std::optional<int> {
        if (sample.atMs == 0 || nowMs - sample.atMs > kSensorFreshMs)
            return std::nullopt;
        return sample.value;
    };

    GroupProtocol::RiderTelemetry telemetry;
    telemetry.elapsedWorkoutSeconds = ride.elapsedSeconds;
    telemetry.intervalIndex = ride.intervalIndex;
    telemetry.status = ride.over ? GroupProtocol::RiderStatus::Done
                     : ride.paused ? GroupProtocol::RiderStatus::Paused
                                   : GroupProtocol::RiderStatus::Riding;
    telemetry.powerWatts = fresh(m_power);
    telemetry.heartRateBpm = fresh(m_heartRate);
    telemetry.cadenceRpm = fresh(m_cadence);
    if (ride.targetPowerWatts > 0)
        telemetry.targetPowerWatts = ride.targetPowerWatts;
    if (telemetry.powerWatts && ride.ftpWatts > 0)
        telemetry.percentFtp = qRound(100.0 * *telemetry.powerWatts / ride.ftpWatts);

    emit telemetrySent(telemetry);
    if (m_session)
        m_session->sendTelemetry(telemetry);
    if (ride.over)
        m_telemetryTimer.stop();
}
