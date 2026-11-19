#ifndef TARGETALERT_H
#define TARGETALERT_H

#include <QtGlobal>

/// Decides when to play the "too low" / "too high" sound for one metric
/// (power or cadence): a value outside target +/- range triggers an alert,
/// and the same alert is not repeated within kCooldownMs.
class TargetAlert
{
public:
    enum class Breach { None, Low, High };

    static constexpr qint64 kCooldownMs = 10000;

    /// @param nowMs  current time in milliseconds, used only for the cooldown.
    Breach check(double value, double target, double range,
                 bool alertLow, bool alertHigh, qint64 nowMs);

private:
    bool ready(qint64 lastMs, qint64 nowMs) const;

    qint64 m_lastLowMs  = -1;
    qint64 m_lastHighMs = -1;
};

#endif // TARGETALERT_H
