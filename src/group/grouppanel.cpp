#include "grouppanel.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

using namespace GroupProtocol;

namespace {
constexpr int kPanelWidth = 220;
}

GroupPanel::GroupPanel(GroupSession *session, QWidget *parent)
    : QWidget(parent), m_session(session)
{
    setObjectName(QStringLiteral("groupPanel"));
    setFixedWidth(kPanelWidth);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel(tr("Group ride"), this);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.15);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *codeRow = new QHBoxLayout();
    m_codeLabel = new QLabel(this);
    QFont codeFont(QStringLiteral("monospace"));
    codeFont.setStyleHint(QFont::Monospace);
    codeFont.setBold(true);
    codeFont.setPointSizeF(titleFont.pointSizeF() * 1.2);
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

    m_riderList = new QListWidget(this);
    m_riderList->setFocusPolicy(Qt::NoFocus);
    m_riderList->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(m_riderList, 1);

    m_startButton = new QPushButton(tr("Start group ride"), this);
    m_startButton->setToolTip(tr("Everyone starts together 10 seconds after you press this"));
    m_startButton->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_startButton);

    m_waitingLabel = new QLabel(tr("The host starts the ride for everyone."), this);
    m_waitingLabel->setWordWrap(true);
    layout->addWidget(m_waitingLabel);

    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(250);
    connect(m_countdownTimer, &QTimer::timeout, this, &GroupPanel::refreshStatus);

    connect(m_copyButton, &QPushButton::clicked, this, [this] {
        if (m_session) QApplication::clipboard()->setText(m_session->roomCode());
        m_copyButton->setText(tr("Copied"));
        QTimer::singleShot(1500, m_copyButton, [this] { m_copyButton->setText(tr("Copy")); });
    });
    connect(m_startButton, &QPushButton::clicked, this, &GroupPanel::startRequested);

    if (m_session) {
        connect(m_session, &GroupSession::rosterChanged, this, [this] {
            refreshRoster();
            refreshHostControls();
        });
        connect(m_session, &GroupSession::stateChanged, this, &GroupPanel::refreshStatus);
    }

    refreshRoster();
    refreshHostControls();
    refreshStatus();
}

void GroupPanel::setScheduledStart(qint64 localStartMs) {
    m_scheduledStartLocalMs = localStartMs;
    m_countdownTimer->start();
    refreshHostControls();
    refreshStatus();
}

void GroupPanel::setRideStarted() {
    m_rideStarted = true;
    m_countdownTimer->stop();
    refreshHostControls();
    refreshStatus();
}

void GroupPanel::refreshRoster() {
    m_riderList->clear();
    if (!m_session) return;
    for (const RosterEntry &rider : m_session->roster()) {
        QString text = rider.name;
        if (rider.riderId == m_session->riderId()) text += tr(" (you)");
        if (rider.isHost) text += tr(" · host");
        text += QStringLiteral("\n") + (rider.isReady ? tr("Ready") : tr("Getting ready…"));
        auto *item = new QListWidgetItem(text, m_riderList);
        item->setForeground(rider.isReady ? palette().color(QPalette::WindowText)
                                          : palette().color(QPalette::PlaceholderText));
    }
}

void GroupPanel::refreshHostControls() {
    const bool isHost = m_session && m_session->isHost();
    const bool waitingForStart = !m_rideStarted && m_scheduledStartLocalMs == 0;
    m_startButton->setVisible(isHost && waitingForStart);
    m_waitingLabel->setVisible(!isHost && waitingForStart);
}

void GroupPanel::refreshStatus() {
    if (!m_session) {
        m_statusLabel->setText(tr("Disconnected from the group."));
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
        const int remainingSeconds = int((qMax<qint64>(0, remainingMs) + 999) / 1000);
        m_statusLabel->setText(tr("Starting in %1 s").arg(remainingSeconds));
        return;
    }
    m_statusLabel->setText(tr("Share the code. Riders appear here when they join."));
}
