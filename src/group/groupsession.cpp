#include "groupsession.h"

#include <QDateTime>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcGroup, "mt.group")

using namespace GroupProtocol;

namespace {

constexpr int kClockProbesOnJoin = 5;
constexpr int kClockProbeSpacingMs = 200;
constexpr int kClockResyncIntervalMs = 60 * 1000;
constexpr int kMaxReconnectDelayMs = 15 * 1000;
constexpr qint64 kStartResyncThresholdMs = 10;
constexpr qint64 kReadyResendMinIntervalMs = 2000;

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

bool isFatalJoinError(const QString &errorCode) {
    return errorCode == QLatin1String("room_not_found")
        || errorCode == QLatin1String("room_full")
        || errorCode == QLatin1String("protocol_mismatch");
}

} // namespace

GroupSession::GroupSession(const QUrl &relayBaseUrl, QObject *parent)
    : QObject(parent), m_relayBaseUrl(relayBaseUrl)
{
    connect(&m_socket, &QWebSocket::connected, this, &GroupSession::onConnected);
    connect(&m_socket, &QWebSocket::disconnected, this, &GroupSession::onDisconnected);
    connect(&m_socket, &QWebSocket::textMessageReceived, this, &GroupSession::onTextMessage);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &GroupSession::onReconnectTimer);
    connect(&m_clockProbeTimer, &QTimer::timeout, this, &GroupSession::onClockProbeTimer);
}

GroupSession::~GroupSession() {
    m_leaving = true;
    m_socket.disconnect(this);
    m_socket.abort();
}

void GroupSession::hostRoom(const QString &displayName) {
    m_displayName = displayName;
    openSocket(createRoomUrl(m_relayBaseUrl, displayName));
}

void GroupSession::joinRoom(const QString &roomCode, const QString &displayName) {
    m_displayName = displayName;
    m_roomCode = roomCode;
    openSocket(joinRoomUrl(m_relayBaseUrl, roomCode, displayName));
}

void GroupSession::leave() {
    m_leaving = true;
    m_reconnectTimer.stop();
    m_clockProbeTimer.stop();
    if (m_socket.state() == QAbstractSocket::ConnectedState) {
        m_socket.sendTextMessage(QString::fromUtf8(encodeLeave()));
        m_socket.close();
    } else {
        m_socket.abort();
    }
    setState(State::Closed);
}

void GroupSession::setReady(bool isReady, const SharedSensors &sensors) {
    m_lastReady = isReady;
    m_lastSensors = sensors;
    m_readyRequested = true;
    m_lastReadySentMs = nowMs();
    qCInfo(lcGroup) << "ready" << isReady << "in" << m_roomCode;
    send(encodeReady(isReady, sensors));
}

void GroupSession::shareWorkout(const SharedWorkout &workout) {
    send(encodeWorkout(workout));
}

void GroupSession::startIn(int delayMs) {
    if (!hasClockOffset()) {
        emit relayError(QStringLiteral("clock_not_ready"));
        return;
    }
    send(encodeStart(localToServerMs(nowMs()) + delayMs));
}

void GroupSession::sendTelemetry(const RiderTelemetry &telemetry) {
    if (m_state != State::Connected) return;
    send(encodeTelemetry(telemetry));
}

void GroupSession::openSocket(const QUrl &url) {
    m_leaving = false;
    setState(m_everJoined ? State::Reconnecting : State::Connecting);
    m_socket.abort();
    m_socket.open(url);
}

void GroupSession::setState(State state) {
    if (m_state == state) return;
    m_state = state;
    emit stateChanged(state);
}

void GroupSession::send(const QByteArray &frame) {
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        qCWarning(lcGroup) << "dropped" << frame.left(40) << "- socket state" << m_socket.state();
        return;
    }
    m_socket.sendTextMessage(QString::fromUtf8(frame));
}

void GroupSession::onConnected() {
    m_reconnectAttempt = 0;
}

void GroupSession::onDisconnected() {
    m_clockProbeTimer.stop();
    qCInfo(lcGroup) << "disconnected from" << m_roomCode << "code" << m_socket.closeCode()
                    << m_socket.closeReason() << "leaving" << m_leaving;
    if (m_leaving) { setState(State::Closed); return; }

    // Refusals arrive as an error frame (handled in onTextMessage), so a first
    // connection that drops without a welcome means the relay is unreachable.
    if (!m_everJoined) {
        emit relayError(QStringLiteral("relay_unreachable"));
        setState(State::Closed);
        return;
    }

    setState(State::Reconnecting);
    const int delayMs = qMin(kMaxReconnectDelayMs, 1000 << qMin(m_reconnectAttempt, 4));
    ++m_reconnectAttempt;
    m_reconnectTimer.start(delayMs);
}

void GroupSession::onReconnectTimer() {
    openSocket(joinRoomUrl(m_relayBaseUrl, m_roomCode, m_displayName, m_rejoinToken));
}

