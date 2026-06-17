#include "zwift_click_test.h"

#include "zwift_click_manager.h"
#include "zwift_click_protocol.h"

#include <QTimer>
#include <QTextStream>

namespace {
QTextStream out(stdout);
void line(const QString &s) { out << s << '\n'; out.flush(); }
constexpr int kGearCount = 24;   // demo gear range for this harness only
} // namespace

ZwiftClickTest::ZwiftClickTest(QObject *parent) : QObject(parent) {}

void ZwiftClickTest::start(const QString &nameFilter, int scanSeconds, int runSeconds,
                           bool singleDevice)
{
    Q_UNUSED(scanSeconds);
    m_runSeconds = runSeconds;

    line(QStringLiteral("── Zwift Click v2 controller test ───────────────────"));
    line(QStringLiteral("  name filter : %1")
             .arg(nameFilter.isEmpty() ? QStringLiteral("(auto: click/play)") : nameFilter));
    line(QStringLiteral("  mode        : %1")
             .arg(singleDevice ? QStringLiteral("SINGLE device (one link — does it report all buttons?)")
                               : QStringLiteral("all devices")));
    line(QStringLiteral("  listening %1 s — wake each controller (press a button);")
             .arg(runSeconds));
    line(QStringLiteral("  then try the paddles, d-pad, and A/B/Y/Z"));
    line(QString());

    m_manager = new ZwiftClickManager(this);

    connect(m_manager, &ZwiftClickManager::deviceConnected, this,
            [](const QString &name, const QString &addr) {
                line(QStringLiteral("  ✓ connected: %1 [%2]").arg(name, addr));
            });
    connect(m_manager, &ZwiftClickManager::deviceDisconnected, this,
            [](const QString &addr) { line(QStringLiteral("  ✗ disconnected: %1").arg(addr)); });

    auto act = [](const QString &s) { line(QStringLiteral("    → %1").arg(s)); };
    connect(m_manager, &ZwiftClickManager::shiftUp, this, [this, act]() {
        m_gear = qBound(1, m_gear + 1, kGearCount);
        act(QStringLiteral("▲ SHIFT UP    gear %1/%2").arg(m_gear).arg(kGearCount));
    });
    connect(m_manager, &ZwiftClickManager::shiftDown, this, [this, act]() {
        m_gear = qBound(1, m_gear - 1, kGearCount);
        act(QStringLiteral("▼ SHIFT DOWN  gear %1/%2").arg(m_gear).arg(kGearCount));
    });
    connect(m_manager, &ZwiftClickManager::difficultyUp,  this, [act]() { act(QStringLiteral("difficulty +1%%  (Y)")); });
    connect(m_manager, &ZwiftClickManager::difficultyDown, this, [act]() { act(QStringLiteral("difficulty -1%%  (B)")); });
    connect(m_manager, &ZwiftClickManager::startPauseWorkout, this, [act]() { act(QStringLiteral("START / PAUSE workout  (A)")); });
    connect(m_manager, &ZwiftClickManager::lap, this, [act]() { act(QStringLiteral("LAP  (Z)")); });
    connect(m_manager, &ZwiftClickManager::radioPrev, this, [act]() { act(QStringLiteral("radio ◀ previous station  (d-pad left)")); });
    connect(m_manager, &ZwiftClickManager::radioNext, this, [act]() { act(QStringLiteral("radio ▶ next station  (d-pad right)")); });
    connect(m_manager, &ZwiftClickManager::radioVolumeUp,   this, [act]() { act(QStringLiteral("radio volume +  (d-pad up)")); });
    connect(m_manager, &ZwiftClickManager::radioVolumeDown, this, [act]() { act(QStringLiteral("radio volume -  (d-pad down)")); });
    connect(m_manager, &ZwiftClickManager::unmappedButton, this,
            [act](int bit) { act(QStringLiteral("(unmapped bit %1)").arg(bit)); });

    m_manager->start(nameFilter, singleDevice);
    line(QStringLiteral("Gear starts at %1/%2").arg(m_gear).arg(kGearCount));

    m_runTimer = new QTimer(this);
    m_runTimer->setSingleShot(true);
    connect(m_runTimer, &QTimer::timeout, this, [this]() {
        line(QString());
        line(QStringLiteral("── test complete — releasing controllers ──"));
        m_manager->stop();
        emit finished();
    });
    m_runTimer->start(m_runSeconds * 1000);
}
