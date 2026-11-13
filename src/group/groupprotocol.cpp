#include "groupprotocol.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>

#include <limits>

namespace GroupProtocol {

namespace {

const QString kCodeAlphabet = QStringLiteral("0123456789ABCDEFGHJKMNPQRSTVWXYZ");
constexpr int kCodeLength = 4;

QByteArray toFrame(const QJsonObject &object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QUrl wsUrl(const QUrl &relayBaseUrl, const QUrlQuery &query) {
    QUrl url(relayBaseUrl);
    QString path = url.path();
    if (path.endsWith(QLatin1Char('/'))) path.chop(1);
    url.setPath(path + QStringLiteral("/ws"));
    url.setQuery(query);
    return url;
}

std::optional<int> optionalInt(const QJsonObject &object, const QString &key) {
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) return std::nullopt;
    return qRound(value.toDouble());
}

SharedSensors sensorsFromJson(const QJsonObject &object) {
    SharedSensors sensors;
    sensors.power = object.value(QStringLiteral("power")).toBool();
    sensors.heartRate = object.value(QStringLiteral("hr")).toBool();
    sensors.cadence = object.value(QStringLiteral("cad")).toBool();
    return sensors;
}

SharedWorkout workoutFromJson(const QJsonObject &object) {
    SharedWorkout workout;
    workout.name = object.value(QStringLiteral("name")).toString();
    workout.xml = object.value(QStringLiteral("xml")).toString().toUtf8();
    return workout;
}

} // namespace

QString normalizeRoomCode(const QString &userInput) {
    QString code = userInput.trimmed().toUpper();
    if (code.startsWith(QStringLiteral("GRP-"))) code.remove(0, 4);
    code.replace(QLatin1Char('O'), QLatin1Char('0'));
    code.replace(QLatin1Char('I'), QLatin1Char('1'));
    code.replace(QLatin1Char('L'), QLatin1Char('1'));
    if (code.size() != kCodeLength) return QString();
    for (const QChar character : code)
        if (!kCodeAlphabet.contains(character)) return QString();
    return code;
}

QUrl createRoomUrl(const QUrl &relayBaseUrl, const QString &displayName) {
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("v"), QString::number(kProtocolVersion));
    query.addQueryItem(QStringLiteral("create"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("name"), displayName);
    return wsUrl(relayBaseUrl, query);
}

QUrl joinRoomUrl(const QUrl &relayBaseUrl, const QString &roomCode,
                 const QString &displayName, const QString &rejoinToken) {
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("v"), QString::number(kProtocolVersion));
    query.addQueryItem(QStringLiteral("code"), normalizeRoomCode(roomCode));
    query.addQueryItem(QStringLiteral("name"), displayName);
    if (!rejoinToken.isEmpty()) query.addQueryItem(QStringLiteral("token"), rejoinToken);
    return wsUrl(relayBaseUrl, query);
}

QByteArray encodePing(qint64 localSendMs) {
    return toFrame({{QStringLiteral("type"), QStringLiteral("ping")},
                    {QStringLiteral("t0"), double(localSendMs)}});
}

QByteArray encodeReady(bool isReady, const SharedSensors &sensors) {
    const QJsonObject sensorsJson{{QStringLiteral("power"), sensors.power},
                                  {QStringLiteral("hr"), sensors.heartRate},
                                  {QStringLiteral("cad"), sensors.cadence}};
    return toFrame({{QStringLiteral("type"), QStringLiteral("ready")},
                    {QStringLiteral("ready"), isReady},
                    {QStringLiteral("sensors"), sensorsJson}});
}

QByteArray encodeWorkout(const SharedWorkout &workout) {
    return toFrame({{QStringLiteral("type"), QStringLiteral("workout")},
                    {QStringLiteral("name"), workout.name},
                    {QStringLiteral("xml"), QString::fromUtf8(workout.xml)}});
}

QByteArray encodeStart(qint64 startAtServerMs) {
    return toFrame({{QStringLiteral("type"), QStringLiteral("start")},
                    {QStringLiteral("at"), double(startAtServerMs)}});
}

QByteArray encodeTelemetry(const RiderTelemetry &telemetry) {
    QJsonObject object{{QStringLiteral("type"), QStringLiteral("tel")},
                       {QStringLiteral("t"), qRound(telemetry.elapsedWorkoutSeconds * 10.0) / 10.0},
                       {QStringLiteral("iv"), telemetry.intervalIndex},
                       {QStringLiteral("st"), statusToWire(telemetry.status)}};
    if (telemetry.powerWatts) object.insert(QStringLiteral("p"), *telemetry.powerWatts);
    if (telemetry.targetPowerWatts) object.insert(QStringLiteral("tgt"), *telemetry.targetPowerWatts);
    if (telemetry.heartRateBpm) object.insert(QStringLiteral("hr"), *telemetry.heartRateBpm);
    if (telemetry.cadenceRpm) object.insert(QStringLiteral("cad"), *telemetry.cadenceRpm);
    return toFrame(object);
}