void GroupSession::onTextMessage(const QString &frame) {
    const IncomingMessage message = decode(frame.toUtf8());
    switch (message.type) {
    case MessageType::Welcome:
        handleWelcome(message.welcome);
        break;
    case MessageType::Roster: {
        m_roster = message.roster;
        m_isHost = message.hostRiderId == m_riderId;
        resendReadyIfRelayDisagrees();
        emit rosterChanged(m_roster);
        break;
    }
    case MessageType::Workout:
        emit workoutReceived(message.workout);
        break;
    case MessageType::Start:
        m_announcedStartServerMs = message.startAtServerMs;
        emit startScheduled(serverToLocalMs(message.startAtServerMs));
        break;
    case MessageType::Telemetry:
        emit telemetryReceived(message.telemetry);
        break;
    case MessageType::Pong: {
        ClockSample sample = message.pong;
        sample.localReceiveMs = nowMs();
        handlePong(sample);
        break;
    }
    case MessageType::Error:
        qCWarning(lcGroup) << "relay error" << message.errorCode << message.errorDetail;
        emit relayError(message.errorCode);
        if (isFatalJoinError(message.errorCode)) leave();
        break;
    case MessageType::Keepalive:
    case MessageType::Unknown:
        break;
    }
}

void GroupSession::handleWelcome(const Welcome &welcome) {
    const bool isRejoin = m_everJoined;
    m_everJoined = true;
    m_roomCode = welcome.roomCode;
    m_riderId = welcome.yourRiderId;
    m_rejoinToken = welcome.rejoinToken;
    m_isHost = welcome.isHost;
    qCInfo(lcGroup) << (isRejoin ? "rejoined" : "joined") << m_roomCode << "as" << m_riderId
                    << (m_isHost ? "(host)" : "");
    setState(State::Connected);

    // A rough offset straight away so a start can be shown before probing ends.
    if (!m_serverOffsetMs) m_serverOffsetMs = welcome.serverMs - nowMs();
    m_clockSamples.clear();
    m_clockProbesRemaining = kClockProbesOnJoin;
    sendNextClockProbe();
    m_clockProbeTimer.start(kClockProbeSpacingMs);

    if (!isRejoin) emit joined(m_roomCode, m_isHost);
    if (isRejoin && m_readyRequested) send(encodeReady(m_lastReady, m_lastSensors));
    if (welcome.workout) emit workoutReceived(*welcome.workout);
    if (welcome.startAtServerMs && welcome.startAtServerMs != m_announcedStartServerMs) {
        m_announcedStartServerMs = welcome.startAtServerMs;
        emit startScheduled(serverToLocalMs(*welcome.startAtServerMs));
    }
}

void GroupSession::onClockProbeTimer() {
    if (m_clockProbesRemaining > 0) {
        sendNextClockProbe();
        if (m_clockProbesRemaining == 0) m_clockProbeTimer.start(kClockResyncIntervalMs);
        return;
    }
    // Periodic burst to follow clock drift over a long ride.
    m_clockSamples.clear();
    m_clockProbesRemaining = kClockProbesOnJoin;
    sendNextClockProbe();
    m_clockProbeTimer.start(kClockProbeSpacingMs);
}

void GroupSession::sendNextClockProbe() {
    --m_clockProbesRemaining;
    send(encodePing(nowMs()));
}

void GroupSession::handlePong(ClockSample sample) {
    m_clockSamples.append(sample);
    m_lastRoundTripMs = sample.localReceiveMs - sample.localSendMs;
    const std::optional<qint64> offset = estimateServerOffsetMs(m_clockSamples);
    if (!offset) return;
    const qint64 previousOffset = m_serverOffsetMs.value_or(*offset);
    m_serverOffsetMs = offset;
    emit clockSynchronized(*offset, m_lastRoundTripMs);

    // A better estimate moves a pending shared start, so a slow first sample
    // can't leave this rider starting late.
    if (m_announcedStartServerMs && qAbs(*offset - previousOffset) >= kStartResyncThresholdMs) {
        const qint64 localStartMs = serverToLocalMs(*m_announcedStartServerMs);
        if (localStartMs > nowMs()) emit startScheduled(localStartMs);
    }
}

void GroupSession::resendReadyIfRelayDisagrees() {
    if (!m_readyRequested) return;
    for (const RosterEntry &entry : std::as_const(m_roster)) {
        if (entry.riderId != m_riderId || entry.isReady == m_lastReady) continue;
        const qint64 now = nowMs();
        if (now - m_lastReadySentMs < kReadyResendMinIntervalMs) return;
        m_lastReadySentMs = now;
        qCWarning(lcGroup) << "relay shows ready" << entry.isReady << "but we are" << m_lastReady << "- resending";
        send(encodeReady(m_lastReady, m_lastSensors));
        return;
    }
}
