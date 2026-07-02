/*
 * tst_intervals_icu_oauth_exchange.cpp
 *
 * Qt Test suite for ExtRequest::intervalsIcuOAuthExchange /
 * intervalsIcuOAuthRefresh and Util::parseJsonIntervalsIcuOAuthErrorPayload.
 *
 * The Intervals.icu OAuth callback (system-browser loopback on desktop, popup
 * on WASM) posts the authorization code to the Cloudflare Worker proxy at
 * URL_TOKEN_ICV.  The Worker looks up the client_secret from its
 * CLIENT_SECRETS KV namespace keyed by the client_id supplied by the caller,
 * so the request body MUST carry client_id in addition to code and
 * redirect_uri.  These tests pin the outgoing request shape.
 *
 * Tests inject a MockNetworkAccessManager that captures the last outgoing
 * body and headers without touching the network.
 */

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QBuffer>

#include "extrequest.h"
#include "environnement.h"

// Forward-declare only the Util method under test so we don't have to pull in
// util.h (which transitively depends on QWT, Account, Settings, XmlUtil, etc.
// — all irrelevant here).  A tiny util_parse_error_stub.cpp compiled into this
// test target provides the definition, copied verbatim from src/app/util.cpp.
class Util
{
public:
    static QString parseJsonIntervalsIcuOAuthErrorPayload(const QString &data);
};

// ─────────────────────────────────────────────────────────────────────────────
// FinishedReply — stub QNetworkReply that never opens a socket.
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
        setOperation(QNetworkAccessManager::PostOperation);
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200);
        setFinished(true);
        QTimer::singleShot(0, this, &FinishedReply::finished);
    }
    qint64 bytesAvailable() const override { return 0; }
    bool   isSequential()   const override { return true; }
    void   abort()                override {}
protected:
    qint64 readData(char *, qint64) override { return -1; }
};

// ─────────────────────────────────────────────────────────────────────────────
// MockNetworkAccessManager — captures the last request, headers, and body.
// ─────────────────────────────────────────────────────────────────────────────
class MockNetworkAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit MockNetworkAccessManager(QObject *parent = nullptr)
        : QNetworkAccessManager(parent) {}

    QNetworkRequest lastRequest;
    QByteArray      lastBody;
    Operation       lastOperation = GetOperation;
    int             callCount = 0;

    void reset()
    {
        callCount = 0;
        lastRequest = QNetworkRequest();
        lastBody.clear();
        lastOperation = GetOperation;
    }

protected:
    QNetworkReply* createRequest(Operation op,
                                 const QNetworkRequest &request,
                                 QIODevice *outgoingData) override
    {
        lastRequest   = request;
        lastOperation = op;
        lastBody.clear();
        if (outgoingData) {
            lastBody = outgoingData->readAll();
        }
        ++callCount;
        return new FinishedReply(request, this);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
class TstIntervalsIcuOAuthExchange : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    // ── intervalsIcuOAuthExchange ──────────────────────────────────────────
    void testExchange_bodyIsJson();
    void testExchange_bodyContainsCodeRedirectUriClientId();
    void testExchange_contentTypeIsJson();
    void testExchange_urlIsTokenProxy();
    void testExchange_emptyClientIdReturnsNullptr();
    void testExchange_nullManagerReturnsNullptr();

    // ── intervalsIcuOAuthRefresh ───────────────────────────────────────────
    void testRefresh_bodyContainsRefreshTokenClientIdGrantType();
    void testRefresh_emptyClientIdReturnsNullptr();

    // ── parseJsonIntervalsIcuOAuthErrorPayload ─────────────────────────────
    void testParseErrorPayload_missingClientId();
    void testParseErrorPayload_unauthorizedClient();
    void testParseErrorPayload_kvUnavailable();
    void testParseErrorPayload_emptyString();
    void testParseErrorPayload_malformedJson();
    void testParseErrorPayload_notAnObject();
    void testParseErrorPayload_objectWithoutErrorField();

private:
    MockNetworkAccessManager *m_manager = nullptr;

    // Fixed test constants — do NOT depend on Environnement so the test is
    // hermetic (no CredentialStore reads, no build-time defines).
    static constexpr const char CLIENT_ID[]     = "259";
    static constexpr const char CODE[]          = "test_auth_code_abc";
    static constexpr const char REDIRECT_URI[]  = "http://localhost:43210/";
    static constexpr const char REFRESH_TOKEN[] = "rt_test_refresh_token";
};

