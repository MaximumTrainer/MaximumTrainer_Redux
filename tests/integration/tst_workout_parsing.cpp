/*
 * tst_workout_parsing.cpp
 *
 * Live Intervals.icu Workout Pull-and-Load Test — MaximumTrainer
 *
 * This file contains a single live integration test that makes a real HTTPS
 * request to intervals.icu/api/v1 to fetch, download, and parse a ZWO workout
 * file.  QSKIP is issued when credentials are absent.
 *
 * ZWO inline-string parsing tests and programmatic model tests were removed;
 * canonical coverage lives in:
 *   • tst_workout_io.cpp      (ZWO parsing, XML round-trip, mock JSON)
 *   • tst_workout_creator.cpp (Workout / Interval model construction)
 *
 * Build (no display required):
 *   cd tests/integration
 *   qmake workout_parsing_tests.pro && make -j$(nproc)
 *   ../../build/tests/workout_parsing_tests -v2
 */

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QByteArray>
#include <QDate>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include "importerworkoutzwo.h"
#include "account.h"
#include "workout.h"
#include "interval.h"

// ---------------------------------------------------------------------------
// Helper: spin the event loop until the reply finishes or the timeout fires.
// ---------------------------------------------------------------------------
static bool waitForNetworkReply(QNetworkReply *reply, int timeoutMs = 30000)
{
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    loop.exec();
    return reply->isFinished();
}

// ---------------------------------------------------------------------------
// TstWorkoutParsing
// ---------------------------------------------------------------------------
class TstWorkoutParsing : public QObject
{
    Q_OBJECT

private:
    Account *m_account = nullptr;

private slots:

    // ── Setup ────────────────────────────────────────────────────────────────
    void initTestCase()
    {
        // Register a dummy Account so Workout::calculateWorkoutMetrics() works.
        m_account = new Account(qApp);
        m_account->FTP  = 200; // W
        m_account->LTHR = 160; // bpm
        m_account->display_name = QStringLiteral("CI Test User");
        qApp->setProperty("Account", QVariant::fromValue<Account*>(m_account));
    }

    void cleanupTestCase()
    {
        qApp->setProperty("Account", QVariant());
    }

