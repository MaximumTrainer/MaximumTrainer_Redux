#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QUrlQuery>

#include "groupprotocol.h"
#include "groupsession.h"
#include "groupridersmodel.h"

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
    void ridersModelPutsYouFirstThenJoinOrder();
    void ridersModelStates();
    void ridersModelDetailShowsBehindAndAhead();
    void ridersModelKeepsTelemetryAcrossRosterChanges();
    void liveTwoRidersShareStartAndTelemetry();
    void liveHostReadyAfterDelaySurvivesJoins();

private:
    static QList<RosterEntry> roster(std::initializer_list<const char *> ids) {
        QList<RosterEntry> entries;
        for (const char *id : ids) {
            RosterEntry entry;
            entry.riderId = QString::fromLatin1(id);
            entry.name = QStringLiteral("Name ") + entry.riderId;
            entry.isReady = true;
            entries.append(entry);
        }
        return entries;
    }
    static RiderTelemetry tel(const char *id, std::optional<int> power, std::optional<int> target, double elapsed = 100) {
        RiderTelemetry telemetry;
        telemetry.riderId = QString::fromLatin1(id);
        telemetry.powerWatts = power;
        telemetry.targetPowerWatts = target;
        telemetry.elapsedWorkoutSeconds = elapsed;
        return telemetry;
    }
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
    sent.percentFtp = 105;
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
    QCOMPARE(message.telemetry.percentFtp, std::optional<int>(105));
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

void TstGroup::ridersModelPutsYouFirstThenJoinOrder() {
    GroupRidersModel model;
    model.setRoster(roster({"r1", "r2", "r3"}), "r2");
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.rowAt(0).riderId, QStringLiteral("r2"));
    QVERIFY(model.rowAt(0).isYou);
    QCOMPARE(model.rowAt(1).riderId, QStringLiteral("r1"));
    QCOMPARE(model.rowAt(2).riderId, QStringLiteral("r3"));
}

void TstGroup::ridersModelStates() {
    using State = GroupRidersModel::RowState;
    GroupRidersModel model;
    QList<RosterEntry> entries = roster({"me", "a", "b", "c", "d", "e"});
    entries[5].isReady = false;
    model.setRoster(entries, "me");
    const qint64 now = 1'000'000;
    model.updateTelemetry(tel("a", 210, 200), now);            // +5%: on target
    model.updateTelemetry(tel("b", 170, 200), now);            // -15%: off target
    RiderTelemetry paused = tel("c", 0, 200);
    paused.status = RiderStatus::Paused;
    model.updateTelemetry(paused, now);
    model.updateTelemetry(tel("d", 200, 200), now - GroupRidersModel::kStaleAfterMs - 1);

    QCOMPARE(model.stateOf(model.rowAt(0), now), State::Waiting);
    QCOMPARE(model.stateOf(model.rowAt(1), now), State::OnTarget);
    QCOMPARE(model.stateOf(model.rowAt(2), now), State::OffTarget);
    QCOMPARE(model.stateOf(model.rowAt(3), now), State::Paused);
    QCOMPARE(model.stateOf(model.rowAt(4), now), State::Stale);
    QCOMPARE(model.stateOf(model.rowAt(5), now), State::Connecting);

    // Your own row never goes stale: it is fed locally, not by the relay.
    model.updateOwnTelemetry(tel("", 200, 0), now - 60'000);
    QCOMPARE(model.stateOf(model.rowAt(0), now), State::OnTarget);
}

void TstGroup::ridersModelDetailShowsBehindAndAhead() {
    GroupRidersModel model;
    model.setRoster(roster({"me", "late", "early", "close"}), "me");
    const qint64 now = 5'000;
    model.updateOwnTelemetry(tel("", 200, 200, 600), now);
    RiderTelemetry late = tel("late", 200, 200, 576);
    late.percentFtp = 105;
    late.cadenceRpm = 92;
    model.updateTelemetry(late, now);
    model.updateTelemetry(tel("early", 200, 200, 671), now);
    model.updateTelemetry(tel("close", 200, 200, 597), now);

    QCOMPARE(model.detailLine(model.rowAt(1), now), QStringLiteral("105% FTP · 92 rpm · 0:24 behind"));
    QCOMPARE(model.detailLine(model.rowAt(2), now), QStringLiteral("1:11 ahead"));
    QCOMPARE(model.detailLine(model.rowAt(3), now), QString());
}

void TstGroup::ridersModelKeepsTelemetryAcrossRosterChanges() {
    GroupRidersModel model;
    model.setRoster(roster({"me", "a"}), "me");
    model.updateTelemetry(tel("a", 250, 250), 1000);
    model.setRoster(roster({"me", "a", "b"}), "me");
    QCOMPARE(model.rowCount(), 3);
    QVERIFY(model.rowAt(1).telemetry.has_value());
    QCOMPARE(model.rowAt(1).telemetry->powerWatts, std::optional<int>(250));
    model.setRoster(roster({"me", "b"}), "me");
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.rowAt(1).riderId, QStringLiteral("b"));
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

void TstGroup::liveHostReadyAfterDelaySurvivesJoins() {
    const QByteArray relay = qgetenv("GROUP_RELAY_URL");
    if (relay.isEmpty()) QSKIP("GROUP_RELAY_URL not set");
    const QUrl relayUrl(QString::fromUtf8(relay));

    GroupSession host(relayUrl);
    QSignalSpy hostJoined(&host, &GroupSession::joined);
    host.hostRoom("Host");
    QVERIFY(hostJoined.wait(5000));
    host.shareWorkout(SharedWorkout{"Test", "<Workout/>"});
    QTest::qWait(3000);                       // rider picks a connection method
    host.setReady(true, SharedSensors{true, true, true});
    QTest::qWait(1000);

    QList<GroupSession *> riders;
    for (int index = 0; index < 5; ++index) {
        auto *rider = new GroupSession(relayUrl, this);
        rider->joinRoom(host.roomCode(), QStringLiteral("R%1").arg(index));
        riders.append(rider);
    }
    QTRY_COMPARE_WITH_TIMEOUT(host.roster().size(), 6, 10000);
    for (const RosterEntry &entry : host.roster())
        if (entry.riderId == host.riderId()) QVERIFY2(entry.isReady, "host lost its ready flag");
    for (GroupSession *rider : riders) { rider->leave(); rider->deleteLater(); }
    host.leave();
}

QTEST_MAIN(TstGroup)
#include "tst_group.moc"
