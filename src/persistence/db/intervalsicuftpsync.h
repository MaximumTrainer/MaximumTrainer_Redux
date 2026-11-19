#ifndef INTERVALSICUFTPSYNC_H
#define INTERVALSICUFTPSYNC_H

#include <QObject>
#include <QString>

class QNetworkReply;

/// Saves a new FTP to the athlete's Intervals.icu Ride settings. Login copies
/// the FTP from Intervals.icu, so a local-only change would be lost. Reads the
/// settings and writes the whole object back with only the FTP field in use
/// (indoor_ftp when set, else ftp) changed, so zones and other fields are kept.
/// Deletes itself when done.
class IntervalsIcuFtpSync : public QObject
{
    Q_OBJECT
public:
    /// \a lthrBpm > 0 also updates the Ride LTHR (FTP tests measure it).
    IntervalsIcuFtpSync(const QString &athleteId, const QString &bearerToken, int ftpWatts,
                        QObject *parent = nullptr, int lthrBpm = 0);
    void start();

signals:
    void finished(bool ok, const QString &error);

private:
    void onSettingsRead();
    void onSettingsWritten();
    void finish(bool ok, const QString &error);

    QString        m_athleteId;
    QString        m_token;
    int            m_ftpWatts;
    int            m_lthrBpm = 0;
    QNetworkReply *m_reply = nullptr;
};

#endif // INTERVALSICUFTPSYNC_H
