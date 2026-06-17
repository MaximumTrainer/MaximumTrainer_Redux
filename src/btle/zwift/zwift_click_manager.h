#ifndef ZWIFT_CLICK_MANAGER_H
#define ZWIFT_CLICK_MANAGER_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QElapsedTimer>
#include <QList>
#include <QSet>
#include <QString>

class ZwiftClickHub;

/*
 * ZwiftClickManager
 *
 * Owns the connected Zwift Click v2 controllers as a single logical input. It
 * connects a ZwiftClickHub to each shifter, applies the per-bit cross-device
 * debounce (the linked pair echoes the shift on BOTH BLE devices, and buttons
 * can repeat — so one physical press of any button counts once), and maps each
 * button bit to a high-level action signal.
 *
 * The connection is per-workout: the pre-workout sensor screen feeds it the
 * shifters it discovers (connectDevice) or it can scan itself (start), and the
 * workout owns it and frees it on close.
 *
 * Fixed button map (Zwift Click v2, captured on hardware):
 *   right paddle (12) → shiftUp        left paddle (8)  → shiftDown
 *   Y (6)            → difficultyUp     B (5)           → difficultyDown
 *   A (4)            → startPauseWorkout  Z (7)         → lap
 *   d-pad L (0)      → radioPrev        d-pad R (2)     → radioNext
 *   d-pad U (1)      → radioVolumeUp    d-pad D (3)     → radioVolumeDown
 */
class ZwiftClickManager : public QObject
{
    Q_OBJECT
public:
    explicit ZwiftClickManager(QObject *parent = nullptr);

    /// Scan for Click v2 controllers and connect to each as it advertises (used
    /// by the standalone test harness). stop() tears everything down.
    void start(const QString &nameFilter = QString());
    /// Connect a hub to one already-discovered shifter (fed from another screen's
    /// shared BLE scan, so we don't run a second discovery agent).
    void connectDevice(const QBluetoothDeviceInfo &info);
    /// Stop scanning but KEEP the connected hubs (used after the connection is
    /// handed to the workout — no need to keep discovering).
    void stopDiscovery();
    void stop();

    int connectedCount() const;

    /// True if a discovered device looks like a Click/Play shifter (and not the
    /// trainer), so a shared scan can decide which devices to feed us.
    static bool looksLikeShifter(const QBluetoothDeviceInfo &info,
                                 const QString &nameFilter = QString());

signals:
    // Mapped actions (one per physical button press, deduped across devices).
    void shiftUp();
    void shiftDown();
    void difficultyUp();
    void difficultyDown();
    void startPauseWorkout();
    void lap();
    void radioPrev();
    void radioNext();
    void radioVolumeUp();
    void radioVolumeDown();

    // Status / diagnostics.
    void deviceConnected(const QString &name, const QString &address);
    void deviceDisconnected(const QString &address);
    void unmappedButton(int bitIndex);

private:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onButtonPressed(int bit);
    bool acceptButton(int bit);   // false if this bit fired too recently
    void restartDiscovery();      // (re)start the LE scan if not already running

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    QString                         m_nameFilter;
    QSet<QString>                   m_knownIds;  // device keys already handled (dedup)
    QList<ZwiftClickHub*>           m_hubs;

    QElapsedTimer m_clock;
    qint64        m_lastButtonMs[32];
    static constexpr qint64 BUTTON_DEBOUNCE_MS = 300;
};

#endif // ZWIFT_CLICK_MANAGER_H
