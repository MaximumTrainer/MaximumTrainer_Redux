#include "zwift_click_relay.h"

#include "zwift_click_protocol.h"
#include "logger.h"

#include <QDateTime>
#include <QLowEnergyDescriptor>
#include <QTimer>

namespace {
const QByteArray kNotifyOn   = QByteArray::fromHex("0100");
const QByteArray kIndicateOn = QByteArray::fromHex("0200");

// The trainer-side relay setup written to the FC82 control point before the
// Click is connected (configures the trainer's ZCS relay). Sent once, paced.
const char *const kSetupSteps[] = {
    "410805", "000800", "442001", "440801",
    "410805", "000810", "0008800a", "450801",
};

// "Zwift Click" name bytes the trainer announces in its 0x47 device frame.
const QByteArray kZwiftClickName = QByteArrayLiteral("Zwift Click");

// Relayed RideOn that makes the linked Click start streaming buttons (the one
// command that actually matters).
QByteArray relayedRideOn() { return QByteArray::fromHex("4e08021208526964654f6e0203"); }

// EXPERIMENT: the follow-up frames Zwift sent after the relayed RideOn (from the
// capture, handle-83 writes). Includes the long ff04 key frame — that value is
// SESSION-SPECIFIC (tied to the Click's ephemeral ECDH pubkey), so replaying the
// captured bytes may not satisfy the current session; we try it to see if it
// keeps the Click awake (LED solid) without computing real crypto.
const char *const kZwiftFollowup[] = {
    "4e08021203000800", "4e08021203410805", "4e08021203000810",
    "4e0802120400088008", "4e0802120400088306", "440800",
    "4e08021203ff0400",
    "4e0802121aff04000a153899a425aa40d1200ddf",
};

constexpr int  kButtonDebounceMs = 300;   // one physical press counts once
constexpr int  kRideOnDelayMs    = 4500;  // 441002 → RideOn gap (matches capture)
constexpr int  kProgressMs       = 4000;  // re-send RideOn if no frames yet
constexpr int  kMaxRetries       = 8;
constexpr int  kWatchdogMs       = 5000;
constexpr int  kStallMs          = 20000; // >20 s silent ⇒ stalled
} // namespace

ZwiftClickRelay::ZwiftClickRelay(QObject *parent) : QObject(parent)
{
    m_notifyUuid   = QBluetoothUuid(QString::fromLatin1(ZwiftClickProtocol::Uuid::Measurement));
    m_controlUuid  = QBluetoothUuid(QString::fromLatin1(ZwiftClickProtocol::Uuid::ControlPoint));
    m_indicateUuid = QBluetoothUuid(QString::fromLatin1(ZwiftClickProtocol::Uuid::Response));
}

void ZwiftClickRelay::detach()
{
    if (m_watchdog)
        m_watchdog->stop();
    if (m_fc82) {
        disconnect(m_fc82, nullptr, this, nullptr);   // drop links to the dying service
        m_fc82 = nullptr;
    }
    // Reset the handshake state so a later attach starts over; pending timers
    // become no-ops while m_fc82 is null.
    m_setupDone = m_connectSent = m_rideOnSent = m_sawRelay = m_stalled = false;
    m_retries = 0;
    m_lastRelayMs = 0;
    m_lastBitmap = 0xFFFFFFFFu;
}

void ZwiftClickRelay::attachService(QLowEnergyService *fc82Service)
{
    if (!fc82Service || fc82Service == m_fc82)
        return;
    detach();                          // clear any previous service + state
    m_fc82 = fc82Service;
    if (!m_clock.isValid())
        m_clock.start();

    connect(m_fc82, &QLowEnergyService::stateChanged,
            this, &ZwiftClickRelay::onServiceStateChanged);
    connect(m_fc82, &QLowEnergyService::characteristicChanged,
            this, &ZwiftClickRelay::onCharacteristicChanged);

    const auto discovered  = QLowEnergyService::RemoteServiceDiscovered;
    const auto discovering = QLowEnergyService::RemoteServiceDiscovering;
    if (m_fc82->state() == discovered)
        beginSetup();
    else if (m_fc82->state() != discovering)
        m_fc82->discoverDetails();
}

void ZwiftClickRelay::onServiceStateChanged(QLowEnergyService::ServiceState state)
{
    if (state != QLowEnergyService::RemoteServiceDiscovered)
        return;
    beginSetup();
}

