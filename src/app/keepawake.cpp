#include "keepawake.h"

#ifdef Q_OS_WIN32
#   include <windows.h>
#elif defined(MT_HAS_DBUS)
#   include <QDBusConnection>
#   include <QDBusMessage>
#   include <QDBusReply>
#   include <QDebug>

namespace {
QDBusMessage screenSaverCall(const QString &method)
{
    return QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.ScreenSaver"),
                                          QStringLiteral("/org/freedesktop/ScreenSaver"),
                                          QStringLiteral("org.freedesktop.ScreenSaver"), method);
}
constexpr int kReplyTimeoutMs = 1000;
}
#endif

KeepAwake::KeepAwake()
{
#ifdef Q_OS_WIN32
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
#elif defined(MT_HAS_DBUS)
    QDBusMessage inhibit = screenSaverCall(QStringLiteral("Inhibit"));
    inhibit << QStringLiteral("MaximumTrainer") << QStringLiteral("Workout in progress");
    const QDBusReply<quint32> reply = QDBusConnection::sessionBus().call(inhibit, QDBus::Block, kReplyTimeoutMs);
    if (reply.isValid())
        m_inhibitCookie = reply.value();
    else
        qWarning() << "KeepAwake: cannot inhibit sleep:" << reply.error().message();
#endif
}

KeepAwake::~KeepAwake()
{
#ifdef Q_OS_WIN32
    SetThreadExecutionState(ES_CONTINUOUS);
#elif defined(MT_HAS_DBUS)
    if (m_inhibitCookie != 0) {
        QDBusMessage release = screenSaverCall(QStringLiteral("UnInhibit"));
        release << m_inhibitCookie;
        QDBusConnection::sessionBus().call(release, QDBus::NoBlock);
    }
#endif
}
