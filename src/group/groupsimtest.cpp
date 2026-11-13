#include "groupsimtest.h"

#include "grouppanel.h"
#include "groupridersmodel.h"

#include <QApplication>

#include "apptheme.h"

#include <QDateTime>
#include <QTextStream>
#include <QtMath>

#include <algorithm>
#include <limits>

using namespace GroupProtocol;

namespace {

constexpr int kStartDelayMs = 5000;
constexpr int kSetupBudgetMs = 30000;

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

qint64 percentile(QList<qint64> values, double fraction) {
    if (values.isEmpty()) return -1;
    std::sort(values.begin(), values.end());
    const int index = qBound(0, int(fraction * (values.size() - 1) + 0.5), int(values.size() - 1));
    return values.at(index);
}

} // namespace

GroupSimTest::GroupSimTest(const QUrl &relayBaseUrl, int riderCount, int rideSeconds,
                           const QString &joinRoomCode, QObject *parent)
    : QObject(parent),
      m_relayBaseUrl(relayBaseUrl),
      m_riderCount(qBound(joinRoomCode.isEmpty() ? 2 : 1, riderCount,
                          joinRoomCode.isEmpty() ? kMaxRiders : kMaxRiders - 1)),
      m_rideSeconds(qMax(5, rideSeconds)),
      m_joinRoomCode(joinRoomCode)
{
    m_deadline.setSingleShot(true);
    connect(&m_deadline, &QTimer::timeout, this, [this] { fail(QStringLiteral("timed out")); });
}

void GroupSimTest::start() {
    m_wallClock.start();
    out() << "group-sim: " << m_riderCount << " riders, " << m_rideSeconds << " s ride, relay "
          << m_relayBaseUrl.toString() << Qt::endl;
    // Joining someone else's room: wait as long as they take to press Start.
    if (m_joinRoomCode.isEmpty())
        m_deadline.start(kSetupBudgetMs + kStartDelayMs + m_rideSeconds * 1000 + 15000);

    for (int index = 0; index < m_riderCount; ++index) {
        SimRider rider;
        rider.name = m_joinRoomCode.isEmpty() && index == 0 ? QStringLiteral("Host")
                                                             : QStringLiteral("Sim %1").arg(index + (m_joinRoomCode.isEmpty() ? 0 : 1));
        rider.basePowerWatts = 150 + index * 7;
        rider.session = new GroupSession(m_relayBaseUrl, this);
        rider.startTimer = new QTimer(this);
        rider.startTimer->setSingleShot(true);
        rider.startTimer->setTimerType(Qt::PreciseTimer);
        rider.telemetryTimer = new QTimer(this);
        rider.telemetryTimer->setInterval(kTelemetryIntervalMs);
        m_riders.append(rider);

        GroupSession *session = rider.session;
        connect(rider.startTimer, &QTimer::timeout, this, [this, index] { onRiderStart(index); });
        connect(rider.telemetryTimer, &QTimer::timeout, this, [this, index] { sendTelemetry(index); });

        connect(session, &GroupSession::relayError, this, [this, index](const QString &code) {
            fail(QStringLiteral("%1 got relay error '%2'").arg(m_riders[index].name, code));
        });
        connect(session, &GroupSession::joined, this, [this, index, session](const QString &code) {
            if (index == 0 && m_joinRoomCode.isEmpty()) onHostJoined(code);
            if (index == 0 && !m_joinRoomCode.isEmpty()) {
                m_roomCode = code;
                out() << m_riderCount << " simulated riders joining " << code
                      << "; waiting for the host to start" << Qt::endl;
            }
            session->setReady(true, SharedSensors{true, true, true});
        });
        connect(session, &GroupSession::rosterChanged, this, [this, index] {
            if (index == 0 && m_joinRoomCode.isEmpty()) maybeStartRide();
        });
        connect(session, &GroupSession::startScheduled, this, [this, index](qint64 localStartMs) {
            SimRider &simRider = m_riders[index];
            if (simRider.actualStartMs) return;
            simRider.scheduledLocalStartMs = localStartMs;
            simRider.startTimer->start(int(qMax<qint64>(0, localStartMs - nowMs())));
        });
        connect(session, &GroupSession::telemetryReceived, this,
                [this, index](const RiderTelemetry &telemetry) {
            ++m_riders[index].telemetryReceived;
            const QString key = telemetry.riderId + QLatin1Char('/')
                              + QString::number(qRound(telemetry.elapsedWorkoutSeconds));
            const auto sentAt = m_sentAtMs.constFind(key);
            if (sentAt != m_sentAtMs.constEnd()) m_latenciesMs.append(nowMs() - *sentAt);
        });
    }

    if (m_joinRoomCode.isEmpty()) {
        m_riders[0].session->hostRoom(m_riders[0].name);
        return;
    }
    for (SimRider &rider : m_riders) rider.session->joinRoom(m_joinRoomCode, rider.name);
}