void ZwiftClickRelay::beginSetup()
{
    if (m_setupDone || !m_fc82)
        return;
    m_setupDone = true;

    // Subscribe FC82 notify + indicate so we receive the relayed Click frames.
    for (const QLowEnergyCharacteristic &c : m_fc82->characteristics()) {
        const QLowEnergyDescriptor cccd =
            c.descriptor(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
        if (!cccd.isValid())
            continue;
        if (c.properties() & QLowEnergyCharacteristic::Notify)
            m_fc82->writeDescriptor(cccd, kNotifyOn);
        else if (c.properties() & QLowEnergyCharacteristic::Indicate)
            m_fc82->writeDescriptor(cccd, kIndicateOn);
    }

    // RideOn handshake, then the trainer-relay setup bytes (paced so we don't
    // overrun the control point).
    writeControl(ZwiftClickProtocol::rideOnTrainerHandshake());
    int delay = 0;
    for (const char *hex : kSetupSteps) {
        const QByteArray bytes = QByteArray::fromHex(hex);
        delay += 120;
        QTimer::singleShot(delay, this, [this, bytes] { writeControl(bytes); });
    }
    LOG_INFO("ZwiftClickRelay", QStringLiteral("FC82 relay armed — waiting for the Click"));
}

void ZwiftClickRelay::writeControl(const QByteArray &bytes)
{
    if (!m_fc82)
        return;
    const QLowEnergyCharacteristic cp = m_fc82->characteristic(m_controlUuid);
    if (!cp.isValid())
        return;
    const auto mode = (cp.properties() & QLowEnergyCharacteristic::WriteNoResponse)
                          ? QLowEnergyService::WriteWithoutResponse
                          : QLowEnergyService::WriteWithResponse;
    m_fc82->writeCharacteristic(cp, bytes, mode);
}

void ZwiftClickRelay::onCharacteristicChanged(const QLowEnergyCharacteristic &c,
                                              const QByteArray &value)
{
    Q_UNUSED(c);
    if (value.isEmpty())
        return;

    // Any 0x4e frame is a relayed Click frame (handshake/status/button) → live.
    if (m_rideOnSent && quint8(value.at(0)) == 0x4e) {
        m_lastRelayMs = QDateTime::currentMSecsSinceEpoch();
        if (m_stalled) {
            m_stalled = false;
            emit relayActiveChanged(true);
            LOG_WARN("ZwiftClickRelay", QStringLiteral("relay RESUMED — Click reconnected"));
        }
        if (!m_sawRelay) {
            m_sawRelay = true;
            emit relayActiveChanged(true);
            LOG_INFO("ZwiftClickRelay", QStringLiteral("Click relaying through trainer — live"));
            if (!m_watchdog) {
                m_watchdog = new QTimer(this);
                connect(m_watchdog, &QTimer::timeout, this, &ZwiftClickRelay::watchdogTick);
            }
            m_watchdog->start(kWatchdogMs);

            // EXPERIMENT: replay Zwift's follow-up frames (incl. the captured ff04
            // key frame), paced, to see if completing the post-RideOn sequence
            // keeps the Click session alive through idle.
            int delay = 0;
            for (const char *hex : kZwiftFollowup) {
                const QByteArray b = QByteArray::fromHex(hex);
                delay += 300;
                QTimer::singleShot(delay, this, [this, b] { writeControl(b); });
            }
            LOG_INFO("ZwiftClickRelay", QStringLiteral("sent Zwift follow-up sequence (incl. captured ff04) — keep-alive experiment"));
        }
    }

    // EXPERIMENT: the Click periodically sends an ff03 secure-handshake frame
    // (its session pubkey). Zwift replies (ff0400 ack, then key material); we
    // never did, so the Click may abandon the session and sleep (LED off). Reply
    // with the short fixed ff0400 ack — a constant byte sequence, no key
    // derivation — to see if it keeps the session alive without real crypto.
    if (value.contains(QByteArray::fromHex("ff03"))) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastSecureAckMs > 500) {
            m_lastSecureAckMs = now;
            writeControl(QByteArray::fromHex("4e08021203ff0400"));
            LOG_INFO("ZwiftClickRelay", QStringLiteral("ff03 seen → replied ff0400 ack (keep-alive experiment)"));
        }
    }

    // Decode a relayed button frame and dispatch transitions.
    quint32 bitmap;
    if (ZwiftClickProtocol::decodeRelayedClickButtons(value, bitmap)) {
        handleBitmap(bitmap);
        return;
    }

    // The trainer announced the linked Click → ask it to connect (once).
    if (!m_connectSent && quint8(value.at(0)) == 0x47 && value.contains(kZwiftClickName))
        requestClickConnect();
}

