#ifndef IDLECURSOR_H
#define IDLECURSOR_H

#include <QElapsedTimer>
#include <QObject>
#include <QPoint>
#include <QTimer>

class QWidget;

/// Hides the mouse cursor over \a window after kIdleMs without movement and
/// shows it again as soon as it moves, or when another window is active.
class IdleCursor : public QObject
{
    Q_OBJECT

public:
    static constexpr int kIdleMs = 10000;

    explicit IdleCursor(QWidget *window, int idleMs = kIdleMs);
    ~IdleCursor() override;

    bool isHidden() const { return m_hidden; }

private:
    void poll();
    void setHidden(bool hidden);

    QWidget *m_window;
    int m_idleMs;
    QTimer m_timer;
    QElapsedTimer m_still;
    QPoint m_lastPos;
    bool m_hidden = false;
};

#endif // IDLECURSOR_H
