#include "idlecursor.h"

#include <QApplication>
#include <QCursor>
#include <QWidget>

namespace {
constexpr int kPollMs = 100;
}

// The position is polled rather than watched through events: the embedded web
// view swallows mouse events, and Windows sends moves when nothing moved.
IdleCursor::IdleCursor(QWidget *window, int idleMs)
    : QObject(window), m_window(window), m_idleMs(idleMs), m_lastPos(QCursor::pos())
{
    m_still.start();
    connect(&m_timer, &QTimer::timeout, this, &IdleCursor::poll);
    m_timer.start(kPollMs);
}

IdleCursor::~IdleCursor()
{
    setHidden(false);
}

void IdleCursor::poll()
{
    const QPoint pos = QCursor::pos();
    const bool busy = pos != m_lastPos || !m_window->isActiveWindow() || QApplication::activePopupWidget()
        || QApplication::mouseButtons() != Qt::NoButton || !m_window->frameGeometry().contains(pos);
    m_lastPos = pos;
    if (busy)
        m_still.restart();
    setHidden(!busy && m_still.elapsed() >= m_idleMs);
}

void IdleCursor::setHidden(bool hidden)
{
    if (hidden == m_hidden)
        return;
    m_hidden = hidden;
    if (hidden)
        QApplication::setOverrideCursor(Qt::BlankCursor);
    else
        QApplication::restoreOverrideCursor();
}