// ─────────────────────────────────────────────────────────────────────────────
void TstIntervalsIcuOAuthExchange::initTestCase()
{
    m_manager = new MockNetworkAccessManager(this);
    qApp->setProperty("NetworkManagerWS",
                      QVariant::fromValue<QNetworkAccessManager*>(m_manager));
}

void TstIntervalsIcuOAuthExchange::cleanupTestCase()
{
    qApp->setProperty("NetworkManagerWS", QVariant());
}

void TstIntervalsIcuOAuthExchange::init()
{
    m_manager->reset();
}

// ─────────────────────────────────────────────────────────────────────────────
// intervalsIcuOAuthExchange
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuOAuthExchange::testExchange_bodyIsJson()
{
    QNetworkReply *reply =
        ExtRequest::intervalsIcuOAuthExchange(CODE, REDIRECT_URI, CLIENT_ID);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    QJsonDocument doc = QJsonDocument::fromJson(m_manager->lastBody);
    QVERIFY2(!doc.isNull(),
             qPrintable(QString("body is not valid JSON: %1")
                            .arg(QString::fromUtf8(m_manager->lastBody))));
    QVERIFY(doc.isObject());
}

void TstIntervalsIcuOAuthExchange::testExchange_bodyContainsCodeRedirectUriClientId()
{
    QNetworkReply *reply =
        ExtRequest::intervalsIcuOAuthExchange(CODE, REDIRECT_URI, CLIENT_ID);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    const QJsonObject obj = QJsonDocument::fromJson(m_manager->lastBody).object();
    QCOMPARE(obj.value(QStringLiteral("code")).toString(),         QString(CODE));
    QCOMPARE(obj.value(QStringLiteral("redirect_uri")).toString(), QString(REDIRECT_URI));
    QCOMPARE(obj.value(QStringLiteral("client_id")).toString(),    QString(CLIENT_ID));
}

void TstIntervalsIcuOAuthExchange::testExchange_contentTypeIsJson()
{
    QNetworkReply *reply =
        ExtRequest::intervalsIcuOAuthExchange(CODE, REDIRECT_URI, CLIENT_ID);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    const QString contentType =
        m_manager->lastRequest.header(QNetworkRequest::ContentTypeHeader).toString();
    QVERIFY2(contentType.toLower().contains(QStringLiteral("application/json")),
             qPrintable(QString("Content-Type must be application/json, got: %1")
                            .arg(contentType)));
}

void TstIntervalsIcuOAuthExchange::testExchange_urlIsTokenProxy()
{
    QNetworkReply *reply =
        ExtRequest::intervalsIcuOAuthExchange(CODE, REDIRECT_URI, CLIENT_ID);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    QCOMPARE(m_manager->lastRequest.url().toString(), URL_TOKEN_ICV);
    QCOMPARE(m_manager->lastOperation, QNetworkAccessManager::PostOperation);
}

void TstIntervalsIcuOAuthExchange::testExchange_emptyClientIdReturnsNullptr()
{
    QNetworkReply *reply =
        ExtRequest::intervalsIcuOAuthExchange(CODE, REDIRECT_URI, QString());
    QVERIFY2(reply == nullptr,
             "intervalsIcuOAuthExchange must reject an empty client_id");
    QCOMPARE(m_manager->callCount, 0);
}

void TstIntervalsIcuOAuthExchange::testExchange_nullManagerReturnsNullptr()
{
    // Detach the mock manager so ExtRequest cannot obtain it.
    qApp->setProperty("NetworkManagerWS", QVariant());

    QNetworkReply *reply =
        ExtRequest::intervalsIcuOAuthExchange(CODE, REDIRECT_URI, CLIENT_ID);
    QVERIFY(reply == nullptr);

    // Restore for subsequent tests.
    qApp->setProperty("NetworkManagerWS",
                      QVariant::fromValue<QNetworkAccessManager*>(m_manager));
}

