#ifndef POSTWORKOUTPANEL_H
#define POSTWORKOUTPANEL_H

#include <QList>
#include <QString>
#include <QWidget>

#include "achievement.h"

class Account;
class QNetworkReply;
class QTimer;
class QBoxLayout;
class QLabel;

/// Overlay shown when a workout finishes: the ride summary (stats and time in
/// power zones), earned achievements, an FTP suggestion, plus Strava and
/// Intervals.icu upload of the recorded FIT file (manual button or automatic,
/// with Strava token refresh, a one-shot 401 retry and upload status polling).
class PostWorkoutPanel : public QWidget
{
    Q_OBJECT

public:
    PostWorkoutPanel(Account *account, const QList<Achievement> &earnedAchievements,
                     const QString &fitFilePath, const QString &fitFileName,
                     const QString &fitFileDescription, QWidget *parent);

    /// A line under the title about the profile update (e.g. a test result
    /// saved to Intervals.icu). Can be called again when the save finishes.
    void setProfileNote(const QString &text);

signals:
    /// The rider accepted a detected FTP; zones and targets need recomputing.
    void ftpUpdated(int ftpWatts);

private slots:
    void uploadToStrava();
    void doStravaUpload();   ///< the actual upload, after any token refresh
    void uploadToIntervalsIcu();
    void slotPostIntervalsIcuUploadDone();
    void slotPostStravaUploadDone();
    void slotPostStravaCheckStatus();
    void slotPostStravaStatusDone();

private:
    void addRideSummary(QBoxLayout *layout);
    void addFtpSuggestion(QBoxLayout *layout);
    void acceptFtp(int ftpWatts);
    void setStravaPostStatus(const QString &text, bool retryable);
    void setIntervalsIcuPostStatus(const QString &text, bool retryable);

    Account *account;
    const QString fitFilePath;
    const QString fitFileName;
    const QString fitFileDescription;

    QNetworkReply *replyPostStravaUpload       = nullptr;
    QNetworkReply *replyPostStravaStatus       = nullptr;
    QTimer        *timerPostStravaStatus       = nullptr;
    qint64         stravaUploadID_post         = 0;
    QNetworkReply *replyPostIntervalsIcuUpload = nullptr;
    bool           m_stravaUpload401Retried    = false;
    QLabel        *m_ftpStatus                 = nullptr;
    QLabel        *m_profileNote               = nullptr;
};

#endif // POSTWORKOUTPANEL_H
