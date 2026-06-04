#ifndef SENSORSWIDGET_H
#define SENSORSWIDGET_H

#include <QWidget>
#include <QVector>

#include "btle_sensor_config.h"

class QLabel;
class QPushButton;

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

private slots:
    void onScanClicked(int rowIndex);
    void onClearClicked(int rowIndex);

private:
    struct SlotRow {
        BtleSensorRole  role;
        QLabel         *deviceLabel = nullptr;
        QPushButton    *scanButton  = nullptr;
        QPushButton    *clearButton = nullptr;
        BtleSavedSensor current;
    };

    void buildUi();
    void refreshRow(int rowIndex);

    QVector<SlotRow> m_rows;
};

#endif // SENSORSWIDGET_H
