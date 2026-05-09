#pragma once

#include <QtTest/QtTest>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QNetworkAccessManager>
#include <QByteArray>

// ─────────────────────────────────────────────────────────────────────────────
// OnlineModeWindow
//
// 1280x720 window that represents the MaximumTrainer online-mode login state.
// Q_OBJECT declared here (in the header) so qmake reliably runs moc on it.
// ─────────────────────────────────────────────────────────────────────────────
class OnlineModeWindow : public QWidget
{
    Q_OBJECT

public:
    explicit OnlineModeWindow(const QString &athleteId,
                              const QString &timestamp,
                              QWidget       *parent = nullptr);

    void markConnected(const QString &confirmedId, const QString &athleteName, int httpStatus);
    void markFailed(int httpStatus, const QString &errorString);

private:
    QLabel *m_statusBadge      = nullptr;
    QLabel *m_requestedIdLabel = nullptr;
    QLabel *m_confirmedIdLabel = nullptr;
    QLabel *m_athleteNameLabel = nullptr;
    QLabel *m_httpStatusLabel  = nullptr;
    QLabel *m_buildLabel       = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// ResultWindow
//
// Generic 1280x720 dark-themed window for integration test result display.
// Shows a title, a configurable list of key-value rows, and a status badge
// that is updated to PASSED / FAILED once the test completes.
// Q_OBJECT declared here so qmake reliably runs moc on it.
// ─────────────────────────────────────────────────────────────────────────────
class ResultWindow : public QWidget
{
    Q_OBJECT

public:
    explicit ResultWindow(const QString &testTitle,
                          const QString &subtitle,
                          const QString &timestamp,
                          QWidget       *parent = nullptr);

    /// Append a labelled row to the result panel.
    /// @param highlight  When true the value is rendered in green (good news).
    void addRow(const QString &key, const QString &value, bool highlight = false);

    void markPassed(const QString &summary);
    void markFailed(const QString &summary);

private:
    QLabel      *m_statusBadge = nullptr;
    QVBoxLayout *m_rowsLayout  = nullptr;
};

// ─────────────────────────────────────────────────────────────────────────────
// TstOnlineMode — QTest class
//
// Q_OBJECT declared here (in the header) so qmake reliably runs moc on it.
// ─────────────────────────────────────────────────────────────────────────────
class TstOnlineMode : public QObject
{
    Q_OBJECT

    QString                m_apiKey;
    QString                m_athleteId;
    QString                m_pushedWorkoutId;    // set by testWorkoutPush; cleaned in cleanupTestCase
    QString                m_uploadedActivityId; // set by testActivityUpload; cleaned in cleanupTestCase
    QString                m_createdEventId;     // set by testCalendarPlan; cleaned in cleanupTestCase
    QByteArray             m_generatedTcx;       // set by testRunWorkout; consumed by testActivityUpload
    QDateTime              m_activityStartTime;  // set by testRunWorkout; used for date-window queries
    QNetworkAccessManager *m_manager = nullptr;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testOnlineModeAuthentication();
    void testCalendar();
    void testWorkoutPush();
    void testWorkoutPull();
    void testRunWorkout();
    void testActivityUpload();
    void testWorkoutHistory();
    void testCalendarPlan();
};
