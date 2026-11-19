#include "postworkoutpanel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QLabel>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "account.h"
#include "achievementcatalog.h"
#include "app_settings.h"
#include "managerachievement.h"
#include "ftpestimator.h"
#include "intervalsicuftpsync.h"
#include "setting_keys.h"
#include "intervalsicuservice.h"
#include "logger.h"
#include "strava_service.h"
#include "util.h"

PostWorkoutPanel::PostWorkoutPanel(Account *account, const QList<Achievement> &earnedAchievements,
                                   const QString &fitFilePath, const QString &fitFileName,
                                   const QString &fitFileDescription, QWidget *parent)
    : QWidget(parent)
    , account(account)
    , fitFilePath(fitFilePath)
    , fitFileName(fitFileName)
    , fitFileDescription(fitFileDescription)
{
    setObjectName("widgetPostWorkout");
    setStyleSheet(
        "QWidget#widgetPostWorkout { background-color: rgba(10,10,10,220); border-radius: 12px; }"
        "QLabel  { color: white; }"
        "QPushButton { background-color: #2a6099; color: white; border-radius: 6px; padding: 11px 28px; font-size: 12pt; }"
        "QPushButton:hover { background-color: #3275bd; }"
        "QPushButton:disabled { background-color: #1a3a59; color: #888; }");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(28, 22, 28, 22);
    setMinimumWidth(460);

    auto *titleLabel = new QLabel(tr("Workout Complete!"), this);
    titleLabel->setStyleSheet("font-size: 14pt; font-weight: bold; color: #80c0ff;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // — Achievements section —
    if (!earnedAchievements.isEmpty()) {
        auto *achHeader = new QLabel(tr("Achievements Earned:"), this);
        achHeader->setStyleSheet("font-size: 10pt; font-weight: bold; color: #f0c040; margin-top: 6px;");
        layout->addWidget(achHeader);
        for (const Achievement &a : earnedAchievements) {
            auto *row = new QLabel(QString("🏆  %1").arg(a.getName()), this);
            row->setStyleSheet("font-size: 10pt;");
            layout->addWidget(row);
        }
    }

    addFtpSuggestion(layout);

    // — Upload section —
    const bool hasStrava   = !account->strava_access_token.isEmpty();
    const bool hasIcu      = !account->intervals_icu_athlete_id.isEmpty() &&
                          (!account->intervals_icu_api_key.isEmpty() ||
                           !account->intervals_icu_access_token.isEmpty());

    if (hasStrava || hasIcu) {
        auto *upHeader = new QLabel(tr("Upload Activity:"), this);
        upHeader->setStyleSheet("font-size: 11pt; font-weight: bold; color: #80c0ff; margin-top: 10px;");
        layout->addWidget(upHeader);

        if (hasStrava) {
            // Manual mode (auto-upload off) shows a button to trigger the upload;
            // both modes share a word-wrapped status label that shows progress
            // and the full error text on failure.
            if (!account->strava_auto_upload) {
                auto *btn = new QPushButton(tr("Upload to Strava"), this);
                btn->setObjectName("btnStrava");
                connect(btn, &QPushButton::clicked, this, &PostWorkoutPanel::uploadToStrava);
                layout->addWidget(btn);
            }
            auto *lbl = new QLabel(this);
            lbl->setObjectName("lblStrava");
            lbl->setWordWrap(true);
            lbl->setStyleSheet("font-size: 11pt;");
            // Allow the "View on Strava" link (shown on success) to open the
            // activity in the user's browser.
            lbl->setOpenExternalLinks(true);
            if (account->strava_auto_upload)
                lbl->setText(tr("Uploading to Strava…"));
            layout->addWidget(lbl);
            if (account->strava_auto_upload)
                QTimer::singleShot(0, this, &PostWorkoutPanel::uploadToStrava);
        }
        if (hasIcu) {
            if (!account->intervals_icu_auto_upload) {
                auto *btn = new QPushButton(tr("Upload to Intervals.icu"), this);
                btn->setObjectName("btnIntervalsIcu");
                connect(btn, &QPushButton::clicked, this, &PostWorkoutPanel::uploadToIntervalsIcu);
                layout->addWidget(btn);
            }
            auto *lbl = new QLabel(this);
            lbl->setObjectName("lblIntervalsIcu");
            lbl->setWordWrap(true);
            lbl->setStyleSheet("font-size: 11pt;");
            // Allow the "View on Intervals.icu" link (shown on success) to
            // open the activity in the user's browser.
            lbl->setOpenExternalLinks(true);
            if (account->intervals_icu_auto_upload)
                lbl->setText(tr("Uploading to Intervals.icu…"));
            layout->addWidget(lbl);
            if (account->intervals_icu_auto_upload)
                QTimer::singleShot(0, this, &PostWorkoutPanel::uploadToIntervalsIcu);
        }
    }

    adjustSize();
}

// Suggests a higher FTP when the recent rides show one (see FtpEstimator).
void PostWorkoutPanel::addFtpSuggestion(QBoxLayout *layout)
{
    if (account->enable_studio_mode || account->rowing_mode || account->FTP <= 0)
        return;
    const FtpEstimator::Estimate estimate = FtpEstimator::fromRides(
        FtpEstimator::recentRides(Util::getSystemPathHistory(), QDateTime::currentDateTime()));
    const int dismissed = AppSettings::read(SettingKeys::Training::FtpSuggestionDismissedW);
    if (!estimate.valid || !FtpEstimator::worthSuggesting(account->FTP, estimate.ftpW, dismissed))
        return;

    auto *header = new QLabel(tr("New FTP detected: %1 W").arg(estimate.ftpW), this);
    header->setStyleSheet("font-size: 11pt; font-weight: bold; color: #80c0ff; margin-top: 10px;");
    layout->addWidget(header);
    auto *detail = new QLabel(tr("Your best 20 minutes in the last %1 weeks was %2 W, so your FTP "
                                 "is about %3 W (currently %4 W).")
                                  .arg(FtpEstimator::kWindowDays / 7).arg(estimate.best20W)
                                  .arg(estimate.ftpW).arg(account->FTP), this);
    detail->setWordWrap(true);
    detail->setStyleSheet("font-size: 10pt;");
    layout->addWidget(detail);

    auto *buttons = new QHBoxLayout();
    auto *update = new QPushButton(tr("Update FTP"), this);
    auto *later  = new QPushButton(tr("Not now"), this);
    later->setStyleSheet("background-color: #444;");
    buttons->addWidget(update);
    buttons->addWidget(later);
    layout->addLayout(buttons);
    m_ftpStatus = new QLabel(this);
    m_ftpStatus->setWordWrap(true);
    m_ftpStatus->setStyleSheet("font-size: 10pt;");
    m_ftpStatus->hide();
    layout->addWidget(m_ftpStatus);

    const int ftp = estimate.ftpW;
    connect(update, &QPushButton::clicked, this, [this, ftp, update, later]() {
        update->setEnabled(false);
        later->setEnabled(false);
        acceptFtp(ftp);
    });
    connect(later, &QPushButton::clicked, this, [this, ftp, update, later]() {
        AppSettings::write(SettingKeys::Training::FtpSuggestionDismissedW, ftp);
        update->setEnabled(false);
        later->setEnabled(false);
        m_ftpStatus->setText(tr("FTP left at %1 W.").arg(account->FTP));
        m_ftpStatus->show();
        adjustSize();
    });
}

void PostWorkoutPanel::acceptFtp(int ftpWatts)
{
    if (auto *achievements = qApp->property("ManagerAchievement").value<ManagerAchievement*>();
        achievements && ftpWatts > account->FTP)
        achievements->recordEvent(AchievementCatalog::Events::FtpRaised);
    account->saveProfileFields(ftpWatts, account->LTHR, account->weight_kg);
    AppSettings::write(SettingKeys::Training::FtpSuggestionDismissedW, ftpWatts);
    emit ftpUpdated(ftpWatts);
    m_ftpStatus->show();

    // Login copies the FTP from Intervals.icu, so save it there too or the
    // next login would put the old value back.
    if (account->intervals_icu_access_token.isEmpty() || account->intervals_icu_athlete_id.isEmpty()) {
        m_ftpStatus->setText(tr("✓ FTP updated to %1 W.").arg(ftpWatts));
        adjustSize();
        return;
    }
    m_ftpStatus->setText(tr("FTP updated to %1 W. Saving to Intervals.icu…").arg(ftpWatts));
    adjustSize();
    auto *sync = new IntervalsIcuFtpSync(account->intervals_icu_athlete_id,
                                         account->intervals_icu_access_token, ftpWatts, this);
    connect(sync, &IntervalsIcuFtpSync::finished, this, [this, ftpWatts](bool ok, const QString &error) {
        if (ok)
            m_ftpStatus->setText(tr("✓ FTP updated to %1 W, here and on Intervals.icu.").arg(ftpWatts));
        else
            m_ftpStatus->setText(tr("FTP updated to %1 W here, but saving it to Intervals.icu failed (%2). "
                                    "Your next login may restore the old value.").arg(ftpWatts).arg(error));
        adjustSize();
    });
    sync->start();
}

// Updates the post-workout Strava status — the manual button (auto-upload off)
// or the status label (auto-upload on), whichever is present.
void PostWorkoutPanel::setStravaPostStatus(const QString &text, bool retryable)
{
    if (auto *lbl = findChild<QLabel*>("lblStrava"))
        lbl->setText(text);
    // The manual button (if any) is re-enabled only when a retry makes sense.
    if (auto *btn = findChild<QPushButton*>("btnStrava"))
        btn->setEnabled(retryable);
    adjustSize();
}

void PostWorkoutPanel::uploadToStrava()
{
    if (fitFilePath.isEmpty()) return;

    // Access tokens live only 6 h — refresh via the token Worker if expired,
    // then upload.
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (account->strava_token_expires_at > now + 60
        || account->strava_refresh_token.isEmpty()) {
        doStravaUpload();
        return;
    }
    setStravaPostStatus(tr("Refreshing Strava token…"), false);
    QNetworkReply *refresh = StravaService::refreshToken(account->strava_refresh_token);
    if (!refresh) { doStravaUpload(); return; }
    connect(refresh, &QNetworkReply::finished, this, [this, refresh]() {
        refresh->deleteLater();
        if (refresh->error() == QNetworkReply::NoError) {
            Util::parseJsonStravaObject(QString::fromUtf8(refresh->readAll()));
            account->saveStravaCredentials();
        } else {
            LOG_WARN("PostWorkoutPanel",
                     QStringLiteral("Strava token refresh failed: ") + refresh->errorString());
        }
        doStravaUpload();
    });
}

void PostWorkoutPanel::doStravaUpload()
{
    setStravaPostStatus(tr("Uploading to Strava…"), false);
    StravaService svc;
    svc.setAccessToken(account->strava_access_token);
    replyPostStravaUpload = svc.uploadActivity(fitFilePath, fitFileName, fitFileDescription,
                                               /*onTrainer*/ true);
    if (!replyPostStravaUpload) {
        setStravaPostStatus(tr("Upload to Strava (Failed — Retry)"), true);
        return;
    }
    connect(replyPostStravaUpload, &QNetworkReply::finished,
            this, &PostWorkoutPanel::slotPostStravaUploadDone);
}

void PostWorkoutPanel::slotPostStravaUploadDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostStravaUpload = nullptr;

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    LOG_INFO("PostWorkoutPanel",
             QStringLiteral("Strava upload response (HTTP %1): %2")
                 .arg(httpStatus).arg(QString::fromUtf8(body)));

    if (reply->error() != QNetworkReply::NoError) {
        // A 401 means the access token is stale or revoked even though the
        // local expiry bookkeeping thought otherwise. Force a refresh through
        // the token Worker and retry once before surfacing the failure.
        if (httpStatus == 401 && !m_stravaUpload401Retried
            && !account->strava_refresh_token.isEmpty()) {
            m_stravaUpload401Retried = true;
            account->strava_token_expires_at = 0;
            LOG_WARN("PostWorkoutPanel",
                     QStringLiteral("Strava upload got 401 – forcing token refresh and retrying"));
            uploadToStrava();
            return;
        }
        LOG_WARN("PostWorkoutPanel",
                 QStringLiteral("Strava upload failed: ") + reply->errorString());
        setStravaPostStatus(tr("✗ Strava upload failed (HTTP %1): %2")
                                .arg(httpStatus).arg(QString::fromUtf8(body).trimmed()), true);
        return;
    }

    stravaUploadID_post = Util::parseIdJsonStravaUploadObject(QString::fromUtf8(body));

    if (stravaUploadID_post <= 0) {
        LOG_WARN("PostWorkoutPanel",
                 QStringLiteral("Strava upload: no upload id in response — ")
                 + QString::fromUtf8(body));
        setStravaPostStatus(tr("✗ Strava did not accept the upload: %1")
                                .arg(QString::fromUtf8(body).trimmed()), true);
        return;
    }

    setStravaPostStatus(tr("Uploading to Strava…"), false);
    timerPostStravaStatus = new QTimer(this);
    connect(timerPostStravaStatus, &QTimer::timeout, this, &PostWorkoutPanel::slotPostStravaCheckStatus);
    timerPostStravaStatus->start(3000);
}

