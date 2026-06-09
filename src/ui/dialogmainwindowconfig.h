#ifndef DIALOGMAINWINDOWCONFIG_H
#define DIALOGMAINWINDOWCONFIG_H

#include <QDialog>
#include <QNetworkReply>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>

#include "settings.h"
#include "account.h"
#include "intervalsicuservice.h"


namespace Ui {
class DialogMainWindowConfig;
}

class DialogMainWindowConfig : public QDialog
{
    Q_OBJECT

public:
    explicit DialogMainWindowConfig(QWidget *parent = 0);
    ~DialogMainWindowConfig();

    void accept();
    void reject();

protected:
    void showEvent(QShowEvent *event) override;



signals:
    void folderWorkoutChanged();
    /// Emitted when Intervals.icu credentials are saved so the tab can refresh.
    void intervalsIcuCredentialsChanged();
    /// Emitted when the athlete profile (FTP / LTHR / weight) was edited, so the
    /// main window can recompute zones and workout targets.
    void profileChanged();


public slots:
    void stravaLinked(bool);
    /// Update the Intervals.icu section visibility based on network state.
    /// When \a isOnline is false the groupBox_intervals is hidden so the
    /// user cannot attempt to test credentials that will time out.
    void setOnlineMode(bool isOnline);


private slots:
    void currentListViewSelectionChanged(int section);
    void on_pushButton_browseWorkoutDir_clicked();
    void on_pushButton_browseHistoryDir_clicked();

    void stravaLabelClicked();
    void unlinkStravaClicked();
    void stravaUnlinkFinished();

    void onTestIntervalsConnectionClicked();
    void onTestIntervalsConnectionFinished();

    /// Logging settings page slots
    void onLogFileEnabledToggled(bool checked);
    void onBrowseLogFileClicked();
    void onOpenLogFileClicked();


private:
    void initUI();
    /// Build the logging settings page and add it to the stacked widget.
    QWidget *createLoggingPage();
    /// Populate the logging-settings page controls from the current Logger state.
    void loadLoggingSettings();
    /// Persist the logging-settings page controls to Logger + QSettings.
    void saveLoggingSettings();


private:
    Ui::DialogMainWindowConfig *ui;

    Settings *settings;
    Account *account;

    QNetworkReply *replyStravaDeauthorization;

    QNetworkReply *replyIntervalsTest = nullptr;
    IntervalsIcuService *m_intervalsService = nullptr;


    // Logging settings page widgets (owned by the page widget, not directly by us)
    QComboBox   *m_comboLogLevel    = nullptr;
    QCheckBox   *m_checkFileLogging = nullptr;
    QLineEdit   *m_editLogFilePath  = nullptr;
    QPushButton *m_btnBrowseLog     = nullptr;
    QPushButton *m_btnOpenLog       = nullptr;
    QLabel      *m_labelLogPathHint = nullptr;

};

#endif // DIALOGMAINWINDOWCONFIG_H
