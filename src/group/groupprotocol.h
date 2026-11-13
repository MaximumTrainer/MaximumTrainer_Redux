#ifndef GROUPPROTOCOL_H
#define GROUPPROTOCOL_H

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

/// Wire format shared with workers/group-relay/worker.js. Pure functions only,
/// so everything here is unit-tested without a socket.
namespace GroupProtocol {

constexpr int kProtocolVersion = 1;
constexpr int kMaxRiders = 20;
constexpr int kTelemetryIntervalMs = 1000;

enum class RiderStatus { Riding, Paused, Done };

struct RiderTelemetry {
    QString riderId;                     // filled by the relay on receipt
    double elapsedWorkoutSeconds = 0.0;
    std::optional<int> powerWatts;
    std::optional<int> targetPowerWatts;
    std::optional<int> heartRateBpm;
    std::optional<int> cadenceRpm;
    int intervalIndex = 0;
    RiderStatus status = RiderStatus::Riding;
};

struct SharedSensors {
    bool power = false;
    bool heartRate = false;
    bool cadence = false;
};

struct RosterEntry {
    QString riderId;
    QString name;
    bool isHost = false;
    bool isReady = false;
    SharedSensors sensors;
};

struct SharedWorkout {
    QString name;
    QByteArray xml;
};

struct Welcome {
    QString roomCode;
    QString yourRiderId;
    QString rejoinToken;
    bool isHost = false;
    qint64 serverMs = 0;
    int maxRiders = kMaxRiders;
    std::optional<qint64> startAtServerMs;
    std::optional<SharedWorkout> workout;
};

struct ClockSample {
    qint64 localSendMs = 0;
    qint64 localReceiveMs = 0;
    qint64 serverMs = 0;
};

enum class MessageType { Unknown, Welcome, Roster, Workout, Start, Telemetry, Pong, Error, Keepalive };

struct IncomingMessage {
    MessageType type = MessageType::Unknown;
    Welcome welcome;
    QList<RosterEntry> roster;
    QString hostRiderId;
    SharedWorkout workout;
    qint64 startAtServerMs = 0;
    RiderTelemetry telemetry;
    ClockSample pong;            // localSendMs + serverMs; caller fills localReceiveMs
    QString errorCode;
    QString errorDetail;
};

QString normalizeRoomCode(const QString &userInput);   // "grp-7kq2" -> "7KQ2", empty if invalid

QUrl createRoomUrl(const QUrl &relayBaseUrl, const QString &displayName);
QUrl joinRoomUrl(const QUrl &relayBaseUrl, const QString &roomCode,
                 const QString &displayName, const QString &rejoinToken = QString());

QByteArray encodePing(qint64 localSendMs);
QByteArray encodeReady(bool isReady, const SharedSensors &sensors);
QByteArray encodeWorkout(const SharedWorkout &workout);
QByteArray encodeStart(qint64 startAtServerMs);
QByteArray encodeTelemetry(const RiderTelemetry &telemetry);
QByteArray encodeLeave();

IncomingMessage decode(const QByteArray &frame);

QString statusToWire(RiderStatus status);
RiderStatus statusFromWire(const QString &wire);

/// Offset to add to the local clock to get server time, taken from the sample
/// with the shortest round trip (the least queueing noise). nullopt if empty.
std::optional<qint64> estimateServerOffsetMs(const QList<ClockSample> &samples);

} // namespace GroupProtocol

#endif // GROUPPROTOCOL_H
