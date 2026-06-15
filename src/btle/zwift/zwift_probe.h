#ifndef ZWIFT_PROBE_H
#define ZWIFT_PROBE_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothUuid>
#include <QLowEnergyController>
#include <QLowEnergyService>
#include <QLowEnergyCharacteristic>
#include <QQueue>
#include <QTimer>
#include <QVector>

/*
 * Phase 1 read-only BLE exploration harness for the Zwift virtual-shifting work
 * (issue #293). Headless console tool, launched via `--zwift-probe`.
 *
 * What it does (and deliberately does NOT do):
 *   - scans LE, logs every device (name / address / advertised services);
 *   - connects to candidates (name filter, or anything advertising the Zwift
 *     trainer service), dumps the full GATT table;
 *   - subscribes to every notify/indicate characteristic and logs raw frames,
 *     decoding 0x03 riding-data via the Phase 0 codec;
 *   - writes ONLY the documented "RideOn" handshake to the Zwift control point
 *     so the trainer starts streaming. It never sends a 0x04 control command,
 *     so it cannot change resistance — purely observational.
 *
 * Goal: confirm the service/characteristic layout, the handshake's trailing
 * bytes, the (assumed) lack of encryption, and the Zwift Click button frames,
 * before any control code is written in Phase 2.
 */
class ZwiftProbe : public QObject
{
    Q_OBJECT
public:
    explicit ZwiftProbe(QObject *parent = nullptr);

    // Optional scripted write sequence run after the handshake (Phase 2).
    //   ErgDemo   — ERG power steps (proves control; no pedaling needed).
    //   GearSweep — hold a grade, auto-step the virtual gear up/down so a rider
    //               can FEEL each gear without touching any input.
    enum class Script { None, ErgDemo, GearSweep };

    // nameFilter: case-insensitive substring; empty => auto (Zwift service or a
    // name hinting at a trainer/Click). listenSeconds: per-device capture window.
    void start(const QString &nameFilter = QString(),
               int scanSeconds = 8, int listenSeconds = 15,
               Script script = Script::None);

signals:
    void finished();

private slots:
    void onDeviceDiscovered(const QBluetoothDeviceInfo &info);
    void onScanFinished();
    void onControlStep();

private:
    void connectNextCandidate();
    void dumpService(QLowEnergyService *service);
    void subscribeAndProbe(QLowEnergyService *service);
    void beginControlScript(QLowEnergyService *zwiftService);
    void finishCurrentDevice();
    void teardownController();

    static bool looksInteresting(const QBluetoothDeviceInfo &info,
                                 const QString &nameFilter);
    static QString propsToString(QLowEnergyCharacteristic::PropertyTypes p);

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    QString                         m_nameFilter;
    int                             m_listenSeconds = 15;

    QQueue<QBluetoothDeviceInfo>    m_candidates;
    QBluetoothDeviceInfo            m_current;
    QLowEnergyController           *m_controller = nullptr;
    QList<QLowEnergyService *>      m_services;
    QTimer                         *m_listenTimer = nullptr;
    bool                            m_zwiftServiceSeen = false;
    bool                            m_scanHandled = false;
    int                             m_suppressedCount = 0;
    int                             m_retriesLeft = 0;

    // Phase 2 control script (resistance-changing writes).
    Script                          m_script = Script::None;
    bool                            m_controlStarted = false;
    QLowEnergyService              *m_ctrlService = nullptr;
    QVector<QPair<QString, QByteArray>> m_ctrlSteps;
    int                             m_ctrlIndex = 0;
    int                             m_ctrlIntervalMs = 4500;
    QTimer                         *m_ctrlTimer = nullptr;
};

#endif // ZWIFT_PROBE_H