    // ========================================================================
    // Intervals.icu pull-and-load workflow (live, optional)
    // ========================================================================
    // 3a. Pull workout list from intervals.icu, download ZWO for the first
    //     workout found, parse it, and verify each interval has a positive
    //     duration.  QSKIP when credentials are absent.
    // ────────────────────────────────────────────────────────────────────────
    void testIntervalsIcuPullAndLoad()
    {
        const QString apiKey    = qEnvironmentVariable("INTERVALS_ICU_API_KEY");
        const QString athleteId = qEnvironmentVariable("INTERVALS_ICU_ATHLETE_ID");

        if (apiKey.isEmpty() || athleteId.isEmpty()) {
            QSKIP("Set INTERVALS_ICU_API_KEY and INTERVALS_ICU_ATHLETE_ID "
                  "to run the live intervals.icu pull-and-load test.");
        }

        const QString baseUrl =
            QStringLiteral("https://intervals.icu/api/v1/athlete/") + athleteId;

        // Build the Basic Auth header (username="API_KEY", password=<apiKey>)
        const QByteArray creds =
            (QStringLiteral("API_KEY:") + apiKey).toUtf8().toBase64();
        const QByteArray authHeader = QByteArrayLiteral("Basic ") + creds;

        QNetworkAccessManager mgr;

        // ── Step 1: Fetch the athlete's recent workout list ─────────────────
        const QUrl listUrl(baseUrl
                           + QStringLiteral("/workouts?oldest=")
                           + QDate::currentDate().addDays(-90).toString(Qt::ISODate)
                           + QStringLiteral("&newest=")
                           + QDate::currentDate().toString(Qt::ISODate));

        QNetworkRequest listReq(listUrl);
        listReq.setRawHeader("Authorization", authHeader);
        listReq.setRawHeader("Accept", "application/json");

        QNetworkReply *listReply = mgr.get(listReq);
        QVERIFY2(waitForNetworkReply(listReply), "Workout list request timed out (30 s)");

        const int listStatus =
            listReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QVERIFY2(listStatus == 200,
                 qPrintable(QString("Expected HTTP 200 for workout list, got %1 — %2")
                                .arg(listStatus)
                                .arg(listReply->errorString())));

        const QByteArray listBody = listReply->readAll();
        listReply->deleteLater();

        const QJsonDocument listDoc = QJsonDocument::fromJson(listBody);
        QVERIFY2(!listDoc.isNull() && listDoc.isArray(),
                 "Workout list response must be a JSON array");

        const QJsonArray workouts = listDoc.array();
        if (workouts.isEmpty()) {
            QSKIP("No workouts found in the last 90 days — skipping ZWO load test");
        }

        // ── Step 2: Collect all workout IDs from the list ────────────────────
        QStringList workoutIds;
        for (const QJsonValue &v : workouts) {
            const QJsonObject obj = v.toObject();
            // The scheduled-workout endpoint uses "id" as a numeric long
            const QJsonValue idVal = obj.value(QStringLiteral("id"));
            if (!idVal.isUndefined() && !idVal.isNull()) {
                const QString id = idVal.isString()
                    ? idVal.toString()
                    : QString::number(static_cast<qlonglong>(idVal.toDouble()));
                if (!id.isEmpty() && id != QStringLiteral("0"))
                    workoutIds << id;
            }
        }

        if (workoutIds.isEmpty()) {
            QSKIP("No workout with a valid 'id' found in the 90-day list — "
                  "skipping ZWO load test");
        }

        // ── Step 3: Walk the full list, try each workout until one has ZWO ───
        // Races, free-form notes, and other non-structured entries return non-200
        // for the .zwo endpoint. We scan all IDs so a valid fixture is always
        // found when the account contains any structured workout in the window.
        QString workoutId;
        QByteArray zwoData;

        for (const QString &id : workoutIds) {
            const QUrl zwoUrl(baseUrl
                              + QStringLiteral("/workouts/")
                              + id
                              + QStringLiteral(".zwo"));

            QNetworkRequest zwoReq(zwoUrl);
            zwoReq.setRawHeader("Authorization", authHeader);
            zwoReq.setRawHeader("Accept", "application/xml");

            QNetworkReply *zwoReply = mgr.get(zwoReq);
            QVERIFY2(waitForNetworkReply(zwoReply),
                     qPrintable(QString("ZWO download timed out (30 s) for workout %1").arg(id)));

            const int zwoStatus =
                zwoReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

            if (zwoStatus == 200) {
                const QByteArray body = zwoReply->readAll();
                zwoReply->deleteLater();
                if (!body.isEmpty()) {
                    workoutId = id;
                    zwoData   = body;
                    break; // Found a usable ZWO — proceed with this one.
                }
            } else {
                // Non-200 means this workout type has no ZWO; try the next one.
                zwoReply->deleteLater();
            }
        }

        if (workoutId.isEmpty()) {
            QSKIP("None of the workouts in the last 90 days returned ZWO data — "
                  "all entries appear to be races, notes, or non-structured workouts");
        }

        // ── Step 4: Parse the ZWO into a Workout model ───────────────────────
        const Workout loaded = ImporterWorkoutZwo::importFromByteArray(zwoData, workoutId);

        QVERIFY2(loaded.getLstInterval().size() >= 1,
                 qPrintable(
                     QString("Expected >= 1 interval after parsing ZWO for workout %1, "
                             "got %2")
                         .arg(workoutId)
                         .arg(loaded.getLstInterval().size())));

        // ── Step 5: Verify every interval has a positive duration ────────────
        const QList<Interval> loadedIntervals = loaded.getLstInterval();
        for (int i = 0; i < loadedIntervals.size(); ++i) {
            const Interval iv = loadedIntervals.at(i);
            const QTime     qt = iv.getDurationQTime();
            const int    durSec = qt.hour() * 3600 + qt.minute() * 60 + qt.second();
            QVERIFY2(durSec >= 1,
                     qPrintable(QString("Interval %1 of workout %2 has zero or negative "
                                        "duration (QTime=%3)")
                                    .arg(i)
                                    .arg(workoutId)
                                    .arg(qt.toString())));
        }

        qDebug().noquote()
            << "[IntervalsIcuPullAndLoad] PASS"
            << "— workout id:" << workoutId
            << "| intervals:" << loadedIntervals.size();
    }
};

QTEST_MAIN(TstWorkoutParsing)
#include "tst_workout_parsing.moc"
