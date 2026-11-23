#include "grouppanel.h"

#include "groupriderdelegate.h"
#include "groupridersmodel.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

using namespace GroupProtocol;

namespace {
constexpr int kPanelWidth = 300;
}

GroupPanel::GroupPanel(GroupSession *session, QWidget *parent)
    : QWidget(parent), m_session(session)
{
    setObjectName(QStringLiteral("groupPanel"));
    setFixedWidth(kPanelWidth);
    m_ridersModel = new GroupRidersModel(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.1);
    m_titleLabel->setFont(titleFont);
    layout->addWidget(m_titleLabel);

    auto *codeRow = new QHBoxLayout();
    m_codeLabel = new QLabel(this);
    QFont codeFont(QStringLiteral("monospace"));
    codeFont.setStyleHint(QFont::Monospace);
    codeFont.setBold(true);
    codeFont.setPointSizeF(titleFont.pointSizeF() * 1.1);
    m_codeLabel->setFont(codeFont);
    m_codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_copyButton = new QPushButton(tr("Copy"), this);
    m_copyButton->setToolTip(tr("Copy the room code to share it with your friends"));
    m_copyButton->setFocusPolicy(Qt::NoFocus);
    codeRow->addWidget(m_codeLabel, 1);
    codeRow->addWidget(m_copyButton);
    layout->addLayout(codeRow);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_riderList = new QListView(this);
    m_riderList->setModel(m_ridersModel);
    m_riderList->setItemDelegate(new GroupRiderDelegate(m_ridersModel, m_riderList));
    m_riderList->setUniformItemSizes(true);
    m_riderList->setFocusPolicy(Qt::NoFocus);
    m_riderList->setSelectionMode(QAbstractItemView::NoSelection);
    m_riderList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_riderList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_riderList->setFrameShape(QFrame::NoFrame);
    m_riderList->viewport()->setAutoFillBackground(false);
    layout->addWidget(m_riderList, 1);
    connect(m_ridersModel, &QAbstractItemModel::modelReset, this, &GroupPanel::refreshStatus);

    m_startButton = new QPushButton(tr("Start group ride"), this);
    m_startButton->setToolTip(tr("Everyone starts together 10 seconds after you press this (or Enter)"));
    m_startButton->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_startButton);

    m_waitingLabel = new QLabel(tr("The host starts the ride for everyone."), this);
    m_waitingLabel->setWordWrap(true);
    layout->addWidget(m_waitingLabel);

    m_chatFeed = new QLabel(this);
    m_chatFeed->setObjectName(QStringLiteral("groupChatFeed"));
    m_chatFeed->setTextFormat(Qt::RichText);
    m_chatFeed->setWordWrap(true);
    m_chatFeed->hide();
    layout->addWidget(m_chatFeed);

    m_chatEdit = new QLineEdit(this);
    m_chatEdit->setObjectName(QStringLiteral("groupChatEdit"));
    m_chatEdit->setPlaceholderText(tr("Message the group (T)"));
    m_chatEdit->setMaxLength(GroupProtocol::kMaxChatLength);
    m_chatEdit->setClearButtonEnabled(true);
    m_chatEdit->installEventFilter(this);
    layout->addWidget(m_chatEdit);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(500);
    connect(m_refreshTimer, &QTimer::timeout, this, [this] {
        m_ridersModel->tick(QDateTime::currentMSecsSinceEpoch());
        refreshStatus();
    });
    m_refreshTimer->start();

    connect(m_copyButton, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_codeLabel->text());
        m_copyButton->setText(tr("Copied"));
        QTimer::singleShot(1500, m_copyButton, [this] { m_copyButton->setText(tr("Copy")); });
    });
    connect(m_startButton, &QPushButton::clicked, this, &GroupPanel::startRequested);

    if (m_session) {
        connect(m_session, &GroupSession::rosterChanged, this, [this](const QList<RosterEntry> &roster) {
            m_ridersModel->setRoster(roster, m_session ? m_session->riderId() : QString());
            refreshHostControls();
            refreshStatus();
        });
        connect(m_session, &GroupSession::telemetryReceived, this, [this](const RiderTelemetry &telemetry) {
            m_ridersModel->updateTelemetry(telemetry, QDateTime::currentMSecsSinceEpoch());
        });
        connect(m_session, &GroupSession::stateChanged, this, &GroupPanel::refreshStatus);
        connect(m_session, &GroupSession::chatReceived, this, &GroupPanel::showChat);
        connect(m_session, &GroupSession::relayError, this, [this](const QString &code) {
            if (code == QLatin1String("chat_too_fast"))
                appendFeedLine(QStringLiteral("<i style='color:#9a9aa6'>%1</i>").arg(tr("One message every 2 seconds.")));
            else if (code == QLatin1String("chat_hourly_limit"))
                appendFeedLine(QStringLiteral("<i style='color:#9a9aa6'>%1</i>").arg(tr("Message limit reached (60 an hour).")));
        });
        m_ridersModel->setRoster(m_session->roster(), m_session->riderId());
    }

    refreshHostControls();
    refreshStatus();
}

