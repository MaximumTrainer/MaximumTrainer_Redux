#ifndef TRAINER_CLICK_PROBE_H
#define TRAINER_CLICK_PROBE_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyService>

class QTimer;
class ZwiftClickRelay;

/*
 * TrainerClickProbe — CLI experiment (issue #300 follow-up).
 *
 * Connects to the trainer ONCE and proves that the trainer-relayed Zwift Click
 * and FTMS ERG coexist, using the PROVEN ZwiftClickRelay (the shipped relay with
 * its watchdog/retry) armed AFTER FTMS control is granted — the ordering fix.
 *
 * It enables FTMS control (Request Control), then re-sends the current virtual
 * gear's resistance (0x04) every 3 s and logs the ACK (⇒ ERG alive). Once
 * control is granted it attaches ZwiftClickRelay to the FC82 service; paddle
 * presses shift the gear and push the new resistance (logged + ACK-confirmed),
 * other buttons log. A 30 s heartbeat reports relay/gear/press/ack state for a
 * long soak.
 */
class TrainerClickProbe : public QObject
{
    Q_OBJECT
public:
    explicit TrainerClickProbe(QObject *parent = nullptr);
    void start(const QString &nameFilter = QStringLiteral("Victory"), int runSeconds = 120);

signals:
    void finished();

private:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onDiscoveryFinished();
    void setupFtms();
    void requestFtmsControl();
    void sendResistance(int levelTenths);
    void onFtmsChanged(const QLowEnergyCharacteristic &c, const QByteArray &v);
    void armRelay();          // create ZwiftClickRelay + attach FC82 (after grant)
    void shift(int delta);

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    QLowEnergyController *m_ctrl = nullptr;
    QLowEnergyService    *m_ftms = nullptr;
    QLowEnergyService    *m_fc82 = nullptr;
    ZwiftClickRelay      *m_clickRelay = nullptr;
    QString  m_nameFilter;
    bool     m_ftmsGranted = false;
    bool     m_started = false;
    bool     m_relayActive = false;
    int      m_gear = 12;             // virtual gear 1..24 (paddles shift it)
    int      m_buttonPresses = 0;
    int      m_ergAcks = 0;
    int      m_logNextAcks = 0;       // log the next N 0x04 ACKs (right after a shift)
    QTimer  *m_ergTimer = nullptr;
    QTimer  *m_statusTimer = nullptr;
    QTimer  *m_runTimer = nullptr;
};

#endif // TRAINER_CLICK_PROBE_H
