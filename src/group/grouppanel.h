#ifndef GROUPPANEL_H
#define GROUPPANEL_H

#include "groupprotocol.h"
#include "groupsession.h"

#include <QPointer>
#include <QWidget>

class GroupRidersModel;
class QLabel;
class QLineEdit;
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
    /// Puts the cursor in the chat box (the T key in the workout window).
    void focusChat();
    /// Adds a line to the chat feed and the sender's row (also for previews).
    void showChat(const GroupProtocol::ChatMessage &message);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void startRequested();

private:
    void refreshHostControls();
    void refreshStatus();
    void sendChat();
    void leaveChat();
    void appendFeedLine(const QString &html);

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
    QLabel *m_chatFeed = nullptr;
    QLineEdit *m_chatEdit = nullptr;
    QStringList m_feedLines;
    qint64 m_scheduledStartLocalMs = 0;
    bool m_rideStarted = false;
};

#endif // GROUPPANEL_H
