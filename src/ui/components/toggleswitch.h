#ifndef TOGGLESWITCH_H
#define TOGGLESWITCH_H

#include <QAbstractButton>

class QPropertyAnimation;

/*
 * ToggleSwitch
 *
 * A compact iOS-style on/off switch. Qt has no native switch widget, so this is
 * a small QAbstractButton subclass that paints a rounded track and a sliding
 * knob, animating between states. It is checkable and emits toggled(bool) like
 * a QCheckBox, so it is a drop-in for one. Used for the prominent "Studio Mode"
 * toggle on the Studio page.
 */
class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT
    Q_PROPERTY(qreal knobPosition READ knobPosition WRITE setKnobPosition)

public:
    explicit ToggleSwitch(QWidget *parent = nullptr);

    QSize sizeHint() const override;

    qreal knobPosition() const { return m_knobPosition; }
    void  setKnobPosition(qreal pos);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_knobPosition = 0.0;   // 0 = off (left), 1 = on (right)
    QPropertyAnimation *m_animation = nullptr;
};

#endif // TOGGLESWITCH_H
