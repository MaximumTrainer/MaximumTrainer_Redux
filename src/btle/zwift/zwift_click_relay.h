#ifndef ZWIFT_CLICK_RELAY_H
#define ZWIFT_CLICK_RELAY_H

#include <QObject>
#include <QBluetoothUuid>
#include <QElapsedTimer>
#include <QLowEnergyCharacteristic>
#include <QLowEnergyService>

class QTimer;

/*
 * Reads a Click v2 controller through the trainer's FC82 relay instead of
 * connecting to the controller's own (unstable) BLE device directly. The
 * controller links to the trainer; the trainer relays its button frames to us
 * over FC82, so the BLE link is held by the trainer and stays stable for a whole
 * workout (validated with a ~15-min soak).
 *
 * BtleHub owns the trainer connection; when it discovers the FC82 service it
 * hands the service here, and this object drives the relay handshake and decodes
 * the relayed button frames into high-level action signals.
 *
 * The handshake subscribes to FC82 notify/indicate, writes the relay-setup
 * sequence to the control point, asks the trainer to connect the linked
 * controller once it is announced, and then reads the streamed button frames. A
 * watchdog re-pings if the relay falls silent. Native only.
 */
class ZwiftClickRelay : public QObject
{
    Q_OBJECT
public:
    explicit ZwiftClickRelay(QObject *parent = nullptr);

    // Drive the relay on an FC82 service object created by BtleHub. Safe to call
    // whether the service is still discovering or already discovered, and to call
    // again with a NEW service after a trainer reconnect (state is reset; callers
    // that wired our signals stay wired since this object persists).
    void attachService(QLowEnergyService *fc82Service);

    // Stop using the current service (it's about to be destroyed on a trainer
    // disconnect) and reset state so the next attachService restarts cleanly.
    void detach();

signals:
    // One signal per physical button (debounced, fired on press). The relay knows
    // only the controller's buttons; the caller decides what each does, so the
    // action mapping can change without touching the relay. The two shift paddles
    // are the only fixed-purpose buttons (that's their physical identity).
    void paddleUpPressed();    // right paddle  (bit 12)
    void paddleDownPressed();  // left  paddle  (bit 8)
    void buttonAPressed();     // bit 4
    void buttonBPressed();     // bit 5
    void buttonYPressed();     // bit 6
    void buttonZPressed();     // bit 7
    void dpadLeftPressed();    // bit 0
    void dpadUpPressed();      // bit 1
    void dpadRightPressed();   // bit 2
    void dpadDownPressed();    // bit 3

    // True once relayed Click frames are flowing, false if the watchdog flags a
    // stall — for optional status UI / logging.
    void relayActiveChanged(bool active);
    void unmappedButton(int bitIndex);

private slots:
    void onServiceStateChanged(QLowEnergyService::ServiceState state);
    void onCharacteristicChanged(const QLowEnergyCharacteristic &c, const QByteArray &value);

private:
    void beginSetup();                       // subscribe + RideOn + relay-setup writes
    void writeControl(const QByteArray &bytes);
    void requestClickConnect();              // 441002 once the Click is announced
    void relayRideOn(bool isRetry);          // tell the Click to start streaming
    void checkProgress();                    // re-send RideOn until frames flow
    void watchdogTick();                     // re-ping if the relay falls silent
    void handleBitmap(quint32 bitmap);
    bool acceptButton(int bit);
    void dispatchButton(int bit);

    QLowEnergyService *m_fc82 = nullptr;
    QBluetoothUuid     m_notifyUuid;
    QBluetoothUuid     m_controlUuid;
    QBluetoothUuid     m_indicateUuid;

    bool   m_setupDone    = false;
    bool   m_connectSent  = false;   // sent 441002
    bool   m_rideOnSent   = false;   // relayed RideOn to the Click
    bool   m_sawRelay     = false;   // a relayed 0x4e frame arrived → live
    int    m_retries      = 0;
    qint64 m_lastRelayMs  = 0;
    qint64 m_lastSecureAckMs = 0;    // last reply to the Click's ff03 (keep-alive experiment)
    bool   m_stalled      = false;

    quint32       m_lastBitmap = 0xFFFFFFFFu;
    QElapsedTimer m_clock;                    // per-bit debounce clock
    qint64        m_lastButtonMs[32] = {0};

    QTimer *m_watchdog = nullptr;
};

#endif // ZWIFT_CLICK_RELAY_H
