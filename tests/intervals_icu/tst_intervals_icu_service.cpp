/*
 * tst_intervals_icu_service.cpp
 *
 * Qt Test suite for IntervalsIcuApi.
 *
 * Tests run without making real network connections by injecting a
 * MockNetworkAccessManager that captures every QNetworkRequest and returns
 * an immediately-finished stub QNetworkReply that never touches the network.
 * The manager is registered on the QCoreApplication as the "NetworkManagerWS"
 * property, matching the production pattern used by ExtRequest and
 * IntervalsIcuDAO.
 *
 * Test groups
 * ──────────────────────────────────────────────────────────────────
 * Auth header   – Authorization: Basic is present and correctly encoded
 *                 for each public method
 * Accept header – Accept: application/json is set on every request
 * URLs          – each method targets the expected endpoint path
 * getEvents     – query string contains oldest= and newest= parameters
 * Null manager  – methods return nullptr when NetworkManagerWS is absent
 */

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDate>
#include <QTimer>

#include "intervals_icu_api.h"

// ─────────────────────────────────────────────────────────────────────────────
// FinishedReply — a stub QNetworkReply that is immediately finished and never
// opens a socket or performs any I/O.  It is used by MockNetworkAccessManager
// to satisfy callers without touching the network.
// ─────────────────────────────────────────────────────────────────────────────
class FinishedReply : public QNetworkReply
{
    Q_OBJECT
public:
    explicit FinishedReply(const QNetworkRequest &req, QObject *parent = nullptr)
        : QNetworkReply(parent)
    {
        setRequest(req);
        setUrl(req.url());
        setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        setFinished(true);
        // Schedule finished() in the next event loop tick so the reply is
        // fully constructed before any slot runs.
        QTimer::singleShot(0, this, &FinishedReply::finished);
    }

    qint64 bytesAvailable() const override { return 0; }
    bool   isSequential()   const override { return true; }
    void   abort()                override {}

protected:
    qint64 readData(char *, qint64) override { return -1; }
};

// ─────────────────────────────────────────────────────────────────────────────
// MockNetworkAccessManager
//
// Overrides createRequest() to record the last request and return a
// FinishedReply stub.  No base-class delegation — no real I/O is possible.
// ─────────────────────────────────────────────────────────────────────────────
class MockNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit MockNetworkAccessManager(QObject *parent = nullptr)
        : QNetworkAccessManager(parent) {}

    QNetworkRequest lastRequest;
    int             callCount = 0;

    void reset()
    {
        callCount   = 0;
        lastRequest = QNetworkRequest();
    }

protected:
    QNetworkReply* createRequest(Operation op,
                                 const QNetworkRequest &request,
                                 QIODevice *outgoingData) override
    {
        Q_UNUSED(op)
        Q_UNUSED(outgoingData)
        lastRequest = request;
        ++callCount;
        return new FinishedReply(request, this);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
static QByteArray expectedBasicHeader(const QString &apiKey)
{
    const QString credentials = QStringLiteral("API_KEY:") + apiKey;
    return QByteArray("Basic ") + credentials.toUtf8().toBase64();
}

// ─────────────────────────────────────────────────────────────────────────────
class TstIntervalsIcuService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    // ── Authorization header ────────────────────────────────────────────────
    void testGetAthlete_authHeader();
    void testGetEvents_authHeader();
    void testGetWorkouts_authHeader();
    void testGetWorkout_authHeader();
    void testConvertWorkoutToZwo_authHeader();
    void testDownloadZwo_authHeader();
    void testDownloadMrc_authHeader();
    void testDownloadEvent_authHeader();

    // ── Accept header ───────────────────────────────────────────────────────
    void testGetAthlete_acceptHeader();

    // ── URL construction ────────────────────────────────────────────────────
    void testGetAthlete_url();
    void testGetEvents_url();
    void testGetWorkouts_url();
    void testGetWorkout_url();
    void testConvertWorkoutToZwo_url();
    void testDownloadZwo_url();
    void testDownloadMrc_url();
    void testDownloadEvent_url();

    // ── getEvents query parameters ──────────────────────────────────────────
    void testGetEvents_queryParams();

    // ── Null manager guard ──────────────────────────────────────────────────
    void testNullManager_returnsNullptr();

private:
    MockNetworkAccessManager *m_manager = nullptr;

    static constexpr const char ATHLETE_ID[] = "i12345";
    static constexpr const char API_KEY[]    = "testApiKey";
    static constexpr const char WORKOUT_ID[] = "w99";
    static constexpr const char EVENT_ID[]   = "55512345";
};

// ─────────────────────────────────────────────────────────────────────────────
void TstIntervalsIcuService::initTestCase()
{
    m_manager = new MockNetworkAccessManager(this);
    qApp->setProperty("NetworkManagerWS",
                      QVariant::fromValue<QNetworkAccessManager*>(m_manager));
}