void GroupSimTest::onHostJoined(const QString &roomCode) {
    m_roomCode = roomCode;
    out() << "host created room " << roomCode << " after " << m_wallClock.elapsed() << " ms" << Qt::endl;
    m_riders[0].session->shareWorkout(SharedWorkout{QStringLiteral("Group sim"), sampleWorkoutXml()});
    for (int index = 1; index < m_riders.size(); ++index)
        m_riders[index].session->joinRoom(roomCode, m_riders[index].name);
}

void GroupSimTest::maybeStartRide() {
    GroupSession *host = m_riders[0].session;
    if (m_rideStarted || host->roster().size() < m_riderCount) return;
    for (const RosterEntry &entry : host->roster())
        if (!entry.isReady) return;
    if (!host->hasClockOffset()) return;
    m_rideStarted = true;
    out() << "all " << m_riderCount << " riders joined and ready after " << m_wallClock.elapsed()
          << " ms; starting in " << kStartDelayMs / 1000 << " s" << Qt::endl;
    host->startIn(kStartDelayMs);
}

void GroupSimTest::onRiderStart(int riderIndex) {
    SimRider &rider = m_riders[riderIndex];
    rider.actualStartMs = nowMs();
    rider.telemetryTimer->start();
    if (riderIndex == m_riderCount - 1 && m_joinRoomCode.isEmpty())
        QTimer::singleShot(m_rideSeconds * 1000 / 2, this, &GroupSimTest::dropAndRejoinOneRider);
    if (riderIndex == 0 && !m_joinRoomCode.isEmpty())
        out() << "group started, riding for " << m_rideSeconds << " s" << Qt::endl;
    if (riderIndex == 0)
        QTimer::singleShot(m_rideSeconds * 1000 + 2500, this, &GroupSimTest::finish);
}

void GroupSimTest::sendTelemetry(int riderIndex) {
    SimRider &rider = m_riders[riderIndex];
    if (rider.done) return;
    const double elapsedSeconds = (nowMs() - rider.actualStartMs) / 1000.0;
    if (elapsedSeconds > m_rideSeconds) {
        rider.done = true;
        rider.telemetryTimer->stop();
        return;
    }
    RiderTelemetry telemetry;
    telemetry.elapsedWorkoutSeconds = qRound(elapsedSeconds);
    telemetry.targetPowerWatts = rider.basePowerWatts;
    telemetry.powerWatts = rider.basePowerWatts + int(12 * qSin(elapsedSeconds / 3.0 + riderIndex));
    telemetry.heartRateBpm = 120 + riderIndex + int(elapsedSeconds / 4);
    telemetry.cadenceRpm = 85 + riderIndex % 10;
    telemetry.intervalIndex = int(elapsedSeconds) / 60;
    telemetry.percentFtp = qRound(100.0 * *telemetry.powerWatts / 250.0);
    if (riderIndex % 5 == 2) telemetry.powerWatts = int(rider.basePowerWatts * 0.85);
    if (riderIndex % 7 == 3 && int(elapsedSeconds) % 40 >= 30) telemetry.status = RiderStatus::Paused;

    if (rider.session->state() != GroupSession::State::Connected) return;
    m_sentAtMs.insert(rider.session->riderId() + QLatin1Char('/')
                      + QString::number(qRound(telemetry.elapsedWorkoutSeconds)), nowMs());
    rider.session->sendTelemetry(telemetry);
    ++rider.telemetrySent;
}

void GroupSimTest::dropAndRejoinOneRider() {
    GroupSession *session = m_riders[m_riderCount - 1].session;
    const QString name = m_riders[m_riderCount - 1].name;
    m_droppedRiderId = session->riderId();
    out() << "dropping " << name << " (" << m_droppedRiderId << ") to test reconnect" << Qt::endl;
    connect(session, &GroupSession::stateChanged, this, [this, session, name](GroupSession::State state) {
        if (state != GroupSession::State::Connected) return;
        m_droppedRiderRejoinedSameId = session->riderId() == m_droppedRiderId;
        out() << name << " reconnected as " << session->riderId() << Qt::endl;
    });
    session->dropConnectionForTesting();
}

