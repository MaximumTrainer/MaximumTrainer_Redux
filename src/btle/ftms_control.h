#pragma once

#include <QByteArray>
#include <QObject>

class QTimer;

/// The FTMS Control Point (0x2AD9) conversation with one trainer, kept free of
/// Bluetooth objects so it can be tested: BtleHub feeds it the trainer's
/// indications and writes whatever writeRequested() asks for.
///
/// It owns the one thing ERG depends on: the trainer ends up holding the newest
/// target. One op is in flight at a time (real trainers answer an overlapping
/// write with ATT error 0x80), a newer target replaces an older queued one, and
/// control is requested again whenever the trainer drops it (Machine Status
/// "control permission lost" or "reset", a target refused with "control not
/// permitted", a Request Control that never got an answer). Each grant is
/// followed by Start or Resume before the target, and switching between ERG and
/// simulation/resistance goes through Reset first.
class FtmsControl : public QObject
{
    Q_OBJECT

public:
    /// What the trainer was last told to do on this connection; Unknown until
    /// the first Reset on a new connection, None right after a Reset.
    enum class TrainerMode { Unknown, None, Erg, Other };

    explicit FtmsControl(QObject *parent = nullptr);

    /// A new connection: forget control, the queue and what the trainer holds.
    void reset();
    /// The Control Point accepts commands (its indications are enabled), so
    /// control can be requested.
    void setReady();
    bool isReady() const { return m_ready; }
    bool controlGranted() const { return m_granted; }

    /// The trainer state wanted now (Set Target Power, Resistance Level or
    /// Indoor Bike Simulation payload). Sent once control is held.
    void setTarget(const QByteArray &command);
    QByteArray target() const { return m_target; }
    /// Send Start or Resume and the current target again even if the trainer
    /// confirmed them, and get control back first if it was lost. Called when a ride starts on a
    /// connection that was opened earlier.
    void reassert();

    void onControlPointIndication(const QByteArray &indication);
    void onMachineStatus(const QByteArray &status);
    void onWriteFailed();

    static constexpr int kOpTimeoutMs = 2500;
    static constexpr int kRetryDelayMs = 1500;
    static constexpr int kRefreshMs = 20000;

signals:
    void writeRequested(const QByteArray &payload);
    void granted();

private:
    void pump();
    void write(const QByteArray &payload);
    void onOpTimeout();

    bool m_ready = false;
    bool m_granted = false;
    bool m_started = false;   ///< Start or Resume answered since control was granted
    TrainerMode m_trainerMode = TrainerMode::Unknown;
    bool m_resetUnsupported = false;
    bool m_inFlight = false;
    QByteArray m_inFlightPayload;
    QByteArray m_target;
    QByteArray m_confirmed;   ///< the target the trainer last acknowledged
    QByteArray m_refused;     ///< a target the trainer refused; not retried until it changes
    QByteArray m_lastLoggedTarget;
    QByteArray m_lastLoggedConfirmed;
    QTimer *m_opTimeout = nullptr;
    QTimer *m_retry = nullptr;
    QTimer *m_refresh = nullptr;
};
