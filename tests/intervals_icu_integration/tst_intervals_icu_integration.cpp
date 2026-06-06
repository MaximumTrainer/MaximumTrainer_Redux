/*
 * tst_intervals_icu_integration.cpp
 *
 * Live integration tests for IntervalsIcuApi.
 *
 * These tests make REAL HTTP requests to https://intervals.icu/api/v1 using
 * a dedicated test account.  Credentials are read from environment variables:
 *
 *   INTERVALS_ICU_API_KEY    – personal API key (Settings → API on intervals.icu)
 *   INTERVALS_ICU_ATHLETE_ID – athlete ID, e.g. i12345
 *
 * Every test calls QSKIP when credentials are absent, so the suite degrades
 * gracefully in local development and fork PRs that cannot access GitHub Secrets.
 *
 * A real QNetworkAccessManager is registered as the "NetworkManagerWS" application
 * property — the same pattern used by IntervalsIcuApi in production.
 *
 * Test groups
 * ──────────────────────────────────────────────────────────────────────────────
 * testGetAthlete    – GET /athlete/{id} → 200 OK, JSON "id" matches athleteId
 * testGetEvents     – GET /athlete/{id}/events?... → 200 OK, JSON array
 * testGetWorkouts   – GET /athlete/{id}/workouts → 200 OK, JSON array
 * testBadApiKey     – GET /athlete/{id} with wrong key → HTTP 4xx (401 or 403)
 */

#include "tst_intervals_icu_integration.h"

#include <QCoreApplication>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDate>
#include <QEventLoop>
#include <QTimer>

#include "intervals_icu_api.h"

static constexpr int TIMEOUT_MS = 30'000;  // 30 s per request

// ─────────────────────────────────────────────────────────────────────────────
// Helper: spin the event loop until the reply finishes or the timeout fires.
// ─────────────────────────────────────────────────────────────────────────────
static bool waitForReply(QNetworkReply *reply)
{
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(TIMEOUT_MS, &loop, &QEventLoop::quit);
    loop.exec();
    return reply->isFinished();
}

// ─────────────────────────────────────────────────────────────────────────────
// Test slot implementations
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuIntegration::initTestCase()
{
    m_apiKey    = qEnvironmentVariable("INTERVALS_ICU_API_KEY");
    m_athleteId = qEnvironmentVariable("INTERVALS_ICU_ATHLETE_ID");

    if (m_apiKey.isEmpty() || m_athleteId.isEmpty()) {
        QSKIP("Set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID to run "
              "live Intervals.icu integration tests.");
    }

    m_manager = new QNetworkAccessManager(this);
    qApp->setProperty("NetworkManagerWS",
                      QVariant::fromValue<QNetworkAccessManager*>(m_manager));
}

void TstIntervalsIcuIntegration::cleanupTestCase()
{
    qApp->setProperty("NetworkManagerWS", QVariant());
}

void TstIntervalsIcuIntegration::testGetAthlete()
{
    QNetworkReply *reply = IntervalsIcuApi::getAthlete(m_athleteId, m_apiKey);
    QVERIFY2(reply, "getAthlete returned nullptr — NetworkManagerWS not registered");

    QVERIFY2(waitForReply(reply), "getAthlete timed out after 30 s");
    QCOMPARE(reply->error(), QNetworkReply::NoError);

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QCOMPARE(status, 200);

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    QVERIFY2(!obj.isEmpty(), "getAthlete: response body is not a JSON object");
    QCOMPARE(obj.value(QStringLiteral("id")).toString(), m_athleteId);

    reply->deleteLater();
}

void TstIntervalsIcuIntegration::testGetEvents()
{
    const QDate today = QDate::currentDate();
    QNetworkReply *reply = IntervalsIcuApi::getEvents(
        m_athleteId, m_apiKey,
        today.addDays(-7),
        today.addDays(7));
    QVERIFY2(reply, "getEvents returned nullptr — NetworkManagerWS not registered");

    QVERIFY2(waitForReply(reply), "getEvents timed out after 30 s");
    QCOMPARE(reply->error(), QNetworkReply::NoError);

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QCOMPARE(status, 200);

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QVERIFY2(doc.isArray(), "getEvents: response body is not a JSON array");

    reply->deleteLater();
}

void TstIntervalsIcuIntegration::testGetWorkouts()
{
    QNetworkReply *reply = IntervalsIcuApi::getWorkouts(m_athleteId, m_apiKey);
    QVERIFY2(reply, "getWorkouts returned nullptr — NetworkManagerWS not registered");

    QVERIFY2(waitForReply(reply), "getWorkouts timed out after 30 s");
    QCOMPARE(reply->error(), QNetworkReply::NoError);

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QCOMPARE(status, 200);

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QVERIFY2(doc.isArray(), "getWorkouts: response body is not a JSON array");

    reply->deleteLater();
}

void TstIntervalsIcuIntegration::testBadApiKey()
{
    QNetworkReply *reply = IntervalsIcuApi::getAthlete(
        m_athleteId, QStringLiteral("invalid-key-xyz"));
    QVERIFY2(reply, "getAthlete(bad key) returned nullptr — NetworkManagerWS not registered");

    QVERIFY2(waitForReply(reply), "getAthlete(bad key) timed out after 30 s");

    // intervals.icu returns 401 or 403 depending on endpoint/version for bad credentials.
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QVERIFY2(status == 401 || status == 403,
             qPrintable(QStringLiteral("Expected 401 or 403, got %1").arg(status)));

    reply->deleteLater();
}

QTEST_MAIN(TstIntervalsIcuIntegration)
