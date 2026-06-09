#ifndef STUDIOWIDGET_H
#define STUDIOWIDGET_H

#include <QWidget>
#include <QVector>

#include "userstudio.h"
#include "btle_sensor_config.h"

class QCheckBox;
class QComboBox;
class QScrollArea;
class ToggleSwitch;
class QSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QGroupBox;
class QGridLayout;
class Account;

/*
 * StudioWidget
 *
 * Main-window page (a FancyTabBar tab) for Studio mode. Replaces the former
 * server-hosted QWebEngineView studio page (now dead). Built programmatically
 * like SensorsWidget.
 *
 * Exposes Enable Studio mode + a rider-count dropdown (1..kMaxRiders) that
 * drives N compact per-rider cards. Each card configures one rider: name
 * (default RiderN), FTP, LTHR, the five BLE sensor slots (persisted per rider
 * via BtleSensorStore) and a per-rider Smart Trainer ERG setting.
 *
 * Rider identity (name/FTP/LTHR) is persisted via the existing UserStudio XML
 * (XmlUtil); per-rider ERG is persisted in QSettings ("studioErg/riderN").
 * Studio-mode side effects (window title, Sensors-tab enable) stay in
 * MainWindow, reached via the signals below.
 */
class StudioWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StudioWidget(QWidget *parent = nullptr);

    /// Re-read persisted settings into the controls (e.g. when the tab is shown).
    void reload();

    /// Per-rider ERG settings, persisted in QSettings group "studioErg/riderN".
    /// Static so the workout-launch path in MainWindow reads the same keys.
    static bool studioErgControl(int riderIndex, bool defaultValue);
    static int  studioErgRamp(int riderIndex, int defaultValue);

signals:
    void studioModeChanged(bool enabled);
    void riderCountChanged(int nbRiders);
    /// Emitted when a rider's name/FTP/LTHR changes so MainWindow refreshes the
    /// in-memory vecUserStudio used to launch workouts.
    void ridersChanged(QVector<UserStudio> riders);

private slots:
    void onStudioModeToggled(bool enabled);
    void onRiderCountChanged(int index);
    void onExportClicked();
    void onImportClicked();

private:
    static constexpr int kMaxRiders = 15;

    struct SensorSlot {
        BtleSensorRole role = BtleSensorRole::HeartRate;
        QLabel      *deviceLabel = nullptr;
        QPushButton *clearButton = nullptr;
    };
    struct RiderCard {
        QGroupBox *box      = nullptr;
        QLineEdit *nameEdit = nullptr;
        QSpinBox  *ftpSpin  = nullptr;
        QSpinBox  *lthrSpin = nullptr;
        QVector<SensorSlot> sensorSlots;
        QCheckBox *ergCheck    = nullptr;
        QSpinBox  *ergRampSpin = nullptr;
    };

    void buildUi();
    QGroupBox *buildRiderCard(int riderIndex);     // riderIndex is 1-based
    void updateVisibleCards(int count);
    /// Show/hide everything except the Studio Mode switch (rider count, import/
    /// export, hint and the rider cards) so the page is empty when studio is off.
    void setStudioControlsVisible(bool visible);
    void loadRiderIntoCard(int riderIndex);
    void refreshSensorSlot(int riderIndex, int slotIdx);

    void onNameChanged(int riderIndex);
    void onFtpChanged(int riderIndex);
    void onLthrChanged(int riderIndex);
    void onScanClicked(int riderIndex, int slotIdx);
    void onClearClicked(int riderIndex, int slotIdx);
    void onErgChanged(int riderIndex);

    void persistRiders();
    static QString ergGroup(int riderIndex);
    static void    saveErg(int riderIndex, bool control, int ramp);

    Account      *m_account         = nullptr;
    ToggleSwitch *m_enableSwitch     = nullptr;
    QComboBox    *m_riderCountCombo  = nullptr;
    QWidget      *m_controlsRow      = nullptr;
    QLabel       *m_riderCountLabel  = nullptr;
    QPushButton  *m_importButton     = nullptr;
    QPushButton  *m_exportButton     = nullptr;
    QLabel       *m_noteLabel        = nullptr;
    QScrollArea  *m_scrollArea       = nullptr;
    QGridLayout *m_cardsGrid        = nullptr;
    QVector<RiderCard>  m_cards;     // index 0 == rider 1
    QVector<UserStudio> m_riders;    // full vector from the UserStudio XML
};

#endif // STUDIOWIDGET_H
