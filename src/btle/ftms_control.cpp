#include "ftms_control.h"

#include "logger.h"

#include <QTimer>

namespace {
constexpr quint8 kRequestControl = 0x00;
constexpr quint8 kReset = 0x01;
constexpr quint8 kSetTargetPower = 0x05;
constexpr quint8 kStartOrResume = 0x07;
constexpr quint8 kResponseCode = 0x80;
constexpr quint8 kResultSuccess = 0x01;
constexpr quint8 kResultControlNotPermitted = 0x05;

// Fitness Machine Status (0x2ADA) op codes that leave the trainer without our target.
constexpr quint8 kStatusReset = 0x01;
constexpr quint8 kStatusStoppedByUser = 0x02;
constexpr quint8 kStatusStartedByUser = 0x04;
constexpr quint8 kStatusControlPermissionLost = 0xFF;

QString opcodeText(quint8 opcode)
{
    return QStringLiteral("0x%1").arg(opcode, 2, 16, QLatin1Char('0'));
}

FtmsControl::TrainerMode modeOf(const QByteArray &target)
{
    return quint8(target.at(0)) == kSetTargetPower ? FtmsControl::TrainerMode::Erg
                                                   : FtmsControl::TrainerMode::Other;
}

qint16 int16At(const QByteArray &payload, int offset)
{
    return qint16(quint16(quint8(payload.at(offset))) | quint16(quint8(payload.at(offset + 1))) << 8);
}

QString targetText(const QByteArray &target)
{
    const quint8 opcode = quint8(target.at(0));
    if (opcode == 0x05 && target.size() >= 3)
        return QStringLiteral("power %1 W").arg(int16At(target, 1));
    if (opcode == 0x04 && target.size() >= 3)
        return QStringLiteral("resistance level %1").arg(int16At(target, 1) / 10.0, 0, 'f', 1);
    if (opcode == 0x11 && target.size() >= 5)
        return QStringLiteral("slope %1 %").arg(int16At(target, 3) / 100.0, 0, 'f', 2);
    return QStringLiteral("op %1").arg(opcodeText(opcode));
}
} // namespace

FtmsControl::FtmsControl(QObject *parent)
    : QObject(parent)
{
    m_opTimeout = new QTimer(this);
    m_opTimeout->setSingleShot(true);
    m_opTimeout->setInterval(kOpTimeoutMs);
    connect(m_opTimeout, &QTimer::timeout, this, &FtmsControl::onOpTimeout);

    m_retry = new QTimer(this);
    m_retry->setSingleShot(true);
    m_retry->setInterval(kRetryDelayMs);
    connect(m_retry, &QTimer::timeout, this, &FtmsControl::pump);

    // A trainer can drop its target without telling us (a status notification
    // it does not send, a brief power-save); re-sending the same target now and
    // then costs one write and puts it back.
    m_refresh = new QTimer(this);
    m_refresh->setInterval(kRefreshMs);
    connect(m_refresh, &QTimer::timeout, this, [this]() {
        if (!m_granted || m_target.isEmpty())
            return;
        m_confirmed.clear();
        m_refused.clear();
        pump();
    });
}

void FtmsControl::reset()
{
    m_ready = false;
    m_granted = false;
    m_started = false;
    m_trainerMode = TrainerMode::None;
    m_resetUnsupported = false;
    m_inFlight = false;
    m_inFlightPayload.clear();
    m_confirmed.clear();
    m_refused.clear();
    m_lastLoggedTarget.clear();
    m_lastLoggedConfirmed.clear();
    m_opTimeout->stop();
    m_retry->stop();
    // The wanted target survives: it is what the trainer must hold once the
    // connection is back.
}

void FtmsControl::setReady()
{
    m_ready = true;
    pump();
}

void FtmsControl::setTarget(const QByteArray &command)
{
    if (command.isEmpty())
        return;
    m_target = command;
    if (!m_refresh->isActive())
        m_refresh->start();
    pump();
}

void FtmsControl::reassert()
{
    m_started = false;
    m_confirmed.clear();
    m_refused.clear();
    m_lastLoggedTarget.clear();
    m_lastLoggedConfirmed.clear();
    pump();
}

void FtmsControl::pump()
{
    if (!m_ready || m_inFlight || m_retry->isActive())
        return;
    if (!m_granted) {
        LOG_INFO("BtleHub", QStringLiteral("FTMS Request Control sent"));
        write(QByteArray(1, char(kRequestControl)));
        return;
    }
    // FTMS wants Start or Resume before targets; some trainers acknowledge a
    // target without it and keep their old resistance.
    if (!m_started) {
        LOG_INFO("BtleHub", QStringLiteral("FTMS Start/Resume sent"));
        write(QByteArray(1, char(kStartOrResume)));
        return;
    }
    if (m_target.isEmpty() || m_target == m_confirmed || m_target == m_refused)
        return;
    // A trainer left in simulation or resistance mode can acknowledge Set
    // Target Power and keep riding as a flat road (JetBlack Victory, 28 Nov):
    // a fresh connection used to clear the mode, so put the trainer back to its
    // defaults the same way before switching between ERG and the other modes.
    if (m_trainerMode != TrainerMode::None && modeOf(m_target) != m_trainerMode && !m_resetUnsupported) {
        LOG_INFO("BtleHub", QStringLiteral("FTMS switching to %1: resetting the trainer first")
                                .arg(modeOf(m_target) == TrainerMode::Erg ? QStringLiteral("ERG")
                                                                          : QStringLiteral("simulation/resistance")));
        write(QByteArray(1, char(kReset)));
        return;
    }
    if (m_target != m_lastLoggedTarget) {
        m_lastLoggedTarget = m_target;
        LOG_INFO("BtleHub", QStringLiteral("FTMS target sent: %1").arg(targetText(m_target)));
    }
    write(m_target);
}

