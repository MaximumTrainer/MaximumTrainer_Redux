#include "metriccard.h"

#include <QEvent>
#include <QLabel>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

MetricCard::MetricCard(QWidget *parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("metricCard"));
    setProperty("targetState", QStringLiteral("none"));
    setStyleSheet(QStringLiteral(
        "QFrame#metricCard { background-color: #26262d; border: 1px solid #34343d; border-radius: 12px; }"
        "QFrame#metricCard[targetState=\"below\"] { border-left: 4px solid #e5534b; }"
        "QFrame#metricCard[targetState=\"above\"] { border-left: 4px solid #f2a33a; }"));
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(8, 6, 8, 6);
    m_layout->setSpacing(4);
}

void MetricCard::addPart(QWidget *part, int stretch, bool follower)
{
    m_layout->addWidget(part, stretch);
    if (follower) {
        m_followers.append(part);
    } else {
        m_parts.append(part);
        part->installEventFilter(this);
    }
    syncVisibility();
}

void MetricCard::setCaption(const QString &text)
{
    auto *caption = new QLabel(text, this);
    caption->setStyleSheet(QStringLiteral("font: 600 8pt 'Inter'; color: #9a9aa6; letter-spacing: 1px;"
                                          " border: none; background: transparent;"));
    m_layout->insertWidget(0, caption);
}

void MetricCard::setTargetState(int state)
{
    const QString value = state == -1 ? QStringLiteral("below")
                        : state == 1  ? QStringLiteral("above")
                        : state == 0  ? QStringLiteral("ok")
                                      : QStringLiteral("none");
    if (property("targetState").toString() == value)
        return;
    setProperty("targetState", value);
    style()->unpolish(this);
    style()->polish(this);
}

bool MetricCard::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::ShowToParent || event->type() == QEvent::HideToParent)
        QTimer::singleShot(0, this, &MetricCard::syncVisibility);
    return QFrame::eventFilter(watched, event);
}

void MetricCard::syncVisibility()
{
    bool anyVisible = false;
    for (QWidget *part : m_parts)
        anyVisible = anyVisible || !part->isHidden();
    for (QWidget *follower : m_followers)
        follower->setVisible(anyVisible);
    setVisible(anyVisible);
}
