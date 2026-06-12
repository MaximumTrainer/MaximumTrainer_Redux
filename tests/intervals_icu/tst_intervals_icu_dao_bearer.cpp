/*
 * tst_intervals_icu_dao_bearer.cpp
 *
 * Qt Test suite for the OAuth2 Bearer-token methods added to IntervalsIcuDAO:
 *   - IntervalsIcuDAO::getAthleteBearer()
 *   - IntervalsIcuDAO::getAthleteSettingsBearer()
 *
 * Tests run without real network connections using the shared
 * MockNetworkAccessManager defined in this file (mirrors the mock in
 * tst_intervals_icu_service.cpp).
 *
 * Test groups
 * ──────────────────────────────────────────────────────────────────
 * Bearer header  – Authorization: Bearer <token> is present
 * Accept header  – Accept: application/json is set
 * URL structure  – endpoint paths are correct
 * Null manager   – returns nullptr when NetworkManagerWS is absent
 */

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include "intervalsicudao.h"

// ─────────────────────────────────────────────────────────────────────────────
// FinishedReply — immediately-finished stub (no I/O)
// ─────────────────────────────────────────────────────────────────────────────
class BearerFinishedReply : public QNetworkReply
{
    Q_OBJECT
public:
    explicit BearerFinishedReply(const QNetworkRequest &req, QObject *parent = nullptr)
        : QNetworkReply(parent)
    {
        setRequest(req);
        setUrl(req.url());
        setOperation(QNetworkAccessManager::GetOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        setFinished(true);
        QTimer::singleShot(0, this, &BearerFinishedReply::finished);
    }

    qint64 bytesAvailable() const override { return 0; }
    bool   isSequential()   const override { return true; }
    void   abort()                override {}

protected:
    qint64 readData(char *, qint64) override { return -1; }
};

// ─────────────────────────────────────────────────────────────────────────────
// MockNetworkAccessManager
// ─────────────────────────────────────────────────────────────────────────────
class BearerMockManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit BearerMockManager(QObject *parent = nullptr)
        : QNetworkAccessManager(parent) {}

    QNetworkRequest lastRequest;
    int             callCount = 0;

    void reset() { callCount = 0; lastRequest = QNetworkRequest(); }

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &req,
                                 QIODevice *) override
    {
        lastRequest = req;
        ++callCount;
        return new BearerFinishedReply(req, this);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Test class
// ─────────────────────────────────────────────────────────────────────────────
class TstIntervalsIcuDaoBearer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    // Bearer header tests
    void testGetAthleteBearer_hasBearerHeader();
    void testGetAthleteSettingsBearer_hasBearerHeader();

    // Accept header tests
    void testGetAthleteBearer_acceptHeader();
    void testGetAthleteSettingsBearer_acceptHeader();

    // URL structure tests
    void testGetAthleteBearer_url();
    void testGetAthleteSettingsBearer_url();
    void testGetAthleteBearer_currentUser_url();

    // Null manager guard
    void testNullManager_returnsNullptr();

private:
    BearerMockManager *m_manager = nullptr;

    static constexpr const char *ATHLETE_ID   = "i12345";
    static constexpr const char *BEARER_TOKEN = "test_access_token_abc123";
};

void TstIntervalsIcuDaoBearer::initTestCase()
{
    m_manager = new BearerMockManager(this);
    qApp->setProperty("NetworkManagerWS",
                      QVariant::fromValue<QNetworkAccessManager *>(m_manager));
}

void TstIntervalsIcuDaoBearer::cleanupTestCase()
{
    qApp->setProperty("NetworkManagerWS", QVariant());
}

void TstIntervalsIcuDaoBearer::init()
{
    m_manager->reset();
}

// ─────────────────────────────────────────────────────────────────────────────
// Bearer header tests
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuDaoBearer::testGetAthleteBearer_hasBearerHeader()
{
    QNetworkReply *reply = IntervalsIcuDAO::getAthleteBearer(ATHLETE_ID, BEARER_TOKEN);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    const QByteArray authHeader = m_manager->lastRequest.rawHeader("Authorization");
    const QByteArray expected   = QByteArray("Bearer ") + BEARER_TOKEN;
    QCOMPARE(authHeader, expected);
}

