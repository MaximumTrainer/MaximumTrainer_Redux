#ifndef GROUPPANEL_H
#define GROUPPANEL_H

#include "groupprotocol.h"
#include "groupsession.h"

#include <QPointer>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

/// Right-hand column of the workout window during a group workout: room code,
/// riders and, for the host, the button that starts everyone together.
class GroupPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GroupPanel(GroupSession *session, QWidget *parent = nullptr);

    /// Local epoch ms of the shared start, or 0 while waiting for the host.
    void setScheduledStart(qint64 localStartMs);
    void setRideStarted();

signals:
    void startRequested();

private:
    void refreshRoster();
    void refreshStatus();
    void refreshHostControls();

    QPointer<GroupSession> m_session;
    QLabel *m_codeLabel = nullptr;
    QPushButton *m_copyButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QListWidget *m_riderList = nullptr;
    QPushButton *m_startButton = nullptr;
    QLabel *m_waitingLabel = nullptr;
    QTimer *m_countdownTimer = nullptr;
    qint64 m_scheduledStartLocalMs = 0;
    bool m_rideStarted = false;
};

#endif // GROUPPANEL_H
