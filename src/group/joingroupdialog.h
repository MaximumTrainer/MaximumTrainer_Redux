#ifndef JOINGROUPDIALOG_H
#define JOINGROUPDIALOG_H

#include "groupsession.h"
#include "workout.h"

#include <QDialog>
#include <QPointer>

class QLabel;
class QLineEdit;
class QPushButton;

/// Asks for a room code and display name, joins the room and waits for the
/// host's workout. Hands the connected session over with workoutReady().
class JoinGroupDialog : public QDialog
{
    Q_OBJECT

public:
    JoinGroupDialog(const QUrl &relayBaseUrl, const QString &defaultName, QWidget *parent = nullptr);
    ~JoinGroupDialog() override;

signals:
    /// Ownership of the session passes to the receiver.
    void workoutReady(GroupSession *session, const Workout &workout);

private:
    void join();
    void showError(const QString &message);
    void setBusy(bool busy, const QString &status);
    static QString describeRelayError(const QString &errorCode);

    QUrl m_relayBaseUrl;
    QLineEdit *m_codeEdit = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_joinButton = nullptr;
    QPointer<GroupSession> m_session;
};

#endif // JOINGROUPDIALOG_H