void PostWorkoutPanel::slotPostStravaCheckStatus()
{
    StravaService svc;
    svc.setAccessToken(account->strava_access_token);
    replyPostStravaStatus = svc.checkUploadStatus(stravaUploadID_post);
    if (replyPostStravaStatus)
        connect(replyPostStravaStatus, &QNetworkReply::finished,
                this, &PostWorkoutPanel::slotPostStravaStatusDone);
}

void PostWorkoutPanel::slotPostStravaStatusDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostStravaStatus = nullptr;

    const QByteArray statusBody = reply->readAll();

    if (reply->error() != QNetworkReply::NoError) {
        if (timerPostStravaStatus) timerPostStravaStatus->stop();
        LOG_WARN("PostWorkoutPanel", QStringLiteral("Strava status check failed: ") + reply->errorString());
        setStravaPostStatus(tr("✗ Strava status check failed: %1").arg(reply->errorString()), true);
        return;
    }

    const int code = Util::parseStravaUploadStatus(QString::fromUtf8(statusBody));
    // 0 = completed, 1 = in progress, 2 = error, -1 = unexpected
    if (code == 1) return;  // keep timer running

    if (timerPostStravaStatus) { timerPostStravaStatus->stop(); timerPostStravaStatus->deleteLater(); timerPostStravaStatus = nullptr; }

    if (code == 0) {
        // Per Strava brand guidelines, deep-link back to the activity with a
        // "View on Strava" link in Strava orange (#FC5200).
        const qint64 activityId = Util::parseStravaActivityId(QString::fromUtf8(statusBody));
        if (activityId > 0) {
            const QString link =
                QStringLiteral("<a style=\"color:#FC5200; text-decoration:underline;\" "
                               "href=\"https://www.strava.com/activities/%1\">%2</a>")
                    .arg(activityId)
                    .arg(tr("View on Strava"));
            // Link on its own line so it never gets clipped on a narrow panel.
            setStravaPostStatus(tr("✓ Uploaded to Strava") + "<br>" + link, false);
        } else {
            setStravaPostStatus(tr("✓ Uploaded to Strava"), false);
        }
        LOG_INFO("PostWorkoutPanel", "Strava upload succeeded");
    } else {
        LOG_WARN("PostWorkoutPanel", QStringLiteral("Strava processing error: ") + QString::fromUtf8(statusBody));
        setStravaPostStatus(tr("✗ Strava could not process the activity: %1")
                                .arg(QString::fromUtf8(statusBody).trimmed()), true);
    }
}

