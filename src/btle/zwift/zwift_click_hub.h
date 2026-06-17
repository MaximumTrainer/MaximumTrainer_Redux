#ifndef ZWIFT_CLICK_HUB_H
#define ZWIFT_CLICK_HUB_H

#include <QObject>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyService>

class QTimer;

/*
 * ZwiftClickHub
 *
 * Input hub for a single Zwift Click / Play shifter. These advertise the Zwift
 * 0xFC82 service and — on the firmware tested (Click/Play fw 1.2.0) — speak
 * UNENCRYPTED: write "RideOn" to the control point, then the device streams a
 * 0x23 button-status frame on the measurement char ~2x/s (field 1 = a 32-bit
 * active-low button bitmap). See notes/zwift-cog-protocol-findings.md.
 *
 * A +/- pair is two physical devices; each reports its shift paddle on a fixed
 * bit (right/up = 12, left/down = 8) so direction is known from the data alone —
 * no press-to-assign. Other buttons (d-pad, Y/Z/A/B) arrive as other bits and
 * are surfaced raw via buttonPressed() for future mapping (radio/volume/pause).
 *
 * One controller per instance: a +/- pair is two ZwiftClickHub objects.
 */
class ZwiftClickHub : public QObject
{
    Q_OBJECT

public:
    explicit ZwiftClickHub(QObject *parent = nullptr);
    ~ZwiftClickHub() override;

    void connectToDevice(const QBluetoothDeviceInfo &device);
    void disconnectFromDevice();

    bool isConnected() const;
    QString deviceName() const { return m_name; }
    QString deviceAddress() const;

signals:
    void connected();
    void disconnected();
    void connectionError(const QString &message);

    /// Shift paddle pressed: delta = +1 (upshift, bit 12) or -1 (downshift, bit 8).
    void shiftRequested(int delta);

    /// Any button edge (active-low bit cleared/set). bitIndex is 0..31. The two
    /// shift bits also fire these, so a consumer can treat the device uniformly.
    void buttonPressed(int bitIndex);
    void buttonReleased(int bitIndex);

    /// Diagnostic: the raw active-low bitmap, emitted whenever it changes (before
    /// debounce). Lets the test harness show the exact DOWN/UP frame sequence.
    void frameChanged(quint32 bitmap);

private:
    void teardown();
    void startController();   // (re)create the controller and connect (with retry)
    void setupService();
    void sendRideOn();        // write the RideOn handshake (kicks the stream)
    void watchdogTick();      // re-kick / reconnect a stream that went silent
    void onCharacteristicChanged(const QLowEnergyCharacteristic &c,
                                 const QByteArray &value);

    QLowEnergyController *m_controller = nullptr;
    QLowEnergyService    *m_service    = nullptr;
    QBluetoothDeviceInfo  m_device;
    QString               m_name;

    // Last seen active-low bitmap (idle 0xFFFFFFFF) for edge detection.
    quint32 m_lastBitmap = 0xFFFFFFFFu;

    // Stream health watchdog. A connected controller can stop streaming with the
    // BLE link still up (no disconnect fires) — observed on a Click v2 pair where
    // one unit went silent and never recovered. The watchdog ticks while
    // connected: when a device has been silent past QUIET it re-kicks RideOn to
    // restart the stream, and if it stays silent past STALL it forces a full
    // reconnect. A healthy stream (frames flowing) never triggers either.
    QTimer *m_watchdog = nullptr;
    bool    m_streamQuiet = false;
    quint64 m_frameCount = 0;
    qint64  m_lastFrameMs = 0;     // QDateTime::currentMSecsSinceEpoch of last frame
    qint64  m_lastKickMs  = 0;     // last RideOn re-kick, so we don't spam it
    static constexpr int STREAM_QUIET_MS = 2500;   // log/“quiet” threshold
    static constexpr int KICK_EVERY_MS   = 3000;   // min gap between RideOn re-kicks
    static constexpr int STALL_MS        = 20000;  // silent this long ⇒ reconnect
    static constexpr int WATCHDOG_TICK_MS = 1000;

    // Connect retry: "Unknown Error" on connect is usually transient.
    int m_retriesLeft = 0;
    static constexpr int MAX_RETRIES = 4;
};

#endif // ZWIFT_CLICK_HUB_H
