#ifndef TRAINER_GEAR_TEST_H
#define TRAINER_GEAR_TEST_H

#include <QObject>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QTimer>

class BtleHub;

/*
 * Headless virtual-shifting feel test, driven through BtleHub — the app's
 * PROVEN FTMS control path (solid LED, real resistance), NOT the standalone
 * Zwift harness (which never actuated). It auto-cycles a virtual gear so a
 * rider can FEEL the gears change while pedaling, with no source code or
 * keyboard needed on the trainer-side machine (issue #293).
 *
 * Launched via `--trainer-gear-test [name]`.
 */
class TrainerGearTest : public QObject
{
    Q_OBJECT
public:
    explicit TrainerGearTest(QObject *parent = nullptr);
    void start(const QString &nameFilter, int scanSeconds = 8);

signals:
    void finished();

private:
    void onGearTick();

    QBluetoothDeviceDiscoveryAgent *m_agent = nullptr;
    BtleHub *m_hub      = nullptr;
    QTimer  *m_tick     = nullptr;
    double   m_cadence  = -1.0;
    int      m_elapsed  = 0;
    int      m_lastGear = -1;
    bool     m_connecting = false;
    bool     m_started  = false;
};

#endif // TRAINER_GEAR_TEST_H
