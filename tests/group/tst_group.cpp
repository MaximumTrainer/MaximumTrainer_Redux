#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QUrlQuery>

#include "groupprotocol.h"
#include "groupsession.h"

using namespace GroupProtocol;

class TstGroup : public QObject
{
    Q_OBJECT

private slots:
    void normalizeRoomCode_data();
    void normalizeRoomCode();
    void joinUrlCarriesVersionCodeNameAndToken();
    void createUrlKeepsRelayPathPrefix();
    void telemetryRoundTripsAndOmitsMissingSensors();
    void decodeWelcomeWithWorkoutAndStart();
    void decodeRoster();
    void decodeErrorAndGarbage();
    void clockOffsetUsesShortestRoundTrip();
    void clockOffsetIgnoresImpossibleSamples();
    void liveTwoRidersShareStartAndTelemetry();

private:
    static QJsonObject parse(const QByteArray &frame) { return QJsonDocument::fromJson(frame).object(); }
};

void TstGroup::normalizeRoomCode_data() {
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");
    QTest::newRow("display form") << "GRP-7KQ2" << "7KQ2";
    QTest::newRow("lowercase + spaces") << "  grp-7kq2 " << "7KQ2";
    QTest::newRow("bare code") << "7KQ2" << "7KQ2";
    QTest::newRow("O/I/L typed for 0/1") << "OIL2" << "0112";
    QTest::newRow("U is not in the alphabet") << "7KU2" << "";
    QTest::newRow("too short") << "7KQ" << "";
    QTest::newRow("too long") << "7KQ22" << "";
}

void TstGroup::normalizeRoomCode() {
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(GroupProtocol::normalizeRoomCode(input), expected);
}

void TstGroup::joinUrlCarriesVersionCodeNameAndToken() {
    const QUrl url = joinRoomUrl(QUrl("wss://relay.example/"), "grp-7kq2", "Julie D", "tok123");
    QCOMPARE(url.scheme(), QStringLiteral("wss"));
    QCOMPARE(url.path(), QStringLiteral("/ws"));
    const QUrlQuery query(url);
    QCOMPARE(query.queryItemValue("v"), QString::number(kProtocolVersion));
    QCOMPARE(query.queryItemValue("code"), QStringLiteral("7KQ2"));
    QCOMPARE(query.queryItemValue("name", QUrl::FullyDecoded), QStringLiteral("Julie D"));
    QCOMPARE(query.queryItemValue("token"), QStringLiteral("tok123"));
    QVERIFY(!QUrlQuery(joinRoomUrl(QUrl("ws://h"), "7KQ2", "A")).hasQueryItem("token"));
}

void TstGroup::createUrlKeepsRelayPathPrefix() {
    const QUrl url = createRoomUrl(QUrl("wss://example.com/relay"), "Host");
    QCOMPARE(url.path(), QStringLiteral("/relay/ws"));
    QCOMPARE(QUrlQuery(url).queryItemValue("create"), QStringLiteral("1"));
}

void TstGroup::telemetryRoundTripsAndOmitsMissingSensors() {
    RiderTelemetry sent;
    sent.elapsedWorkoutSeconds = 1934.24;
    sent.powerWatts = 281;
    sent.targetPowerWatts = 290;
    sent.intervalIndex = 6;
    sent.status = RiderStatus::Paused;

    const QByteArray frame = encodeTelemetry(sent);
    const QJsonObject object = parse(frame);
    QCOMPARE(object.value("type").toString(), QStringLiteral("tel"));
    QVERIFY(!object.contains("hr"));
    QVERIFY(!object.contains("cad"));
    QVERIFY(frame.size() < 120);

    QJsonObject relayed = object;
    relayed.insert("id", "r3");
    const IncomingMessage message = decode(QJsonDocument(relayed).toJson(QJsonDocument::Compact));
    QCOMPARE(message.type, MessageType::Telemetry);
    QCOMPARE(message.telemetry.riderId, QStringLiteral("r3"));
    QCOMPARE(message.telemetry.elapsedWorkoutSeconds, 1934.2);
    QCOMPARE(message.telemetry.powerWatts, std::optional<int>(281));
    QCOMPARE(message.telemetry.targetPowerWatts, std::optional<int>(290));
    QVERIFY(!message.telemetry.heartRateBpm.has_value());
    QCOMPARE(message.telemetry.intervalIndex, 6);
    QCOMPARE(message.telemetry.status, RiderStatus::Paused);
}

void TstGroup::decodeWelcomeWithWorkoutAndStart() {
    const QByteArray frame = R"({"type":"welcome","protocol":1,"code":"GRP-7KQ2","you":"r2","token":"abc",
        "host":false,"serverMs":1794580368903,"maxRiders":20,"startAt":1794580400000,
        "workout":{"name":"Over-Unders","xml":"<Workout/>"}})";
    const IncomingMessage message = decode(frame);
    QCOMPARE(message.type, MessageType::Welcome);
    QCOMPARE(message.welcome.roomCode, QStringLiteral("GRP-7KQ2"));
    QCOMPARE(message.welcome.yourRiderId, QStringLiteral("r2"));
    QCOMPARE(message.welcome.rejoinToken, QStringLiteral("abc"));
    QVERIFY(!message.welcome.isHost);
    QCOMPARE(message.welcome.serverMs, qint64(1794580368903));
    QCOMPARE(message.welcome.startAtServerMs, std::optional<qint64>(1794580400000));
    QVERIFY(message.welcome.workout.has_value());
    QCOMPARE(message.welcome.workout->name, QStringLiteral("Over-Unders"));
    QCOMPARE(message.welcome.workout->xml, QByteArray("<Workout/>"));

    const IncomingMessage fresh = decode(R"({"type":"welcome","startAt":null,"workout":null})");
    QVERIFY(!fresh.welcome.startAtServerMs.has_value());
    QVERIFY(!fresh.welcome.workout.has_value());
}

