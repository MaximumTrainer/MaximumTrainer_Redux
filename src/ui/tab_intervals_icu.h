#ifndef TAB_INTERVALS_ICU_H
#define TAB_INTERVALS_ICU_H

#include <QWidget>
#include <QDate>
#include <QList>
#include <QNetworkReply>

#include "intervalsicuservice.h"

namespace Ui {
class TabIntervalsIcu;
}

///
/// Native Qt widget that replaces the former TrainerDay WebView tab.
///
/// Shows the user's Intervals.icu calendar for the selected week and allows
/// downloading a planned workout as a .zwo file directly into the local
/// workout folder.
///
class TabIntervalsIcu : public QWidget
{
    Q_OBJECT

public:
    explicit TabIntervalsIcu(QWidget *parent = nullptr);
    ~TabIntervalsIcu();

    /// Call this once the Account credentials are available (or after they
    /// change in the preferences dialog).
    void refreshCredentials();

    /// Update the widget for the current network state.
    /// When \a isOnline is false the calendar controls are hidden and an
    /// offline banner is shown.
    void setOnlineMode(bool isOnline);

    /// Fetch calendar for [from, to] and import every event that has a workout
    /// file.  Results are communicated via syncFinished() / syncFailed().
    /// Non-blocking — the current week view is not affected.
    void startBatchSync(const QDate &from, const QDate &to);

signals:
    /// Emitted after a .zwo file has been written to disk.
    /// @param workoutName  base name (without extension) of the saved file.
    void workoutDownloaded(const QString &workoutName);

    /// Emitted when a batch sync completes successfully.
    /// @param count  number of workouts successfully imported.
    void syncFinished(int count);

    /// Emitted when a batch sync fails before any imports complete.
    void syncFailed(const QString &errorMessage);

private slots:
    void onRefreshClicked();
    void onPrevWeekClicked();
    void onNextWeekClicked();
    void onLoadWorkoutClicked();
    void onCalendarFetchFinished();
    void onWorkoutDownloadFinished();

    void onSyncAllClicked();
    void onSyncCalendarFetched();
    void onSyncWorkoutDownloaded();

private:
    void updateWeekLabel();
    void populateTable(const QList<IntervalsIcuService::CalendarEvent> &events);
    void setStatus(const QString &msg);
    void setBusy(bool busy);

    void downloadNextSyncWorkout();

    Ui::TabIntervalsIcu *ui;

    IntervalsIcuService *m_service;
    QDate m_weekStart;   ///< Monday of the currently displayed week

    QNetworkReply *m_calendarReply  = nullptr;
    QNetworkReply *m_downloadReply  = nullptr;
    QString        m_pendingWorkoutName; ///< name captured at download start

    // Parallel list of workout IDs for each table row (empty string = no
    // downloadable workout for that event)
    QList<QString> m_rowWorkoutIds;

    // Batch sync state
    QNetworkReply *m_syncCalendarReply  = nullptr;
    QNetworkReply *m_syncDownloadReply  = nullptr;
    QList<IntervalsIcuService::CalendarEvent> m_syncQueue;
    QString m_pendingSyncWorkoutName;
    int m_syncCount  = 0;
    int m_syncTotal  = 0;
};

#endif // TAB_INTERVALS_ICU_H