QByteArray encodeLeave() {
    return toFrame({{QStringLiteral("type"), QStringLiteral("leave")}});
}

QString statusToWire(RiderStatus status) {
    switch (status) {
    case RiderStatus::Paused: return QStringLiteral("paused");
    case RiderStatus::Done:   return QStringLiteral("done");
    case RiderStatus::Riding: break;
    }
    return QStringLiteral("riding");
}

RiderStatus statusFromWire(const QString &wire) {
    if (wire == QLatin1String("paused")) return RiderStatus::Paused;
    if (wire == QLatin1String("done")) return RiderStatus::Done;
    return RiderStatus::Riding;
}

IncomingMessage decode(const QByteArray &frame) {
    IncomingMessage message;
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(frame, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return message;
    const QJsonObject object = document.object();
    const QString type = object.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("welcome")) {
        message.type = MessageType::Welcome;
        Welcome &welcome = message.welcome;
        welcome.roomCode = object.value(QStringLiteral("code")).toString();
        welcome.yourRiderId = object.value(QStringLiteral("you")).toString();
        welcome.rejoinToken = object.value(QStringLiteral("token")).toString();
        welcome.isHost = object.value(QStringLiteral("host")).toBool();
        welcome.serverMs = qint64(object.value(QStringLiteral("serverMs")).toDouble());
        welcome.maxRiders = object.value(QStringLiteral("maxRiders")).toInt(kMaxRiders);
        if (object.value(QStringLiteral("startAt")).isDouble())
            welcome.startAtServerMs = qint64(object.value(QStringLiteral("startAt")).toDouble());
        if (object.value(QStringLiteral("workout")).isObject())
            welcome.workout = workoutFromJson(object.value(QStringLiteral("workout")).toObject());
    } else if (type == QLatin1String("roster")) {
        message.type = MessageType::Roster;
        message.hostRiderId = object.value(QStringLiteral("hostId")).toString();
        const QJsonArray riders = object.value(QStringLiteral("riders")).toArray();
        for (const QJsonValue &value : riders) {
            const QJsonObject rider = value.toObject();
            RosterEntry entry;
            entry.riderId = rider.value(QStringLiteral("id")).toString();
            entry.name = rider.value(QStringLiteral("name")).toString();
            entry.isHost = rider.value(QStringLiteral("host")).toBool();
            entry.isReady = rider.value(QStringLiteral("ready")).toBool();
            entry.sensors = sensorsFromJson(rider.value(QStringLiteral("sensors")).toObject());
            message.roster.append(entry);
        }
    } else if (type == QLatin1String("workout")) {
        message.type = MessageType::Workout;
        message.workout = workoutFromJson(object);
    } else if (type == QLatin1String("start")) {
        message.type = MessageType::Start;
        message.startAtServerMs = qint64(object.value(QStringLiteral("at")).toDouble());
    } else if (type == QLatin1String("tel")) {
        message.type = MessageType::Telemetry;
        RiderTelemetry &telemetry = message.telemetry;
        telemetry.riderId = object.value(QStringLiteral("id")).toString();
        telemetry.elapsedWorkoutSeconds = object.value(QStringLiteral("t")).toDouble();
        telemetry.powerWatts = optionalInt(object, QStringLiteral("p"));
        telemetry.targetPowerWatts = optionalInt(object, QStringLiteral("tgt"));
        telemetry.heartRateBpm = optionalInt(object, QStringLiteral("hr"));
        telemetry.cadenceRpm = optionalInt(object, QStringLiteral("cad"));
        telemetry.intervalIndex = object.value(QStringLiteral("iv")).toInt();
        telemetry.status = statusFromWire(object.value(QStringLiteral("st")).toString());
    } else if (type == QLatin1String("pong")) {
        message.type = MessageType::Pong;
        message.pong.localSendMs = qint64(object.value(QStringLiteral("t0")).toDouble());
        message.pong.serverMs = qint64(object.value(QStringLiteral("serverMs")).toDouble());
    } else if (type == QLatin1String("error")) {
        message.type = MessageType::Error;
        message.errorCode = object.value(QStringLiteral("code")).toString();
        message.errorDetail = object.value(QStringLiteral("detail")).toString();
    } else if (type == QLatin1String("keepalive")) {
        message.type = MessageType::Keepalive;
    }
    return message;
}

std::optional<qint64> estimateServerOffsetMs(const QList<ClockSample> &samples) {
    std::optional<qint64> bestOffset;
    qint64 bestRoundTrip = std::numeric_limits<qint64>::max();
    for (const ClockSample &sample : samples) {
        const qint64 roundTrip = sample.localReceiveMs - sample.localSendMs;
        if (roundTrip < 0 || roundTrip >= bestRoundTrip) continue;
        bestRoundTrip = roundTrip;
        const qint64 localMidpoint = sample.localSendMs + roundTrip / 2;
        bestOffset = sample.serverMs - localMidpoint;
    }
    return bestOffset;
}

} // namespace GroupProtocol
