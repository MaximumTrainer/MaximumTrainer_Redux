#ifndef DEVICE_SESSION_H
#define DEVICE_SESSION_H

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>

#include "sensor_connector.h"

class Account;
class BtleHub;
class QTimer;
class ZwiftClickManager;

/*
 * DeviceSession
 *
 * Keeps the solo rider's saved sensors and Zwift Click connected for as long as
 * the app runs, so a workout or course borrows live connections instead of
 * connecting from scratch every time. Missing devices are retried in the
 * background. Studio mode and simulated sensors keep their own per-ride flow:
 * the session then holds nothing.
 */
class DeviceSession : public QObject
{
    Q_OBJECT

public:
    enum class Mode { Live, Studio, Simulated, NothingPaired, Released };
    Q_ENUM(Mode)

    struct Item {
        int key = 0;                  ///< int(BtleSensorRole), or kZwiftClickKey
        QString name;
        QString device;
        SensorConnector::Status status = SensorConnector::Status::Searching;
    };

    static constexpr int kZwiftClickKey = 1000;

    explicit DeviceSession(Account *account, QObject *parent = nullptr);
    ~DeviceSession() override;

    /// Re-reads the saved sensors and settings; reconnects only when they changed.
    void reconfigure();
    /// Lets go of every device (another app can then take the trainer) until
    /// reconnect() or the next reconfigure() that finds new settings.
    void release();
    void reconnect();
    /// While pairing on the Devices page every device is let go, then reconnected.
    void setPaused(bool paused);
    bool isPaused() const { return m_paused; }
    void retry(int key);

    /// During a ride the Click is searched for continuously; otherwise in bursts.
    void setRideActive(bool active);
    /// Leaves ERG for a flat road, so a closed workout does not hold the last target.
    void returnTrainerToFreeRide();

    Mode mode() const { return m_mode; }
    QVector<Item> items() const;
    SensorConnector *connector() const { return m_connector; }
    ZwiftClickManager *clickManager() const;
    QMap<BtleSensorRole, BtleHub*> connectedHubs() const;
    BtleHub *trainerHub() const;

signals:
    /// The set of devices changed (rebuild the chips).
    void itemsChanged();
    void statusChanged(int key, SensorConnector::Status status);
    void hubConnected(BtleHub *hub);
    void modeChanged(DeviceSession::Mode mode);

private:
    QString configSignature() const;
    Mode wantedMode() const;
    void teardown();
    void build();
    void setMode(Mode mode);
    void maintain();
    void clickBurst();
    SensorConnector::Status clickStatus() const;

    Account                       *m_account = nullptr;
    SensorConnector               *m_connector = nullptr;
    QPointer<ZwiftClickManager>    m_click;
    QTimer                        *m_maintenance = nullptr;
    QTimer                        *m_clickBurstEnd = nullptr;
    Mode                           m_mode = Mode::NothingPaired;
    QString                        m_signature;
    bool                           m_released = false;
    bool                           m_paused = false;
    bool                           m_rideActive = false;
    int                            m_ticks = 0;
    QHash<int, QElapsedTimer>      m_waitingSince;   ///< per key, while not connected

    static constexpr int kMaintenanceMs = 30000;
    static constexpr int kStuckMs = 45000;
    static constexpr int kClickBurstEveryTicks = 4;   ///< every 2 min while idle
    static constexpr int kClickBurstMs = 20000;
};

#endif // DEVICE_SESSION_H