void TstIntervalsIcuService::cleanupTestCase()
{
    qApp->setProperty("NetworkManagerWS", QVariant());
}

void TstIntervalsIcuService::init()
{
    m_manager->reset();
}

// ─────────────────────────────────────────────────────────────────────────────
// Authorization header tests
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuService::testGetAthlete_authHeader()
{
    QNetworkReply *reply = IntervalsIcuApi::getAthlete(ATHLETE_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QByteArray header = m_manager->lastRequest.rawHeader("Authorization");
    QCOMPARE(header, expectedBasicHeader(API_KEY));
}

void TstIntervalsIcuService::testGetEvents_authHeader()
{
    QNetworkReply *reply = IntervalsIcuApi::getEvents(
        ATHLETE_ID, API_KEY,
        QDate(2024, 1, 1), QDate(2024, 1, 31));
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QByteArray header = m_manager->lastRequest.rawHeader("Authorization");
    QCOMPARE(header, expectedBasicHeader(API_KEY));
}

void TstIntervalsIcuService::testGetWorkouts_authHeader()
{
    QNetworkReply *reply = IntervalsIcuApi::getWorkouts(ATHLETE_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QByteArray header = m_manager->lastRequest.rawHeader("Authorization");
    QCOMPARE(header, expectedBasicHeader(API_KEY));
}

void TstIntervalsIcuService::testGetWorkout_authHeader()
{
    QNetworkReply *reply = IntervalsIcuApi::getWorkout(ATHLETE_ID, WORKOUT_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QByteArray header = m_manager->lastRequest.rawHeader("Authorization");
    QCOMPARE(header, expectedBasicHeader(API_KEY));
}

void TstIntervalsIcuService::testConvertWorkoutToZwo_authHeader()
{
    QNetworkReply *reply = IntervalsIcuApi::convertWorkoutToZwo(
        ATHLETE_ID, API_KEY, QByteArrayLiteral("{}"));
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QByteArray header = m_manager->lastRequest.rawHeader("Authorization");
    QCOMPARE(header, expectedBasicHeader(API_KEY));
}

void TstIntervalsIcuService::testDownloadZwo_authHeader()
{
    QNetworkReply *reply = IntervalsIcuApi::downloadWorkoutZwo(
        ATHLETE_ID, WORKOUT_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QByteArray header = m_manager->lastRequest.rawHeader("Authorization");
    QCOMPARE(header, expectedBasicHeader(API_KEY));
}

void TstIntervalsIcuService::testDownloadMrc_authHeader()
{
    QNetworkReply *reply = IntervalsIcuApi::downloadWorkoutMrc(
        ATHLETE_ID, WORKOUT_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QByteArray header = m_manager->lastRequest.rawHeader("Authorization");
    QCOMPARE(header, expectedBasicHeader(API_KEY));
}

void TstIntervalsIcuService::testDownloadEvent_authHeader()
{
    QNetworkReply *reply = IntervalsIcuApi::downloadEventZwo(
        ATHLETE_ID, EVENT_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QByteArray header = m_manager->lastRequest.rawHeader("Authorization");
    QCOMPARE(header, expectedBasicHeader(API_KEY));
}

// ─────────────────────────────────────────────────────────────────────────────
// Accept header test
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuService::testGetAthlete_acceptHeader()
{
    QNetworkReply *reply = IntervalsIcuApi::getAthlete(ATHLETE_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QByteArray header = m_manager->lastRequest.rawHeader("Accept");
    QCOMPARE(header, QByteArray("application/json"));
}

// ─────────────────────────────────────────────────────────────────────────────
// URL construction tests
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuService::testGetAthlete_url()
{
    QNetworkReply *reply = IntervalsIcuApi::getAthlete(ATHLETE_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QString url = m_manager->lastRequest.url().toString();
    QVERIFY(url.contains(QStringLiteral("/athlete/") + ATHLETE_ID));
    QVERIFY(url.startsWith(QStringLiteral("https://intervals.icu/api/v1")));
}

void TstIntervalsIcuService::testGetEvents_url()
{
    QNetworkReply *reply = IntervalsIcuApi::getEvents(
        ATHLETE_ID, API_KEY,
        QDate(2024, 1, 1), QDate(2024, 1, 31));
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QString url = m_manager->lastRequest.url().toString();
    QVERIFY(url.contains(QStringLiteral("/athlete/") + ATHLETE_ID + QStringLiteral("/events")));
}

void TstIntervalsIcuService::testGetWorkouts_url()
{
    QNetworkReply *reply = IntervalsIcuApi::getWorkouts(ATHLETE_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QString url = m_manager->lastRequest.url().toString();
    QVERIFY(url.contains(QStringLiteral("/athlete/") + ATHLETE_ID + QStringLiteral("/workouts")));
    QVERIFY(!url.contains(QStringLiteral("/workouts/"))); // no workout ID segment
}

void TstIntervalsIcuService::testGetWorkout_url()
{
    QNetworkReply *reply = IntervalsIcuApi::getWorkout(ATHLETE_ID, WORKOUT_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QString url = m_manager->lastRequest.url().toString();
    QVERIFY(url.contains(QStringLiteral("/workouts/") + WORKOUT_ID));
    QVERIFY(!url.contains(QStringLiteral(".zwo")));
    QVERIFY(!url.contains(QStringLiteral(".mrc")));
}

void TstIntervalsIcuService::testConvertWorkoutToZwo_url()
{
    QNetworkReply *reply = IntervalsIcuApi::convertWorkoutToZwo(
        ATHLETE_ID, API_KEY, QByteArrayLiteral("{}"));
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QString url = m_manager->lastRequest.url().toString();
    QVERIFY(url.contains(QStringLiteral("/athlete/") + ATHLETE_ID
                         + QStringLiteral("/download-workout.zwo")));
}

void TstIntervalsIcuService::testDownloadZwo_url()
{
    QNetworkReply *reply = IntervalsIcuApi::downloadWorkoutZwo(
        ATHLETE_ID, WORKOUT_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QString url = m_manager->lastRequest.url().toString();
    QVERIFY(url.contains(QStringLiteral("/workouts/") + WORKOUT_ID + QStringLiteral(".zwo")));
}

void TstIntervalsIcuService::testDownloadMrc_url()
{
    QNetworkReply *reply = IntervalsIcuApi::downloadWorkoutMrc(
        ATHLETE_ID, WORKOUT_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QString url = m_manager->lastRequest.url().toString();
    QVERIFY(url.contains(QStringLiteral("/workouts/") + WORKOUT_ID + QStringLiteral(".mrc")));
}

void TstIntervalsIcuService::testDownloadEvent_url()
{
    QNetworkReply *reply = IntervalsIcuApi::downloadEventZwo(
        ATHLETE_ID, EVENT_ID, API_KEY);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QString url = m_manager->lastRequest.url().toString();
    QVERIFY(url.contains(QStringLiteral("/athlete/") + ATHLETE_ID
                         + QStringLiteral("/events/") + EVENT_ID
                         + QStringLiteral("/download.zwo")));
    // A calendar event download must NOT hit the library /workouts/ endpoint.
    QVERIFY(!url.contains(QStringLiteral("/workouts/")));
}

// ─────────────────────────────────────────────────────────────────────────────
// getEvents query parameter test
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuService::testGetEvents_queryParams()
{
    const QDate start(2024, 3, 1);
    const QDate end(2024, 3, 31);

    QNetworkReply *reply = IntervalsIcuApi::getEvents(ATHLETE_ID, API_KEY, start, end);
    QVERIFY(reply != nullptr);
    QCOMPARE(m_manager->callCount, 1);
    reply->deleteLater();

    const QUrl url = m_manager->lastRequest.url();
    QVERIFY(url.hasQuery());
    QVERIFY(url.toString().contains(QStringLiteral("oldest=2024-03-01")));
    QVERIFY(url.toString().contains(QStringLiteral("newest=2024-03-31")));
}

// ─────────────────────────────────────────────────────────────────────────────
// Null manager guard test
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuService::testNullManager_returnsNullptr()
{
    // Temporarily clear the property
    qApp->setProperty("NetworkManagerWS", QVariant());

    QCOMPARE(IntervalsIcuApi::getAthlete(ATHLETE_ID, API_KEY), nullptr);
    QCOMPARE(IntervalsIcuApi::getEvents(ATHLETE_ID, API_KEY,
                                            QDate(2024,1,1), QDate(2024,1,31)), nullptr);
    QCOMPARE(IntervalsIcuApi::getWorkouts(ATHLETE_ID, API_KEY), nullptr);
    QCOMPARE(IntervalsIcuApi::getWorkout(ATHLETE_ID, WORKOUT_ID, API_KEY), nullptr);
    QCOMPARE(IntervalsIcuApi::convertWorkoutToZwo(ATHLETE_ID, API_KEY, QByteArrayLiteral("{}")), nullptr);
    QCOMPARE(IntervalsIcuApi::downloadWorkoutZwo(ATHLETE_ID, WORKOUT_ID, API_KEY), nullptr);
    QCOMPARE(IntervalsIcuApi::downloadWorkoutMrc(ATHLETE_ID, WORKOUT_ID, API_KEY), nullptr);

    // Restore
    qApp->setProperty("NetworkManagerWS",
                      QVariant::fromValue<QNetworkAccessManager*>(m_manager));
}

// ─────────────────────────────────────────────────────────────────────────────
QTEST_MAIN(TstIntervalsIcuService)
#include "tst_intervals_icu_service.moc"
