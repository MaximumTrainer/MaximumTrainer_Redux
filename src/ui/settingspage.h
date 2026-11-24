#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include <QWidget>
#include <QNetworkReply>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>

#include "settings.h"
#include "account.h"


namespace Ui {
class SettingsPage;
}

class DialogConfig;
class QtMediaPlayer;
class QListWidgetItem;
class QVBoxLayout;

/// The Settings tab: every option of the app in one place, applied as soon as
/// it changes. The ride pages are the in-ride panel's own pages (DialogConfig).
class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);
    ~SettingsPage() override;

    enum class Section { Profile, Accounts, Ride, RideScreen, Sounds, Media, General, Logs, Help };
    void showSection(Section section);
    /// Stop the station being tried out (a ride has its own player).
    void stopRadio();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;



signals:
    /// Emitted when the athlete profile (FTP / LTHR / weight) was edited, so the
    /// main window can recompute zones and workout targets.
    void profileChanged();
    /// Emitted when Rowing mode (Beta) was toggled, so the main window can
    /// swap the Workout tab icon and the Devices tab's FTMS slot live.
    void rowingModeChanged();
    void logoutRequested();
    void keyboardShortcutsRequested();
    void userGuideRequested();
    void aboutRequested();


public slots:
    void stravaLinked(bool);
    /// Network state; Intervals.icu settings are disabled with a note while offline.
    void setOnlineMode(bool isOnline);


private slots:
    void currentListViewSelectionChanged(int section);

    void stravaLabelClicked();
    void unlinkStravaClicked();
    void stravaUnlinkFinished();


    /// Logging settings page slots
    void onLogFileEnabledToggled(bool checked);
    void onBrowseLogFileClicked();
    void onOpenLogFolderClicked();


private:
    void updateIntervalsAvailability();
    bool m_networkOnline = true;
    void initUI();
    /// Save everything on the app pages (called on each change).
    void apply();
    void addSection(Section section, const QString &iconPath, const QString &label,
                    const QList<QWidget *> &widgets, bool lastFillsPage = false);
    void addGroupTitle(const QString &title);
    QWidget *createAccountPage();
    QWidget *createSensorAlertsGroup();
    QWidget *createHelpPage();
    /// Show/refresh the "FTP and LTHR synced from intervals.icu" label.
    void updateProfileSyncLabel();
    /// Build the logging settings page and add it to the stacked widget.
    QWidget *createLoggingPage();
    /// Populate the logging-settings page controls from the current Logger state.
    void loadLoggingSettings();
    /// Persist the logging-settings page controls to Logger + QSettings.
    void saveLoggingSettings();


private:
    Ui::SettingsPage *ui;
    DialogConfig *m_rideConfig = nullptr;   // owns the ride pages' controls
    QtMediaPlayer *m_radioPlayer = nullptr;
    QPushButton *m_logoutButton = nullptr;
    QCheckBox *m_dropoutEnabled = nullptr;
    QSpinBox *m_dropoutTimeout = nullptr;
    QSpinBox *m_batteryThreshold = nullptr;

    Settings *settings;
    Account *account;

    QNetworkReply *replyStravaDeauthorization;



    // Logging settings page widgets (owned by the page widget, not directly by us)
    QComboBox   *m_comboLogLevel    = nullptr;
    QCheckBox   *m_checkFileLogging = nullptr;
    QLineEdit   *m_editLogFilePath  = nullptr;
    QPushButton *m_btnBrowseLog     = nullptr;
    QPushButton *m_btnOpenLog       = nullptr;
    QLabel      *m_labelLogPathHint = nullptr;

};

#endif // SETTINGSPAGE_H