void TstIntervalsIcuDaoBearer::testGetAthleteSettingsBearer_hasBearerHeader()
{
    QNetworkReply *reply = IntervalsIcuDAO::getAthleteSettingsBearer(ATHLETE_ID, BEARER_TOKEN);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    const QByteArray authHeader = m_manager->lastRequest.rawHeader("Authorization");
    const QByteArray expected   = QByteArray("Bearer ") + BEARER_TOKEN;
    QCOMPARE(authHeader, expected);
}

// ─────────────────────────────────────────────────────────────────────────────
// Accept header tests
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuDaoBearer::testGetAthleteBearer_acceptHeader()
{
    QNetworkReply *reply = IntervalsIcuDAO::getAthleteBearer(ATHLETE_ID, BEARER_TOKEN);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    QCOMPARE(m_manager->lastRequest.rawHeader("Accept"), QByteArray("application/json"));
}

void TstIntervalsIcuDaoBearer::testGetAthleteSettingsBearer_acceptHeader()
{
    QNetworkReply *reply = IntervalsIcuDAO::getAthleteSettingsBearer(ATHLETE_ID, BEARER_TOKEN);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    QCOMPARE(m_manager->lastRequest.rawHeader("Accept"), QByteArray("application/json"));
}

// ─────────────────────────────────────────────────────────────────────────────
// URL structure tests
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuDaoBearer::testGetAthleteBearer_url()
{
    QNetworkReply *reply = IntervalsIcuDAO::getAthleteBearer(ATHLETE_ID, BEARER_TOKEN);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    const QString url = m_manager->lastRequest.url().toString();
    QVERIFY(url.startsWith(QStringLiteral("https://intervals.icu/api/v1/athlete/")));
    QVERIFY(url.endsWith(QString(ATHLETE_ID)));
}

void TstIntervalsIcuDaoBearer::testGetAthleteSettingsBearer_url()
{
    QNetworkReply *reply = IntervalsIcuDAO::getAthleteSettingsBearer(ATHLETE_ID, BEARER_TOKEN);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    const QString url = m_manager->lastRequest.url().toString();
    QVERIFY(url.contains(QStringLiteral("/athlete/") + ATHLETE_ID));
    // Exact suffix: the former /settings endpoint never existed (404) —
    // per-sport FTP/LTHR/zones live at /sport-settings.
    QVERIFY(url.endsWith(QStringLiteral("/sport-settings")));
}

void TstIntervalsIcuDaoBearer::testGetAthleteBearer_currentUser_url()
{
    // Athlete ID "0" is the OAuth2 "current user" sentinel.
    QNetworkReply *reply = IntervalsIcuDAO::getAthleteBearer("0", BEARER_TOKEN);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    const QString url = m_manager->lastRequest.url().toString();
    QVERIFY(url.endsWith(QStringLiteral("/athlete/0")));
}

// ─────────────────────────────────────────────────────────────────────────────
// Null manager guard test
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuDaoBearer::testNullManager_returnsNullptr()
{
    qApp->setProperty("NetworkManagerWS", QVariant());

    QCOMPARE(IntervalsIcuDAO::getAthleteBearer(ATHLETE_ID, BEARER_TOKEN), nullptr);
    QCOMPARE(IntervalsIcuDAO::getAthleteSettingsBearer(ATHLETE_ID, BEARER_TOKEN), nullptr);

    // Restore
    qApp->setProperty("NetworkManagerWS",
                      QVariant::fromValue<QNetworkAccessManager *>(m_manager));
}

// ─────────────────────────────────────────────────────────────────────────────
QTEST_MAIN(TstIntervalsIcuDaoBearer)
#include "tst_intervals_icu_dao_bearer.moc"
