#ifndef SIMULATOR_HUB_H
#define SIMULATOR_HUB_H

#include <QObject>
#include <QTimer>

/*
 * SimulatorHub
 *
 * Emits the same sensor-data signals as BtleHub at regular intervals using
 * generated values, allowing the full workout data pipeline to be exercised
 * without any physical Bluetooth hardware.
 *
 * Values drift gradually within realistic ranges:
 *   HR      : ~140 bpm  (±15)
 *   Cadence : ~90 rpm   (±10)
 *   Speed   : ~28 km/h  (±5)
 *   Power   : ~200 W    (±30)
 *   SmO2    : ~65 %     (±15)
 *   tHb     : ~13 g/dL  (±2)
 */
class SimulatorHub : public QObject
{
    Q_OBJECT

public:
    explicit SimulatorHub(QObject *parent = nullptr);
    ~SimulatorHub() override = default;

    void start();
    void stop();

    /// Set the userID emitted in every signal (default: 1).
    /// WorkoutDialog expects 1-based userIDs; in Studio Mode each rider's hub
    /// must use a distinct ID (1…N) so signals route to the correct widget.
    void setUserID(int id) { m_userID = id; }

signals:
    // Same signatures as BtleHub / Hub so MainWindow can wire them identically
    void signal_hr(int userID, int hr);
    void signal_cadence(int userID, int cadence);
    void signal_speed(int userID, double speed);   // km/h
    void signal_power(int userID, int power);      // watts
    void signal_balance(int userID, int rightPedalPercentage); // L/R pedal balance, right pedal %
    void signal_pedal(int userID, double leftTorqueEff, double rightTorqueEff,
                      double leftPedalSmooth, double rightPedalSmooth, double combinedPedalSmooth);
    void signal_oxygen(int userID, double smo2Percent, double thbGdL);

public slots:
    // No-op stubs matching BtleHub/Hub slot signatures
    void setLoad(int deviceId, double watts);
    void setSlope(int deviceId, double grade);
    void stopDecodingMsg();

private slots:
    void tick();

private:
    QTimer *m_timer = nullptr;

    // Current simulated values
    double m_hr      = 140.0;
    double m_cadence =  90.0;
    double m_speed   =  28.0;
    double m_power   = 200.0;
    double m_smo2    =  65.0;   // SmO2 % – typical mid-exercise value
    double m_thb     =  13.0;   // tHb g/dL – typical value
    double m_balance =  48.0;   // right pedal % (left slightly stronger, typical)
    double m_torqueEffL = 82.0; // torque effectiveness % (left)
    double m_torqueEffR = 79.0; // torque effectiveness % (right)
    double m_smoothL    = 24.0; // pedal smoothness % (left)
    double m_smoothR    = 23.0; // pedal smoothness % (right)

    // Drift direction per channel (+1 / -1)
    int m_hrDir      = 1;
    int m_cadenceDir = 1;
    int m_speedDir   = 1;
    int m_powerDir   = 1;
    int m_smo2Dir    = 1;
    int m_thbDir     = 1;
    int m_balanceDir = 1;
    int m_torqueEffLDir = 1;
    int m_torqueEffRDir = 1;
    int m_smoothLDir    = 1;
    int m_smoothRDir    = 1;

    int m_userID     = 1;    ///< userID used in all emitted signals (1-based; matches WorkoutDialog's 1-based arrays)
};

#endif // SIMULATOR_HUB_H
