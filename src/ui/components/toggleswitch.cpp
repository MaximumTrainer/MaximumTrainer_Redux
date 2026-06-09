#include "toggleswitch.h"

#include <QPainter>
#include <QPropertyAnimation>
#include <QtMath>

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);

    m_animation = new QPropertyAnimation(this, "knobPosition", this);
    m_animation->setDuration(140);

    // Slide the knob whenever the checked state changes (click or programmatic).
    connect(this, &QAbstractButton::toggled, this, [this](bool checked) {
        m_animation->stop();
        m_animation->setStartValue(m_knobPosition);
        m_animation->setEndValue(checked ? 1.0 : 0.0);
        m_animation->start();
    });
}

QSize ToggleSwitch::sizeHint() const
{
    return QSize(52, 28);
}

void ToggleSwitch::setKnobPosition(qreal pos)
{
    m_knobPosition = pos;
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const qreal h = height();
    const qreal w = width();
    const qreal radius = h / 2.0;
    const qreal margin = 2.0;
    const qreal knobDiameter = h - 2 * margin;

    // Track colour interpolates grey (off) → green (on) by knob position.
    const QColor offColor(120, 120, 120);
    const QColor onColor(89, 171, 121);   // app accent green
    QColor track(
        offColor.red()   + (onColor.red()   - offColor.red())   * m_knobPosition,
        offColor.green() + (onColor.green() - offColor.green()) * m_knobPosition,
        offColor.blue()  + (onColor.blue()  - offColor.blue())  * m_knobPosition);
    if (!isEnabled())
        track = offColor.darker(150);

    p.setPen(Qt::NoPen);
    p.setBrush(track);
    p.drawRoundedRect(QRectF(0, 0, w, h), radius, radius);

    // Knob slides between left and right ends.
    const qreal knobX = margin + m_knobPosition * (w - knobDiameter - 2 * margin);
    p.setBrush(Qt::white);
    p.drawEllipse(QRectF(knobX, margin, knobDiameter, knobDiameter));
}