void TstGroup::decodeRoster() {
    const IncomingMessage message = decode(R"({"type":"roster","hostId":"r1","riders":[
        {"id":"r1","name":"Host","host":true,"ready":true,"sensors":{"power":true,"hr":true,"cad":false}},
        {"id":"r2","name":"Julie","host":false,"ready":false,"sensors":{}}]})");
    QCOMPARE(message.type, MessageType::Roster);
    QCOMPARE(message.hostRiderId, QStringLiteral("r1"));
    QCOMPARE(message.roster.size(), 2);
    QVERIFY(message.roster[0].isHost);
    QVERIFY(message.roster[0].isReady);
    QVERIFY(message.roster[0].sensors.heartRate);
    QVERIFY(!message.roster[0].sensors.cadence);
    QCOMPARE(message.roster[1].name, QStringLiteral("Julie"));
    QVERIFY(!message.roster[1].isReady);
}

void TstGroup::decodeErrorAndGarbage() {
    const IncomingMessage error = decode(R"({"type":"error","code":"room_full","max":20})");
    QCOMPARE(error.type, MessageType::Error);
    QCOMPARE(error.errorCode, QStringLiteral("room_full"));
    QCOMPARE(decode("not json").type, MessageType::Unknown);
    QCOMPARE(decode("[1,2]").type, MessageType::Unknown);
    QCOMPARE(decode(R"({"type":"from_the_future"})").type, MessageType::Unknown);
}

void TstGroup::clockOffsetUsesShortestRoundTrip() {
    // Server is 5000 ms ahead. The slow sample's reply sat in a queue, which
    // would skew its midpoint; the fastest one should win.
    const QList<ClockSample> samples{
        {1000, 1400, 6050},   // 400 ms round trip
        {2000, 2020, 7010},   // 20 ms round trip -> offset exactly 5000
        {3000, 3100, 8090},
    };
    QCOMPARE(estimateServerOffsetMs(samples), std::optional<qint64>(5000));
    QVERIFY(!estimateServerOffsetMs({}).has_value());
}

void TstGroup::clockOffsetIgnoresImpossibleSamples() {
    const QList<ClockSample> samples{{2000, 1990, 0}, {1000, 1010, 1505}};
    QCOMPARE(estimateServerOffsetMs(samples), std::optional<qint64>(500));
}

void TstGroup::liveTwoRidersShareStartAndTelemetry() {
    const QByteArray relay = qgetenv("GROUP_RELAY_URL");
    if (relay.isEmpty()) QSKIP("GROUP_RELAY_URL not set (run `npx wrangler dev` in workers/group-relay)");

    GroupSession host(QUrl(QString::fromUtf8(relay)));
    GroupSession rider(QUrl(QString::fromUtf8(relay)));
    QSignalSpy hostJoined(&host, &GroupSession::joined);
    host.hostRoom("Host");
    QVERIFY(hostJoined.wait(5000));
    QVERIFY(host.isHost());

    QSignalSpy riderWorkout(&rider, &GroupSession::workoutReceived);
    host.shareWorkout(SharedWorkout{"Test", "<Workout/>"});
    rider.joinRoom(host.roomCode(), "Rider");
    QVERIFY(riderWorkout.wait(5000));
    QCOMPARE(riderWorkout.first().first().value<SharedWorkout>().name, QStringLiteral("Test"));

    QSignalSpy hostStart(&host, &GroupSession::startScheduled);
    QSignalSpy riderStart(&rider, &GroupSession::startScheduled);
    host.startIn(3000);
    QVERIFY(riderStart.wait(5000));
    if (hostStart.isEmpty()) QVERIFY(hostStart.wait(5000));
    const qint64 hostLocalStart = hostStart.first().first().toLongLong();
    const qint64 riderLocalStart = riderStart.first().first().toLongLong();
    QVERIFY2(qAbs(hostLocalStart - riderLocalStart) < 100, "same machine, so both starts must agree");

    QSignalSpy riderTelemetry(&rider, &GroupSession::telemetryReceived);
    RiderTelemetry telemetry;
    telemetry.powerWatts = 250;
    host.sendTelemetry(telemetry);
    QVERIFY(riderTelemetry.wait(5000));
    QCOMPARE(riderTelemetry.first().first().value<RiderTelemetry>().powerWatts, std::optional<int>(250));
    QCOMPARE(riderTelemetry.first().first().value<RiderTelemetry>().riderId, host.riderId());

    GroupSession stranger(QUrl(QString::fromUtf8(relay)));
    QSignalSpy strangerError(&stranger, &GroupSession::relayError);
    stranger.joinRoom("ZZZZ", "Nobody");
    QVERIFY(strangerError.wait(5000));
    QCOMPARE(strangerError.first().first().toString(), QStringLiteral("room_not_found"));

    rider.leave();
    host.leave();
}

QTEST_MAIN(TstGroup)
#include "tst_group.moc"
