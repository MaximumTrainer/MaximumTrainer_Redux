#ifndef SENSORSWIDGET_H
#define SENSORSWIDGET_H

#include <QWidget>
#include <QVector>

#include "btle_sensor_config.h"

class QLabel;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QGroupBox;
class QVBoxLayout;
class Account;

/*
 * SensorsWidget
 *
 * Main-window page (a FancyTabBar tab) that lets the user save one BLE device
 * per sensor role (Heart Rate, Power, Cadence/Speed, Trainer, Oxygen). Built
 * programmatically like HistoryWidget. Changes are persisted immediately via
 * BtleSensorStore — there is no OK/Cancel since this is a page, not a dialog.
 *
 * Desktop only – on WASM the constructor builds a simple "not available" page.
 */
class SensorsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SensorsWidget(QWidget *parent = nullptr);

    /// Re-read persisted settings into the rows (e.g. when the tab is shown).
    void reload();

signals:
    /// Around a pairing scan: the app lets go of its devices meanwhile.
    void pairingStarted();
    void pairingFinished();
    /// A saved sensor or a device setting changed.
    void devicesChanged();

private slots:
    void onScanClicked(int rowIndex);
    void onClearClicked(int rowIndex);
    void onControlResistanceToggled(bool checked);
    void onVirtualShiftingToggled(bool checked);
    void onUseZwiftClickToggled(bool checked);
    void onSimulatedSensorsToggled(bool checked);
    void addSimulatedSensorsGroup(QVBoxLayout *layout);
    void onErgRampChanged(int seconds);

private:
    struct SlotRow {
        BtleSensorRole  role;
        QLabel         *roleLabel   = nullptr;
        QLabel         *deviceLabel = nullptr;
        QPushButton    *scanButton  = nullptr;
        QPushButton    *clearButton = nullptr;
        BtleSavedSensor current;
    };

    void buildUi();
    /// Show the Trainer or Rower FTMS slot to match account->rowing_mode
    /// (both stay built with their own saved pairing). Safe to call live.
public:
    void refreshRowingMode();
private:
    void refreshRow(int rowIndex);
    /// True if the Trainer slot currently holds a valid saved device.
    bool trainerPaired() const;

    QVector<SlotRow> m_rows;
    QGroupBox *m_trainerGroup = nullptr;
    QWidget   *m_cyclingOnlyBox = nullptr;
    QCheckBox *m_controlResistanceCheck = nullptr;
    QCheckBox *m_virtualShiftingCheck = nullptr;
    QCheckBox *m_useZwiftClickCheck = nullptr;
    QCheckBox *m_simulatedSensorsCheck = nullptr;
    QLabel    *m_zwiftClickHint = nullptr;
    QSpinBox  *m_ergRampSpin            = nullptr;
    Account   *m_account = nullptr;
};

#endif // SENSORSWIDGET_H
