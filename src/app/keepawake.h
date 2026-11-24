#ifndef KEEPAWAKE_H
#define KEEPAWAKE_H

#include <QtGlobal>

/// Keeps the display and the computer from sleeping while it is alive.
class KeepAwake
{
public:
    KeepAwake();
    ~KeepAwake();
    Q_DISABLE_COPY_MOVE(KeepAwake)

private:
    quint32 m_inhibitCookie = 0;   // Linux
};

#endif // KEEPAWAKE_H
