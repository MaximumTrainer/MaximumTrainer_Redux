#ifndef ZWIFT_CLICK_TEST_H
#define ZWIFT_CLICK_TEST_H

#include <QObject>

class ZwiftClickManager;
class QTimer;

/*
 * Headless end-to-end test for Zwift Click v2 controller input
 * (--zwift-click-test [nameFilter]). Drives a ZwiftClickManager and prints the
 * mapped action for every button: paddles move a virtual gear (1..24), and the
 * d-pad / A-B-Y-Z print their assigned actions (radio, difficulty, start/pause,
 * lap). Proves the manager + full button map on hardware before app wiring.
 */
class ZwiftClickTest : public QObject
{
    Q_OBJECT
public:
    explicit ZwiftClickTest(QObject *parent = nullptr);

    void start(const QString &nameFilter = QString(),
               int scanSeconds = 8, int runSeconds = 120);

signals:
    void finished();

private:
    ZwiftClickManager *m_manager = nullptr;
    QTimer            *m_runTimer = nullptr;
    int                m_runSeconds = 120;
    int                m_gear = 12;   // demo virtual gear 1..24 (harness display only)
};

#endif // ZWIFT_CLICK_TEST_H