// Updates the post-workout Intervals.icu status — the manual button
// (auto-upload off) or the status label (auto-upload on), whichever is present.
void PostWorkoutPanel::setIntervalsIcuPostStatus(const QString &text, bool retryable)
{
    if (auto *lbl = findChild<QLabel*>("lblIntervalsIcu"))
        lbl->setText(text);
    if (auto *btn = findChild<QPushButton*>("btnIntervalsIcu"))
        btn->setEnabled(retryable);
    adjustSize();
}

void PostWorkoutPanel::uploadToIntervalsIcu()
{
    if (fitFilePath.isEmpty()) return;

    auto *svc = new IntervalsIcuService(this);
    svc->setCredentials(account->intervals_icu_api_key, account->intervals_icu_athlete_id);
    svc->setAccessToken(account->intervals_icu_access_token);
    const QString externalId = QFileInfo(fitFilePath).baseName();
    replyPostIntervalsIcuUpload = svc->uploadActivity(fitFilePath, fitFileName, externalId);
    svc->deleteLater();

    if (!replyPostIntervalsIcuUpload) {
        setIntervalsIcuPostStatus(tr("✗ Intervals.icu upload could not start — check your connection."), true);
        return;
    }
    setIntervalsIcuPostStatus(tr("Uploading to Intervals.icu…"), false);
    connect(replyPostIntervalsIcuUpload, &QNetworkReply::finished,
            this, &PostWorkoutPanel::slotPostIntervalsIcuUploadDone);
}

