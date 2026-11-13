#ifndef GROUPSIMTEST_H
#define GROUPSIMTEST_H

#include "groupsession.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QUrl>

/// Headless check of a group-workout room: N simulated riders in one process
/// host/join a room on the relay, start together, stream 1 Hz telemetry, and
/// one rider drops and reconnects mid-ride. Prints a report and exits non-zero
/// if delivery, start sync or the reconnect fail.
class GroupSimTest : public QObject
{
    Q_OBJECT

public:
    GroupSimTest(const QUrl &relayBaseUrl, int riderCount, int rideSeconds, QObject *parent = nullptr);
    void start();

signals:
    void finished(int exitCode);

private:
    struct SimRider {
        GroupSession *session = nullptr;
        QString name;
        int basePowerWatts = 200;
        qint64 scheduledLocalStartMs = 0;
        qint64 actualStartMs = 0;
        int telemetrySent = 0;
        int telemetryReceived = 0;
        QTimer *startTimer = nullptr;
        QTimer *telemetryTimer = nullptr;
        bool done = false;
    };

    void onHostJoined(const QString &roomCode);
    void maybeStartRide();
    void onRiderStart(int riderIndex);
    void sendTelemetry(int riderIndex);
    void dropAndRejoinOneRider();
    void finish();
    void fail(const QString &reason);
    static QByteArray sampleWorkoutXml();

    QUrl m_relayBaseUrl;
    int m_riderCount;
    int m_rideSeconds;
    QList<SimRider> m_riders;
    QString m_roomCode;
    bool m_rideStarted = false;
    bool m_finished = false;
    QHash<QString, qint64> m_sentAtMs;          // "riderId/seq" -> local ms
    QList<qint64> m_latenciesMs;
    QString m_droppedRiderId;
    bool m_droppedRiderRejoinedSameId = false;
    QElapsedTimer m_wallClock;
    QTimer m_deadline;
};

#endif // GROUPSIMTEST_H
