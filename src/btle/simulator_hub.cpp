#include "simulator_hub.h"
#include <QtGlobal>    // qrand / QRandomGenerator
#include <QRandomGenerator>

SimulatorHub::SimulatorHub(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &SimulatorHub::tick);
}

void SimulatorHub::start()
{
    m_timer->start();
}

void SimulatorHub::stop()
{
    m_timer->stop();
}

// ──────────────────────────────────────────────────────────────────────────────
// Tick – advance each channel by a small random delta, keep within bounds
// ──────────────────────────────────────────────────────────────────────────────
static double clamp(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void SimulatorHub::tick()
{
    auto &rng = *QRandomGenerator::global();

    // Helper: random delta 0..maxDelta, applied in current direction
    auto drift = [&](double &val, int &dir, double base,
                     double lo, double hi, double maxDelta) {
        double delta = rng.bounded(maxDelta);
        val += dir * delta;
        val = clamp(val, lo, hi);
        // Flip direction when close to limits or randomly ~10% of the time
        if (val <= lo + 1.0 || val >= hi - 1.0 || rng.bounded(10) == 0)
            dir = -dir;
    };

    drift(m_hr,      m_hrDir,      140.0, 125.0, 165.0, 3.0);
    drift(m_cadence, m_cadenceDir,  90.0,  80.0, 100.0, 2.0);
    drift(m_speed,   m_speedDir,    28.0,  23.0,  33.0, 1.0);
    drift(m_power,   m_powerDir,   200.0, 170.0, 260.0, 5.0);
    drift(m_smo2,    m_smo2Dir,     65.0,  50.0,  80.0, 1.0);
    drift(m_thb,     m_thbDir,      13.0,  11.0,  15.0, 0.2);
    drift(m_balance, m_balanceDir,  48.0,  44.0,  54.0, 1.0);
    drift(m_torqueEffL, m_torqueEffLDir, 82.0, 72.0, 92.0, 2.0);
    drift(m_torqueEffR, m_torqueEffRDir, 79.0, 70.0, 90.0, 2.0);
    drift(m_smoothL,    m_smoothLDir,    24.0, 18.0, 30.0, 1.0);
    drift(m_smoothR,    m_smoothRDir,    23.0, 18.0, 30.0, 1.0);

    emit signal_hr(m_userID,      static_cast<int>(m_hr));
    emit signal_cadence(m_userID, static_cast<int>(m_cadence));
    emit signal_speed(m_userID,   m_speed);
    emit signal_power(m_userID,   static_cast<int>(m_power));
    emit signal_balance(m_userID, static_cast<int>(m_balance));
    // combinedPedalSmooth = -1 → widget shows left/right smoothness separately
    emit signal_pedal(m_userID, qRound(m_torqueEffL), qRound(m_torqueEffR),
                      qRound(m_smoothL), qRound(m_smoothR), -1.0);
    emit signal_oxygen(m_userID,  m_smo2, m_thb);
}

// ──────────────────────────────────────────────────────────────────────────────
// Slots – accept trainer commands; adjust power target to make simulation react
// ──────────────────────────────────────────────────────────────────────────────
void SimulatorHub::setLoad(int /*antID*/, double watts)
{
    // Nudge simulated power toward the requested load
    m_power = clamp(watts, 100.0, 400.0);
}

void SimulatorHub::setSlope(int /*antID*/, double grade)
{
    // Simulate power increase with positive grade
    double targetPower = 200.0 + grade * 15.0;
    m_power = clamp(targetPower, 100.0, 400.0);
}

void SimulatorHub::stopDecodingMsg()
{
    stop();
}
