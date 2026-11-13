#include "joingroupdialog.h"

#include "groupworkoutcodec.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

using namespace GroupProtocol;

JoinGroupDialog::JoinGroupDialog(const QUrl &relayBaseUrl, const QString &defaultName, QWidget *parent)
    : QDialog(parent), m_relayBaseUrl(relayBaseUrl)
{
    setWindowTitle(tr("Join Group Workout"));
    setAttribute(Qt::WA_DeleteOnClose);

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Enter the room code the host shared with you."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *form = new QFormLayout();
    m_codeEdit = new QLineEdit(this);
    m_codeEdit->setPlaceholderText(QStringLiteral("GRP-7KQ2"));
    m_codeEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9-]{0,8}")), m_codeEdit));
    m_nameEdit = new QLineEdit(defaultName, this);
    m_nameEdit->setMaxLength(24);
    form->addRow(tr("Room code"), m_codeEdit);
    form->addRow(tr("Your name"), m_nameEdit);
    layout->addLayout(form);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_joinButton = buttons->addButton(tr("Join"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_joinButton, &QPushButton::clicked, this, &JoinGroupDialog::join);
    connect(m_codeEdit, &QLineEdit::returnPressed, this, &JoinGroupDialog::join);
    m_codeEdit->setFocus();
}

JoinGroupDialog::~JoinGroupDialog() {
    // Still ours only if the join never completed.
    if (m_session) {
        m_session->leave();
        m_session->deleteLater();
    }
}

void JoinGroupDialog::join() {
    const QString code = normalizeRoomCode(m_codeEdit->text());
    if (code.isEmpty()) {
        showError(tr("That doesn't look like a room code. It has 4 letters or digits, like GRP-7KQ2."));
        return;
    }
    const QString name = m_nameEdit->text().trimmed();
    if (name.isEmpty()) {
        showError(tr("Enter the name the other riders will see."));
        return;
    }

    if (m_session) m_session->deleteLater();
    m_session = new GroupSession(m_relayBaseUrl, this);
    setBusy(true, tr("Connecting…"));

    connect(m_session, &GroupSession::joined, this, [this] {
        setBusy(true, tr("Joined. Waiting for the host's workout…"));
    });
    connect(m_session, &GroupSession::relayError, this, [this](const QString &errorCode) {
        showError(describeRelayError(errorCode));
    });
    connect(m_session, &GroupSession::workoutReceived, this, [this](const SharedWorkout &sharedWorkout) {
        const std::optional<Workout> workout = GroupWorkoutCodec::decode(sharedWorkout);
        if (!workout) {
            showError(tr("The host's workout could not be read."));
            return;
        }
        GroupSession *session = m_session;
        session->setParent(nullptr);
        session->disconnect(this);
        m_session = nullptr;
        emit workoutReady(session, *workout);
        accept();
    });

    m_session->joinRoom(code, name);
}

void JoinGroupDialog::showError(const QString &message) {
    if (m_session) {
        m_session->disconnect(this);
        m_session->leave();
        m_session->deleteLater();
        m_session = nullptr;
    }
    setBusy(false, message);
}

void JoinGroupDialog::setBusy(bool busy, const QString &status) {
    m_statusLabel->setText(status);
    m_joinButton->setEnabled(!busy);
    m_codeEdit->setEnabled(!busy);
    m_nameEdit->setEnabled(!busy);
}

QString JoinGroupDialog::describeRelayError(const QString &errorCode) {
    if (errorCode == QLatin1String("room_not_found"))
        return tr("No room with that code. Check the code with the host.");
    if (errorCode == QLatin1String("room_full"))
        return tr("That room is full (%1 riders).").arg(kMaxRiders);
    if (errorCode == QLatin1String("protocol_mismatch"))
        return tr("The group server needs a newer MaximumTrainer. Please update.");
    if (errorCode == QLatin1String("relay_unreachable"))
        return tr("Could not reach the group server. Check your internet connection.");
    return tr("The group server refused to join (%1).").arg(errorCode);
}
