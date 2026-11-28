#include "ftms_control.h"

#include "logger.h"

#include <QTimer>

namespace {
constexpr quint8 kRequestControl = 0x00;
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
        if (m_granted && !m_target.isEmpty())
            reassert();
    });
}

void FtmsControl::reset()
{
    m_ready = false;
    m_granted = false;
    m_inFlight = false;
    m_inFlightPayload.clear();
    m_confirmed.clear();
    m_refused.clear();
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
    m_confirmed.clear();
    m_refused.clear();
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
    if (m_target.isEmpty() || m_target == m_confirmed || m_target == m_refused)
        return;
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

    const bool answersOurOp = !answered.isEmpty() && quint8(answered.at(0)) == requestOp;
    if (result == kResultSuccess) {
        if (answersOurOp)
            m_confirmed = answered;
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
        m_confirmed.clear();
        pump();
        break;
    case kStatusStoppedByUser:
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
    LOG_WARN("BtleHub", QStringLiteral("FTMS response to op %1 timed out, retrying")
                            .arg(opcodeText(quint8(m_inFlightPayload.at(0)))));
    m_inFlight = false;
    m_inFlightPayload.clear();
    pump();
}