// ─────────────────────────────────────────────────────────────────────────────
// intervalsIcuOAuthRefresh
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuOAuthExchange::testRefresh_bodyContainsRefreshTokenClientIdGrantType()
{
    QNetworkReply *reply =
        ExtRequest::intervalsIcuOAuthRefresh(REFRESH_TOKEN, CLIENT_ID);
    QVERIFY(reply != nullptr);
    reply->deleteLater();

    const QJsonObject obj = QJsonDocument::fromJson(m_manager->lastBody).object();
    QCOMPARE(obj.value(QStringLiteral("grant_type")).toString(),    QStringLiteral("refresh_token"));
    QCOMPARE(obj.value(QStringLiteral("refresh_token")).toString(), QString(REFRESH_TOKEN));
    QCOMPARE(obj.value(QStringLiteral("client_id")).toString(),     QString(CLIENT_ID));

    const QString contentType =
        m_manager->lastRequest.header(QNetworkRequest::ContentTypeHeader).toString();
    QVERIFY(contentType.toLower().contains(QStringLiteral("application/json")));
}

void TstIntervalsIcuOAuthExchange::testRefresh_emptyClientIdReturnsNullptr()
{
    QNetworkReply *reply =
        ExtRequest::intervalsIcuOAuthRefresh(REFRESH_TOKEN, QString());
    QVERIFY2(reply == nullptr,
             "intervalsIcuOAuthRefresh must reject an empty client_id");
    QCOMPARE(m_manager->callCount, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// parseJsonIntervalsIcuOAuthErrorPayload
// ─────────────────────────────────────────────────────────────────────────────

void TstIntervalsIcuOAuthExchange::testParseErrorPayload_missingClientId()
{
    QCOMPARE(Util::parseJsonIntervalsIcuOAuthErrorPayload(
                 QStringLiteral("{\"error\":\"missing_client_id\"}")),
             QStringLiteral("missing_client_id"));
}

void TstIntervalsIcuOAuthExchange::testParseErrorPayload_unauthorizedClient()
{
    QCOMPARE(Util::parseJsonIntervalsIcuOAuthErrorPayload(
                 QStringLiteral("{\"error\":\"unauthorized_client\"}")),
             QStringLiteral("unauthorized_client"));
}

void TstIntervalsIcuOAuthExchange::testParseErrorPayload_kvUnavailable()
{
    QCOMPARE(Util::parseJsonIntervalsIcuOAuthErrorPayload(
                 QStringLiteral("{\"error\":\"kv_unavailable\"}")),
             QStringLiteral("kv_unavailable"));
}

void TstIntervalsIcuOAuthExchange::testParseErrorPayload_emptyString()
{
    QCOMPARE(Util::parseJsonIntervalsIcuOAuthErrorPayload(QString()),
             QString());
}

void TstIntervalsIcuOAuthExchange::testParseErrorPayload_malformedJson()
{
    QCOMPARE(Util::parseJsonIntervalsIcuOAuthErrorPayload(
                 QStringLiteral("not{json")),
             QString());
}

void TstIntervalsIcuOAuthExchange::testParseErrorPayload_notAnObject()
{
    QCOMPARE(Util::parseJsonIntervalsIcuOAuthErrorPayload(
                 QStringLiteral("[\"array\", \"not\", \"object\"]")),
             QString());
}

void TstIntervalsIcuOAuthExchange::testParseErrorPayload_objectWithoutErrorField()
{
    QCOMPARE(Util::parseJsonIntervalsIcuOAuthErrorPayload(
                 QStringLiteral("{\"access_token\":\"abc\"}")),
             QString());
}

QTEST_MAIN(TstIntervalsIcuOAuthExchange)
#include "tst_intervals_icu_oauth_exchange.moc"