void GroupSimTest::finish() {
    if (m_finished) return;
    m_finished = true;
    m_deadline.stop();

    qint64 earliestStart = std::numeric_limits<qint64>::max();
    qint64 latestStart = 0;
    int totalSent = 0;
    int totalReceived = 0;
    for (const SimRider &rider : std::as_const(m_riders)) {
        earliestStart = qMin(earliestStart, rider.actualStartMs);
        latestStart = qMax(latestStart, rider.actualStartMs);
        totalSent += rider.telemetrySent;
        totalReceived += rider.telemetryReceived;
    }
    // Every frame a rider sends should reach the other N-1 riders while they
    // are connected; the dropped rider misses a few seconds by design.
    qint64 minOffset = std::numeric_limits<qint64>::max();
    qint64 maxOffset = std::numeric_limits<qint64>::min();
    qint64 minScheduled = std::numeric_limits<qint64>::max();
    qint64 maxScheduled = 0;
    for (const SimRider &rider : std::as_const(m_riders)) {
        minOffset = qMin(minOffset, rider.session->serverOffsetMs());
        maxOffset = qMax(maxOffset, rider.session->serverOffsetMs());
        minScheduled = qMin(minScheduled, rider.scheduledLocalStartMs);
        maxScheduled = qMax(maxScheduled, rider.scheduledLocalStartMs);
    }
    const int expected = totalSent * (m_riderCount - 1);
    const double deliveryRatio = expected ? double(totalReceived) / expected : 0.0;
    const qint64 startSpreadMs = latestStart - earliestStart;

    out() << "--- group-sim report ---" << Qt::endl
          << "room:              " << m_roomCode << Qt::endl
          << "start spread:      " << startSpreadMs << " ms (scheduled " << (maxScheduled - minScheduled)
          << " ms, clock offsets spread " << (maxOffset - minOffset) << " ms)" << Qt::endl
          << "telemetry sent:    " << totalSent << Qt::endl
          << "telemetry recv:    " << totalReceived << " of " << expected
          << QStringLiteral(" (%1%)").arg(deliveryRatio * 100.0, 0, 'f', 1) << Qt::endl
          << "latency p50 / p95: " << percentile(m_latenciesMs, 0.5) << " / "
          << percentile(m_latenciesMs, 0.95) << " ms" << Qt::endl
          << "reconnect:         " << (!m_joinRoomCode.isEmpty() ? "not tested (joined a hosted room)"
                                       : m_droppedRiderRejoinedSameId ? "same rider id" : "FAILED") << Qt::endl
          << "relay round trip:  " << m_riders[0].session->lastRoundTripMs() << " ms" << Qt::endl;

    const bool reconnectOk = !m_joinRoomCode.isEmpty() || m_droppedRiderRejoinedSameId;
    const bool passed = startSpreadMs < 200 && deliveryRatio > 0.9 && reconnectOk;
    out() << (passed ? "PASS" : "FAIL") << Qt::endl;

    for (SimRider &rider : m_riders) rider.session->leave();
    QTimer::singleShot(500, this, [this, passed] { emit finished(passed ? 0 : 1); });
}

void GroupSimTest::fail(const QString &reason) {
    if (m_finished) return;
    m_finished = true;
    out() << "FAIL: " << reason << Qt::endl;
    for (SimRider &rider : m_riders) rider.session->leave();
    QTimer::singleShot(200, this, [this] { emit finished(1); });
}

QByteArray GroupSimTest::sampleWorkoutXml() {
    return QByteArrayLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?><Workout><Name>Group sim</Name></Workout>");
}


bool GroupSimTest::renderPanelPreview(const QString &pngPath, int riderCount) {
    static const char *const names[] = {"Maxime", "Julie", "Marc", "Sam", "Alex", "Chloé", "Nico",
        "Léa", "Tom", "Ines", "Hugo", "Emma", "Louis", "Zoé", "Paul", "Jade", "Theo", "Lina",
        "Victor", "Rose with a very long display name"};
    riderCount = qBound(1, riderCount, kMaxRiders);

    if (qEnvironmentVariableIsSet("MT_PREVIEW_DARK")) qApp->setPalette(AppTheme::darkPalette());
    GroupPanel panel(nullptr);
    panel.setRoomCodeForPreview(QStringLiteral("GRP-V2AT"));
    panel.setRideStarted();

    QList<RosterEntry> roster;
    for (int index = 0; index < riderCount; ++index) {
        RosterEntry entry;
        entry.riderId = QStringLiteral("r%1").arg(index + 1);
        entry.name = QString::fromUtf8(names[index]);
        entry.isHost = index == 0;
        entry.isReady = index != 5;              // one rider still pairing
        roster.append(entry);
    }
    GroupRidersModel *model = panel.ridersModel();
    model->setRoster(roster, QStringLiteral("r1"));

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (int index = 0; index < riderCount; ++index) {
        if (index == 5) continue;
        RiderTelemetry telemetry;
        telemetry.riderId = roster[index].riderId;
        telemetry.elapsedWorkoutSeconds = index == 3 ? 1210 : 1234;   // Sam is 0:24 behind
        const int target = 180 + index * 6;
        telemetry.targetPowerWatts = target;
        telemetry.powerWatts = index % 4 == 2 ? int(target * 0.88) : target + (index % 3) - 1;
        telemetry.percentFtp = 100 + (index % 7) - 3;
        telemetry.heartRateBpm = 140 + index;
        telemetry.cadenceRpm = 85 + index % 9;
        if (index == 3) telemetry.status = RiderStatus::Paused;
        if (index == 7) telemetry.status = RiderStatus::Done;
        const qint64 receivedAt = index == 9 ? now - 10000 : now;   // Ines lost connection
        if (index == 0) model->updateOwnTelemetry(telemetry, receivedAt);
        else model->updateTelemetry(telemetry, receivedAt);
    }

    panel.resize(panel.width(), 852);
    panel.show();
    QCoreApplication::processEvents();
    return panel.grab().save(pngPath);
}
