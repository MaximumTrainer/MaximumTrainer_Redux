#ifndef GROUPPANEL_H
#define GROUPPANEL_H

#include "groupprotocol.h"
#include "groupsession.h"

#include <QPointer>
#include <QWidget>

class GroupRidersModel;
class QLabel;
class QListView;
class QPushButton;
class QTimer;

/// Right-hand column of the workout window during a group workout: room code,
/// riders with their live numbers and, for the host, the shared Start button.
class GroupPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GroupPanel(GroupSession *session, QWidget *parent = nullptr);

    GroupRidersModel *ridersModel() const { return m_ridersModel; }

    /// Local epoch ms of the shared start, or 0 while waiting for the host.
    void setScheduledStart(qint64 localStartMs);
    void setRideStarted();
    void setOwnTelemetry(const GroupProtocol::RiderTelemetry &telemetry);
    /// For screenshots/tests without a relay connection.
    void setRoomCodeForPreview(const QString &roomCode);

signals:
    void startRequested();

private:
    void refreshHostControls();
    void refreshStatus();

    QPointer<GroupSession> m_session;
    GroupRidersModel *m_ridersModel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_codeLabel = nullptr;
    QPushButton *m_copyButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QListView *m_riderList = nullptr;
    QPushButton *m_startButton = nullptr;
    QLabel *m_waitingLabel = nullptr;
    QTimer *m_refreshTimer = nullptr;
    qint64 m_scheduledStartLocalMs = 0;
    bool m_rideStarted = false;
};

#endif // GROUPPANEL_H
