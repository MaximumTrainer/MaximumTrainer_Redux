#ifndef SENSOR_CONNECT_DIALOG_H
#define SENSOR_CONNECT_DIALOG_H

#include <QDialog>
#include <QMap>

#include "btle_sensor_config.h"
#include "sensor_connector.h"

class BtleHub;
class QLabel;
class QPushButton;
class QTimer;

/*
 * SensorConnectDialog
 *
 * Pre-workout connection screen used by Studio mode (one per rider): a live
 * status per saved sensor, driven by a SensorConnector. Solo rides connect
 * inside the workout window instead (SensorStatusStrip).
 *
 * On accept the caller takes the connected hubs via connectedHubs() and calls
 * detachHubs() to assume ownership; otherwise the dialog tears them down.
 */
class SensorConnectDialog : public QDialog
{
    Q_OBJECT

public:
    SensorConnectDialog(const QMap<BtleSensorRole, BtleSavedSensor> &savedSensors,
                        QWidget *parent = nullptr);

    /// Connected hubs keyed by role. Two roles may share one BtleHub* when the
    /// same physical device is saved for both (e.g. Trainer + Power).
    QMap<BtleSensorRole, BtleHub*> connectedHubs() const;

    /// Transfer hub ownership to the caller so the dialog leaves them alive.
    void detachHubs();

private:
    void updateStatusLabel(BtleSensorRole role, SensorConnector::Status status);
    void onContinueClicked();

    SensorConnector                 *m_connector = nullptr;
    QMap<BtleSensorRole, QLabel*>    m_statusLabels;
    QPushButton                     *m_btnContinue = nullptr;
    QTimer                          *m_autoContinueTimer = nullptr;

    static constexpr int AUTO_CONTINUE_MS = 5000;  // grace after the scan ends
};

#endif // SENSOR_CONNECT_DIALOG_H