void GroupPanel::setScheduledStart(qint64 localStartMs) {
    m_scheduledStartLocalMs = localStartMs;
    refreshHostControls();
    refreshStatus();
}

void GroupPanel::setRideStarted() {
    m_rideStarted = true;
    refreshHostControls();
    refreshStatus();
}

void GroupPanel::setOwnTelemetry(const RiderTelemetry &telemetry) {
    m_ridersModel->updateOwnTelemetry(telemetry, QDateTime::currentMSecsSinceEpoch());
}

void GroupPanel::setRoomCodeForPreview(const QString &roomCode) {
    m_codeLabel->setText(roomCode);
}

void GroupPanel::refreshHostControls() {
    const bool isHost = m_session && m_session->isHost();
    const bool waitingForStart = !m_rideStarted && m_scheduledStartLocalMs == 0;
    m_startButton->setVisible(isHost && waitingForStart);
    m_waitingLabel->setVisible(m_session && !isHost && waitingForStart);
}

void GroupPanel::refreshStatus() {
    const int riderCount = m_ridersModel->rowCount();
    m_titleLabel->setText(riderCount == 1 ? tr("Group ride · 1 rider")
                                          : tr("Group ride · %1 riders").arg(riderCount));
    if (!m_session) {
        m_statusLabel->setText(m_rideStarted ? tr("Riding together.") : QString());
        return;
    }
    m_codeLabel->setText(m_session->roomCode());

    switch (m_session->state()) {
    case GroupSession::State::Reconnecting:
        m_statusLabel->setText(tr("Connection lost, reconnecting… Your workout keeps running."));
        return;
    case GroupSession::State::Closed:
        m_statusLabel->setText(tr("Disconnected from the group."));
        return;
    default:
        break;
    }

    if (m_rideStarted) {
        m_statusLabel->setText(tr("Riding together."));
        return;
    }
    if (m_scheduledStartLocalMs > 0) {
        const qint64 remainingMs = m_scheduledStartLocalMs - QDateTime::currentMSecsSinceEpoch();
        m_statusLabel->setText(tr("Starting in %1 s").arg((qMax<qint64>(0, remainingMs) + 999) / 1000));
        return;
    }
    m_statusLabel->setText(tr("Share the code. Riders appear here when they join."));
}

void GroupPanel::focusChat() {
    m_chatEdit->setFocus(Qt::ShortcutFocusReason);
}

void GroupPanel::showChat(const GroupProtocol::ChatMessage &message) {
    const bool mine = m_session && message.riderId == m_session->riderId();
    const QString name = mine ? tr("You") : message.name;
    m_ridersModel->setChat(message.riderId, message.text, QDateTime::currentMSecsSinceEpoch());
    appendFeedLine(QStringLiteral("<b>%1:</b> %2").arg(name.toHtmlEscaped(), message.text.toHtmlEscaped()));
}

void GroupPanel::appendFeedLine(const QString &html) {
    // The last few lines only: riders glance at it between efforts.
    constexpr int kFeedLines = 3;
    m_feedLines.append(html);
    while (m_feedLines.size() > kFeedLines)
        m_feedLines.removeFirst();
    m_chatFeed->setText(m_feedLines.join(QStringLiteral("<br>")));
    m_chatFeed->show();
}

void GroupPanel::sendChat() {
    const QString text = m_chatEdit->text().trimmed();
    if (!text.isEmpty() && m_session)
        m_session->sendChat(text);   // shown when the relay echoes it back
    leaveChat();
}

void GroupPanel::leaveChat() {
    m_chatEdit->clear();
    // Back to the workout so Space, the arrows and Enter work again.
    if (QWidget *workout = window())
        workout->setFocus(Qt::OtherFocusReason);
}

bool GroupPanel::eventFilter(QObject *watched, QEvent *event) {
    // Enter and Esc must not reach the workout window (pause / close).
    if (watched == m_chatEdit && event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            sendChat();
            return true;
        }
        if (key == Qt::Key_Escape) {
            leaveChat();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
