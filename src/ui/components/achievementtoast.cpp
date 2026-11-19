#include "achievementtoast.h"

#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSequentialAnimationGroup>
#include <QVBoxLayout>

namespace {
constexpr int kSlideMs = 450;
constexpr int kTopMargin = 48;   // below the workout top bar
}

AchievementToast::AchievementToast(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("achievementToast"));
    setStyleSheet(QStringLiteral(
        "QWidget#achievementToast { background-color: rgba(18,18,22,235); border: 2px solid #f2b830;"
        " border-radius: 14px; }"
        "QLabel { background: transparent; border: none; }"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 12, 22, 12);
    layout->setSpacing(14);

    auto *icon = new QLabel(this);
    icon->setPixmap(QPixmap(QStringLiteral(":/image/icon/trophy"))
                        .scaled(56, 56, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    layout->addWidget(icon);

    auto *text = new QVBoxLayout();
    text->setSpacing(2);
    auto *caption = new QLabel(tr("ACHIEVEMENT UNLOCKED"), this);
    caption->setStyleSheet(QStringLiteral("color: #f2b830; font-size: 9pt; font-weight: bold; letter-spacing: 2px;"));
    m_name = new QLabel(this);
    m_name->setStyleSheet(QStringLiteral("color: white; font-size: 15pt; font-weight: bold;"));
    m_description = new QLabel(this);
    m_description->setStyleSheet(QStringLiteral("color: #c8c8d0; font-size: 10pt;"));
    text->addWidget(caption);
    text->addWidget(m_name);
    text->addWidget(m_description);
    layout->addLayout(text);

    m_opacity = new QGraphicsOpacityEffect(this);
    m_opacity->setOpacity(0.0);
    setGraphicsEffect(m_opacity);
    hide();
}

QPoint AchievementToast::restingPosition() const
{
    const int parentWidth = parentWidget() ? parentWidget()->width() : width();
    return {(parentWidth - width()) / 2, kTopMargin};
}

void AchievementToast::enqueue(const Achievement &achievement)
{
    m_queue.enqueue(achievement);
    if (!m_animation)
        playNext();
}

void AchievementToast::playNext()
{
    if (m_queue.isEmpty())
        return;
    const Achievement achievement = m_queue.dequeue();
    m_name->setText(achievement.getName());
    m_description->setText(achievement.getDescription());
    adjustSize();

    const QPoint rest = restingPosition();
    const QPoint above(rest.x(), -height());
    move(above);
    show();
    raise();
    emit shown(achievement);

    auto slide = [this](const QPoint &from, const QPoint &to, QEasingCurve::Type easing) {
        auto *move = new QPropertyAnimation(this, "pos");
        move->setDuration(kSlideMs);
        move->setStartValue(from);
        move->setEndValue(to);
        move->setEasingCurve(easing);
        return move;
    };
    auto fade = [this](double from, double to) {
        auto *opacity = new QPropertyAnimation(m_opacity, "opacity");
        opacity->setDuration(kSlideMs);
        opacity->setStartValue(from);
        opacity->setEndValue(to);
        return opacity;
    };

    auto *in = new QParallelAnimationGroup;
    in->addAnimation(slide(above, rest, QEasingCurve::OutBack));
    in->addAnimation(fade(0.0, 1.0));
    auto *out = new QParallelAnimationGroup;
    out->addAnimation(slide(rest, above, QEasingCurve::InCubic));
    out->addAnimation(fade(1.0, 0.0));

    m_animation = new QSequentialAnimationGroup(this);
    m_animation->addAnimation(in);
    m_animation->addPause(kHoldMs);
    m_animation->addAnimation(out);
    connect(m_animation, &QAbstractAnimation::finished, this, [this]() {
        hide();
        m_animation->deleteLater();
        m_animation = nullptr;
        playNext();
    });
    m_animation->start();
}
