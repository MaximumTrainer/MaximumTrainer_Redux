#include "studioconnectionboard.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

namespace {
constexpr int kColumns = 4;
const QString kGrey   = QStringLiteral("#9a9aa6");
const QString kGreen  = QStringLiteral("#3fb27f");
const QString kYellow = QStringLiteral("#f2c230");
const QString kRed    = QStringLiteral("#e5534b");

bool needsRetry(SensorStatusStrip::State state)
{
    return state == SensorStatusStrip::State::NotFound || state == SensorStatusStrip::State::Lost;
}
} // namespace

StudioConnectionBoard::StudioConnectionBoard(QWidget *parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("studioConnectionBoard"));
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral(
        "QWidget#studioConnectionBoard { background-color: #24242a; }"
        "QFrame#boardCard { background-color: #1a1a1f; border: 1px solid #3a3a44; border-radius: 10px; }"
        "QFrame#boardCard[trouble=\"true\"] { border-color: %1; }"
        "QLabel { background: transparent; border: none; }").arg(kRed));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(8);
    auto *header = new QHBoxLayout;
    auto *title = new QLabel(tr("Connecting sensors"), this);
    title->setStyleSheet(QStringLiteral("font: 700 12pt 'Inter'; color: #e6e6ea;"));
    m_count = new QLabel(this);
    m_count->setStyleSheet(QStringLiteral("font: 10pt 'Inter'; color: %1;").arg(kGrey));
    auto *hint = new QLabel(tr("Folds away when everyone is connected or the workout starts"), this);
    hint->setStyleSheet(QStringLiteral("font: 9pt 'Inter'; color: %1;").arg(kGrey));
    header->addWidget(title);
    header->addSpacing(10);
    header->addWidget(m_count);
    header->addStretch();
    header->addWidget(hint);
    layout->addLayout(header);
    m_grid = new QGridLayout;
    m_grid->setSpacing(8);
    for (int column = 0; column < kColumns; ++column)
        m_grid->setColumnStretch(column, 1);
    layout->addLayout(m_grid);
    layout->addStretch();
}

void StudioConnectionBoard::addRider(int rider, const QString &name)
{
    auto *card = new QFrame(this);
    card->setObjectName(QStringLiteral("boardCard"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 8, 12, 8);
    cardLayout->setSpacing(4);
    auto *header = new QHBoxLayout;
    auto *nameLabel = new QLabel(name, card);
    nameLabel->setStyleSheet(QStringLiteral("font: 700 10.5pt 'Inter'; color: #e6e6ea;"));
    auto *ready = new QLabel(card);
    ready->setStyleSheet(QStringLiteral("font: 600 9pt 'Inter'; color: %1;").arg(kGreen));
    header->addWidget(nameLabel);
    header->addStretch();
    header->addWidget(ready);
    cardLayout->addLayout(header);
    const int index = int(m_cards.size());
    m_grid->addWidget(card, index / kColumns, index % kColumns);
    m_cards.insert(rider, card);
    m_ready.insert(rider, ready);
}

void StudioConnectionBoard::addSensor(int rider, int key, const QString &role, const QString &device)
{
    QFrame *card = m_cards.value(rider, nullptr);
    if (!card)
        return;
    auto *row = new QHBoxLayout;
    row->setSpacing(6);
    Row widgets;
    widgets.dot = new QLabel(QStringLiteral("●"), card);
    auto *label = new QLabel(device.isEmpty() ? role : QStringLiteral("%1: %2").arg(role, device), card);
    label->setStyleSheet(QStringLiteral("font: 9pt 'Inter'; color: #c8c8d0;"));
    widgets.status = new QLabel(card);
    widgets.status->installEventFilter(this);
    widgets.dot->installEventFilter(this);
    row->addWidget(widgets.dot);
    row->addWidget(label, 1);
    row->addWidget(widgets.status);
    static_cast<QVBoxLayout*>(card->layout())->addLayout(row);
    m_rows.insert({rider, key}, widgets);
    m_states.insert({rider, key}, SensorStatusStrip::State::Searching);
    setState(rider, key, SensorStatusStrip::State::Searching);
}

void StudioConnectionBoard::setState(int rider, int key, SensorStatusStrip::State state)
{
    const Key id{rider, key};
    if (!m_rows.contains(id))
        return;
    m_states.insert(id, state);
    const Row &row = m_rows[id];
    QString color = kGrey;
    switch (state) {
    case SensorStatusStrip::State::Connecting: color = kYellow; break;
    case SensorStatusStrip::State::Connected:  color = kGreen; break;
    case SensorStatusStrip::State::NotFound:
    case SensorStatusStrip::State::Lost:       color = kRed; break;
    case SensorStatusStrip::State::Searching:
    case SensorStatusStrip::State::Disconnected: break;
    }
    row.dot->setStyleSheet(QStringLiteral("font-size: 9pt; color: %1;").arg(color));
    row.status->setText(statusText(rider, key));
    row.status->setStyleSheet(QStringLiteral("font: 600 9pt 'Inter'; color: %1;")
                                  .arg(state == SensorStatusStrip::State::Connected ? kGrey : color));
    row.status->setCursor(needsRetry(state) ? Qt::PointingHandCursor : Qt::ArrowCursor);
    refreshCard(rider);
    refreshHeader();
}

QString StudioConnectionBoard::statusText(int rider, int key) const
{
    switch (m_states.value({rider, key})) {
    case SensorStatusStrip::State::Searching:  return tr("Searching…");
    case SensorStatusStrip::State::Connecting: return tr("Connecting…");
    case SensorStatusStrip::State::Connected:  return tr("Connected");
    case SensorStatusStrip::State::NotFound:   return tr("Not found - click to retry");
    case SensorStatusStrip::State::Lost:       return tr("Lost - click to retry");
    case SensorStatusStrip::State::Disconnected: return tr("Disconnected");
    }
    return {};
}

int StudioConnectionBoard::connectedCount() const
{
    return int(std::count(m_states.cbegin(), m_states.cend(), SensorStatusStrip::State::Connected));
}

void StudioConnectionBoard::refreshCard(int rider)
{
    bool ready = true;
    bool trouble = false;
    for (auto it = m_states.cbegin(); it != m_states.cend(); ++it) {
        if (it.key().first != rider)
            continue;
        ready = ready && it.value() == SensorStatusStrip::State::Connected;
        trouble = trouble || needsRetry(it.value());
    }
    m_ready.value(rider)->setText(ready ? tr("Ready") : QString());
    QFrame *card = m_cards.value(rider);
    if (card->property("trouble").toBool() != trouble) {
        card->setProperty("trouble", trouble);
        card->style()->unpolish(card);
        card->style()->polish(card);
    }
}

void StudioConnectionBoard::refreshHeader()
{
    m_count->setText(tr("%1 of %2 connected · %3 riders").arg(connectedCount()).arg(sensorCount()).arg(m_cards.size()));
    const bool all = allConnected();
    if (all != m_wasAllConnected) {
        m_wasAllConnected = all;
        emit allConnectedChanged(all);
    }
}

bool StudioConnectionBoard::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        for (auto it = m_rows.cbegin(); it != m_rows.cend(); ++it) {
            if ((watched == it.value().status || watched == it.value().dot) && needsRetry(m_states.value(it.key()))) {
                emit retryRequested(it.key().first, it.key().second);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