void PostWorkoutPanel::slotPostIntervalsIcuUploadDone()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    reply->deleteLater();
    replyPostIntervalsIcuUpload = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 409) {
            setIntervalsIcuPostStatus(tr("✓ Activity already on Intervals.icu"), false);
        } else {
            LOG_WARN("PostWorkoutPanel", QStringLiteral("Intervals.icu upload failed: ") + reply->errorString());
            setIntervalsIcuPostStatus(tr("✗ Intervals.icu upload failed: %1").arg(reply->errorString()), true);
        }
        return;
    }

    // HTTP 201 returns the created activity object; deep-link to it like the
    // Strava panel does (link colour ≈ the intervals.icu logo red).
    const QJsonObject activity = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonValue idValue = activity.value(QStringLiteral("id"));
    const QString activityId = idValue.isString()
            ? idValue.toString()
            : (idValue.isDouble() ? QString::number(static_cast<qint64>(idValue.toDouble()))
                                  : QString());
    if (!activityId.isEmpty()) {
        const QString link =
            QStringLiteral("<a style=\"color:#e8485c; text-decoration:underline;\" "
                           "href=\"https://intervals.icu/activities/%1\">%2</a>")
                .arg(activityId)
                .arg(tr("View on Intervals.icu"));
        setIntervalsIcuPostStatus(tr("✓ Uploaded to Intervals.icu") + "<br>" + link, false);
    } else {
        setIntervalsIcuPostStatus(tr("✓ Uploaded to Intervals.icu"), false);
    }
    LOG_INFO("PostWorkoutPanel",
             QStringLiteral("Intervals.icu upload succeeded (activity ") + activityId + QStringLiteral(")"));
}
