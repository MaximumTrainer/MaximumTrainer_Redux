#ifndef TRAINER_CLICK_PROBE_H
#define TRAINER_CLICK_PROBE_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QLowEnergyController>
#include <QLowEnergyService>

class QTimer;

/*
 * TrainerClickProbe — experiment (issue #300 follow-up).
 *
 * Connects to the trainer ONCE and runs FTMS control and the FC82 Click relay on
 * the same connection, to answer: can we read the trainer-relayed Zwift Click
 * WITHOUT breaking FTMS ERG? The Wireshark capture showed Zwift drives the
 * trainer entirely over FC82 (never FTMS) and the relay only streamed after the
 * host wrote RideOn + ZCS setup. This probe sweeps which of those writes are
 * actually needed and whether each keeps FTMS control alive.
 *
 * It always: enables FTMS control (Request Control), then periodically sends a
 * Set Target Resistance Level (0x04) and logs whether the trainer grants/acks
 * (⇒ ERG is alive) or times out (⇒ FTMS is blocked). And it subscribes the FC82
 * notify char and logs any relayed 0x4e Click button frames.
 *
 * Modes (CLI flags), to find the minimal recipe that relays the Click yet keeps
 * ERG:
 *   passive  (default) — subscribe FC82 only, write nothing to it.
 *   --zcs              — also send the ZCS relay setup + 441002 (connect Click),
 *                        but NOT RideOn (tests if RideOn is the FTMS-killer).
 *   --rideon           — also send RideOn first (the full original arming).
 */
class TrainerClickProbe : public QObject
{
    Q_OBJECT
public:
    explicit TrainerClickProbe(QObject *parent = nullptr);

    enum class Relay { Passive, Zcs, RideOn };
    void start(const QString &nameFilter = QStringLiteral("Victory"),
               int runSeconds = 120, Relay relay = Relay::Passive);

signals:
    void finished();

private:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onDiscoveryFinished();
    void setupFtms();
    void setupFc82();
    void requestFtmsControl();
    void sendResistance(int levelTenths);
    void onFtmsChanged(const QLowEnergyCharacteristic &c, const QByteArray &v);
    void onFc82Changed(const QLowEnergyCharacteristic &c, const QByteArray &v);
    void writeFc82(const QByteArray &bytes);
    void armRelay();   // send the ZCS / RideOn writes per the selected mode

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    QLowEnergyController *m_ctrl = nullptr;
    QLowEnergyService    *m_ftms = nullptr;
    QLowEnergyService    *m_fc82 = nullptr;
    QString  m_nameFilter;
    Relay    m_relay = Relay::Passive;
    bool     m_ftmsGranted = false;
    bool     m_started = false;
    quint32  m_lastBitmap = 0xFFFFFFFFu;
    QTimer  *m_ergTimer = nullptr;
    QTimer  *m_runTimer = nullptr;
};

#endif // TRAINER_CLICK_PROBE_H
