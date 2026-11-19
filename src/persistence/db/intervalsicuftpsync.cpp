#include "intervalsicuftpsync.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

#include "intervalsicudao.h"
#include "logger.h"

IntervalsIcuFtpSync::IntervalsIcuFtpSync(const QString &athleteId, const QString &bearerToken,
                                         int ftpWatts, QObject *parent, int lthrBpm)
    : QObject(parent), m_athleteId(athleteId), m_token(bearerToken), m_ftpWatts(ftpWatts),
      m_lthrBpm(lthrBpm)
{
}

void IntervalsIcuFtpSync::start()
{
    m_reply = IntervalsIcuDAO::getRideSettingsBearer(m_athleteId, m_token);
    if (!m_reply) {
        finish(false, tr("network unavailable"));
        return;
    }
    connect(m_reply, &QNetworkReply::finished, this, &IntervalsIcuFtpSync::onSettingsRead);
}

void IntervalsIcuFtpSync::onSettingsRead()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        finish(false, reply->errorString());
        return;
    }
    QJsonObject settings = QJsonDocument::fromJson(reply->readAll()).object();
    if (settings.isEmpty()) {
        finish(false, tr("no cycling settings found"));
        return;
    }
    // Update the FTP field the athlete uses (login reads indoor_ftp first),
    // rather than creating a separate indoor FTP.
    const QString field = settings.value(QStringLiteral("indoor_ftp")).toInt() > 0
                              ? QStringLiteral("indoor_ftp") : QStringLiteral("ftp");
    settings.insert(field, m_ftpWatts);
    if (m_lthrBpm > 0)
        settings.insert(QStringLiteral("lthr"), m_lthrBpm);
    m_reply = IntervalsIcuDAO::putRideSettingsBearer(m_athleteId, m_token, settings);
    if (!m_reply) {
        finish(false, tr("network unavailable"));
        return;
    }
    connect(m_reply, &QNetworkReply::finished, this, &IntervalsIcuFtpSync::onSettingsWritten);
}

void IntervalsIcuFtpSync::onSettingsWritten()
{
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        finish(false, reply->errorString());
        return;
    }
    LOG_INFO("IntervalsIcuFtpSync", QStringLiteral("FTP %1 W saved to Intervals.icu").arg(m_ftpWatts));
    finish(true, QString());
}

void IntervalsIcuFtpSync::finish(bool ok, const QString &error)
{
    if (!ok)
        LOG_WARN("IntervalsIcuFtpSync", QStringLiteral("saving FTP to Intervals.icu failed: ") + error);
    emit finished(ok, error);
    deleteLater();
}
