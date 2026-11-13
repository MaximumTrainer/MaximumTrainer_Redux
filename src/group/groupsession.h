#ifndef GROUPSESSION_H
#define GROUPSESSION_H

#include "groupprotocol.h"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>

/// One rider's connection to a group-workout room on the relay. Owns the
/// socket, clock-offset estimation and reconnects; knows nothing about the UI
/// or the trainer, so losing the relay never affects the local workout.
class GroupSession : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Connecting, Connected, Reconnecting, Closed };
    Q_ENUM(State)

    explicit GroupSession(const QUrl &relayBaseUrl, QObject *parent = nullptr);
    ~GroupSession() override;

    void hostRoom(const QString &displayName);
    void joinRoom(const QString &roomCode, const QString &displayName);
    void leave();

    void setReady(bool isReady, const GroupProtocol::SharedSensors &sensors);
    void shareWorkout(const GroupProtocol::SharedWorkout &workout);   // host only
    void startIn(int delayMs);                                       // host only
    void sendTelemetry(const GroupProtocol::RiderTelemetry &telemetry);
    /// Kill the socket without a close handshake, as a network drop would.
    void dropConnectionForTesting() { m_socket.abort(); }

    State state() const { return m_state; }
    bool isHost() const { return m_isHost; }
    QString roomCode() const { return m_roomCode; }
    QString riderId() const { return m_riderId; }
    bool hasClockOffset() const { return m_serverOffsetMs.has_value(); }
    qint64 serverOffsetMs() const { return m_serverOffsetMs.value_or(0); }
    qint64 lastRoundTripMs() const { return m_lastRoundTripMs; }
    const QList<GroupProtocol::RosterEntry> &roster() const { return m_roster; }
    /// Local epoch ms of the shared start, if the host has started the room.
    std::optional<qint64> scheduledStartLocalMs() const {
        if (!m_announcedStartServerMs) return std::nullopt;
        return serverToLocalMs(*m_announcedStartServerMs);
    }

    qint64 localToServerMs(qint64 localMs) const { return localMs + serverOffsetMs(); }
    qint64 serverToLocalMs(qint64 serverMs) const { return serverMs - serverOffsetMs(); }

signals:
    void stateChanged(GroupSession::State state);
    void joined(const QString &roomCode, bool isHost);
    void rosterChanged(const QList<GroupProtocol::RosterEntry> &roster);
    void workoutReceived(const GroupProtocol::SharedWorkout &workout);
    /// Local epoch ms at which the shared start happens on this machine. Emitted
    /// again with a corrected time if the clock estimate improves before it.
    void startScheduled(qint64 localStartMs);
    void telemetryReceived(const GroupProtocol::RiderTelemetry &telemetry);
    void clockSynchronized(qint64 serverOffsetMs, qint64 roundTripMs);
    /// Relay refused or reported a problem ("room_not_found", "room_full", "host_only", ...).
    void relayError(const QString &errorCode);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessage(const QString &frame);
    void onReconnectTimer();
    void onClockProbeTimer();

private:
    void openSocket(const QUrl &url);
    void setState(State state);
    void send(const QByteArray &frame);
    void handleWelcome(const GroupProtocol::Welcome &welcome);
    void handlePong(GroupProtocol::ClockSample sample);
    void sendNextClockProbe();
    void resendReadyIfRelayDisagrees();

    QUrl m_relayBaseUrl;
    QWebSocket m_socket;
    State m_state = State::Idle;
    QString m_displayName;
    QString m_roomCode;
    QString m_riderId;
    QString m_rejoinToken;
    bool m_isHost = false;
    bool m_leaving = false;
    bool m_everJoined = false;
    int m_reconnectAttempt = 0;
    QTimer m_reconnectTimer;
    QTimer m_clockProbeTimer;
    QList<GroupProtocol::ClockSample> m_clockSamples;
    int m_clockProbesRemaining = 0;
    std::optional<qint64> m_serverOffsetMs;
    qint64 m_lastRoundTripMs = -1;
    std::optional<qint64> m_announcedStartServerMs;
    bool m_lastReady = false;
    bool m_readyRequested = false;
    qint64 m_lastReadySentMs = 0;
    GroupProtocol::SharedSensors m_lastSensors;
    QList<GroupProtocol::RosterEntry> m_roster;
};

#endif // GROUPSESSION_H