void ZwiftClickRelay::requestClickConnect()
{
    m_connectSent = true;
    writeControl(QByteArray::fromHex("441002"));
    // The trainer needs a few seconds to link + discover the Click before a
    // relayed RideOn can reach it; checkProgress retries if we're early.
    QTimer::singleShot(kRideOnDelayMs, this, [this] { relayRideOn(false); });
    LOG_INFO("ZwiftClickRelay", QStringLiteral("Click announced — connecting via trainer"));
}

void ZwiftClickRelay::relayRideOn(bool isRetry)
{
    if (!m_fc82 || m_sawRelay)
        return;                 // detached, or already streaming
    if (isRetry && m_retries >= kMaxRetries) {
        LOG_WARN("ZwiftClickRelay",
                 QStringLiteral("no relayed frames after %1 attempts").arg(m_retries));
        return;
    }
    if (isRetry)
        ++m_retries;
    m_rideOnSent = true;
    writeControl(relayedRideOn());
    QTimer::singleShot(kProgressMs, this, &ZwiftClickRelay::checkProgress);
}

void ZwiftClickRelay::checkProgress()
{
    if (m_sawRelay)
        return;
    relayRideOn(/*isRetry=*/true);
}

void ZwiftClickRelay::watchdogTick()
{
    if (!m_fc82 || !m_sawRelay)
        return;
    const qint64 quietMs = QDateTime::currentMSecsSinceEpoch() - m_lastRelayMs;
    if (quietMs <= kStallMs)
        return;
    if (!m_stalled) {
        m_stalled = true;
        emit relayActiveChanged(false);
        LOG_WARN("ZwiftClickRelay",
                 QStringLiteral("relay quiet %1s — re-connecting the Click").arg(quietMs / 1000));
    }
    // A long silence means the Click left the trainer's range and the trainer
    // dropped its link (LED back to flashing) — RideOn alone won't bring it back.
    // Re-issue the connect (441002) so the trainer re-links it, then re-arm.
    writeControl(QByteArray::fromHex("441002"));
    writeControl(relayedRideOn());
}

void ZwiftClickRelay::handleBitmap(quint32 bitmap)
{
    const quint32 changed = bitmap ^ m_lastBitmap;
    if (!changed) {
        m_lastBitmap = bitmap;
        return;
    }
    for (int bit = 0; bit < 32; ++bit) {
        if (!(changed & (1u << bit)))
            continue;
        if (ZwiftClickProtocol::clickButtonPressed(bitmap, bit) && acceptButton(bit))
            dispatchButton(bit);
    }
    m_lastBitmap = bitmap;
}

bool ZwiftClickRelay::acceptButton(int bit)
{
    if (bit < 0 || bit >= 32)
        return false;
    const qint64 now = m_clock.isValid() ? m_clock.elapsed() : 0;
    if (now - m_lastButtonMs[bit] < kButtonDebounceMs)
        return false;           // repeated edge
    m_lastButtonMs[bit] = now;
    return true;
}

void ZwiftClickRelay::dispatchButton(int bit)
{
    using namespace ZwiftClick;
    switch (bit) {
    case UpShiftBit:   emit paddleUpPressed();    break;
    case DownShiftBit: emit paddleDownPressed();  break;
    case ButtonABit:   emit buttonAPressed();     break;
    case ButtonBBit:   emit buttonBPressed();     break;
    case ButtonYBit:   emit buttonYPressed();     break;
    case ButtonZBit:   emit buttonZPressed();     break;
    case DpadLeftBit:  emit dpadLeftPressed();    break;
    case DpadUpBit:    emit dpadUpPressed();      break;
    case DpadRightBit: emit dpadRightPressed();   break;
    case DpadDownBit:  emit dpadDownPressed();    break;
    default:           emit unmappedButton(bit);  break;
    }
}
