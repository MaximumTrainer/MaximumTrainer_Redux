#pragma once

#include <QtTest/QtTest>
#include <QNetworkAccessManager>

// ─────────────────────────────────────────────────────────────────────────────
// Live integration test class for IntervalsIcuApi.
// Q_OBJECT declared here (in the header) so qmake reliably runs moc on it.
// ─────────────────────────────────────────────────────────────────────────────
class TstIntervalsIcuIntegration : public QObject
{
    Q_OBJECT

    QString m_apiKey;
    QString m_athleteId;
    QNetworkAccessManager *m_manager = nullptr;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testGetAthlete();
    void testGetEvents();
    void testGetWorkouts();
    void testBadApiKey();
};