void FtmsControl::write(const QByteArray &payload)
{
    m_inFlight = true;
    m_inFlightPayload = payload;
    m_opTimeout->start();
    LOG_VERBOSE("BtleHub", QStringLiteral("FTMS op %1 sent (%2 bytes)")
                             .arg(opcodeText(quint8(payload.at(0)))).arg(payload.size()));
    emit writeRequested(payload);
}

void FtmsControl::onControlPointIndication(const QByteArray &indication)
{
    if (indication.size() < 3 || quint8(indication.at(0)) != kResponseCode)
        return;
    const quint8 requestOp = quint8(indication.at(1));
    const quint8 result = quint8(indication.at(2));

    m_inFlight = false;
    m_opTimeout->stop();
    const QByteArray answered = m_inFlightPayload;
    m_inFlightPayload.clear();

    if (requestOp == kRequestControl) {
        m_granted = result == kResultSuccess;
        m_started = false;
        if (m_granted) {
            LOG_INFO("BtleHub", QStringLiteral("FTMS control granted by trainer"));
            emit granted();
        } else {
            LOG_WARN("BtleHub", QStringLiteral("FTMS control refused (result %1), asking again")
                                    .arg(opcodeText(result)));
            m_retry->start();
        }
        pump();
        return;
    }

    if (requestOp == kReset && result != kResultControlNotPermitted) {
        if (result == kResultSuccess) {
            // Reset drops control and the session, like a new connection.
            m_granted = false;
            m_started = false;
            m_confirmed.clear();
            m_trainerMode = TrainerMode::None;
        } else {
            m_resetUnsupported = true;
        }
        LOG_INFO("BtleHub", QStringLiteral("FTMS Reset answered (result %1)").arg(opcodeText(result)));
        pump();
        return;
    }

    if (requestOp == kStartOrResume && result != kResultControlNotPermitted) {
        // A trainer that does not support it (or is already running) still
        // takes targets, so any other answer moves on to the target.
        m_started = true;
        LOG_INFO("BtleHub", QStringLiteral("FTMS Start/Resume answered (result %1)").arg(opcodeText(result)));
        pump();
        return;
    }

    const bool answersOurOp = !answered.isEmpty() && quint8(answered.at(0)) == requestOp;
    if (result == kResultSuccess) {
        if (answersOurOp) {
            m_confirmed = answered;
            m_trainerMode = modeOf(answered);
            if (answered != m_lastLoggedConfirmed) {
                m_lastLoggedConfirmed = answered;
                LOG_INFO("BtleHub", QStringLiteral("FTMS target confirmed by trainer: %1").arg(targetText(answered)));
            }
        }
        LOG_VERBOSE("BtleHub", QStringLiteral("FTMS op %1 acknowledged by trainer").arg(opcodeText(requestOp)));
    } else if (result == kResultControlNotPermitted) {
        LOG_WARN("BtleHub", QStringLiteral("FTMS op %1 refused: trainer control lost, requesting it again")
                                .arg(opcodeText(requestOp)));
        m_granted = false;
    } else {
        LOG_WARN("BtleHub", QStringLiteral("FTMS control point refused opcode %1 (result %2)")
                                .arg(opcodeText(requestOp), opcodeText(result)));
        if (answersOurOp)
            m_refused = answered;
    }
    pump();
}

void FtmsControl::onMachineStatus(const QByteArray &status)
{
    if (status.isEmpty())
        return;
    switch (quint8(status.at(0))) {
    case kStatusControlPermissionLost:
    case kStatusReset:
        LOG_WARN("BtleHub", QStringLiteral("FTMS trainer %1, requesting control again")
                                .arg(quint8(status.at(0)) == kStatusReset ? QStringLiteral("reset")
                                                                         : QStringLiteral("dropped our control")));
        m_granted = false;
        m_started = false;
        m_confirmed.clear();
        pump();
        break;
    case kStatusStoppedByUser:
        LOG_INFO("BtleHub", QStringLiteral("FTMS trainer stopped or paused, resuming it"));
        m_started = false;
        m_confirmed.clear();
        pump();
        break;
    case kStatusStartedByUser:
        m_confirmed.clear();
        pump();
        break;
    default:
        break;
    }
}

void FtmsControl::onWriteFailed()
{
    if (!m_inFlight)
        return;
    LOG_WARN("BtleHub", QStringLiteral("FTMS write of op %1 failed, retrying")
                            .arg(opcodeText(quint8(m_inFlightPayload.at(0)))));
    m_inFlight = false;
    m_inFlightPayload.clear();
    m_opTimeout->stop();
    m_retry->start();
}

void FtmsControl::onOpTimeout()
{
    if (!m_inFlight)
        return;
    const quint8 opcode = quint8(m_inFlightPayload.at(0));
    LOG_WARN("BtleHub", QStringLiteral("FTMS response to op %1 timed out, retrying")
                            .arg(opcodeText(opcode)));
    if (opcode == kStartOrResume)
        m_started = true;   // not worth holding the target back for
    if (opcode == kReset)
        m_resetUnsupported = true;   // a refused target asks for control again if it did reset
    m_inFlight = false;
    m_inFlightPayload.clear();
    pump();
}
