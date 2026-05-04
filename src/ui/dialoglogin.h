#ifndef DIALOGLOGIN_H
#define DIALOGLOGIN_H

#include <QDialog>
#include <QLabel>
#include <QMovie>
#include <QNetworkReply>
#include <QTimer>


#include <QtWebEngineWidgets/QWebEngineView>

#include "account.h"
#include "settings.h"

class DialogInfoWebView;


namespace Ui {
class DialogLogin;
}

class DialogLogin : public QDialog
{
    Q_OBJECT

public:
    /// @param testMode  When true the constructor skips all network requests
    ///                  and immediately shows widget_bottom in an "offline
    ///                  ready" state.  Only use this in unit/integration tests.
    explicit DialogLogin(QWidget *parent = nullptr, bool testMode = false);
    //    ~DialogLogin();


    void changeEvent(QEvent *event);


    bool getGotUpdate() {
        return this->gotUpdateDialog;
    }




public slots:
    //void provideAuthentication(QNetworkReply*,QAuthenticator*);

    void clearLstUsername();




private slots:
    void showLoadingProgress(int prog);


    void loginLoaded(bool ok);

    void slotFinishedGoogle();
    void slotFinishedGetVersion();
    void onVersionTimeout();
    void onGoogleTimeout();

    void slotFinishedIntervalsIcuAthlete();
    void slotFinishedIntervalsIcuSettings();
    void onIntervalsIcuTimeout();

    /// Called when the user clicks "Login with Intervals.icu".
    void onLoginWithIntervalsIcuClicked();
    /// Called when the Intervals.icu OAuth2 dialog reports success or failure.
    void onIntervalsIcuOAuthLinked(bool linked);
    /// Called when the Intervals.icu OAuth2 dialog is rejected (user closed it).
    void onIntervalsIcuOAuthDialogRejected();

    void on_comboBox_language_currentIndexChanged(int index);
    void on_checkBox_autoLogin_clicked(bool checked);
    void on_checkBox_workOffline_clicked(bool checked);
    void on_pushButton_startOffline_clicked();


private:
    void loginOffline();

    /// Trigger an Intervals.icu athlete-profile fetch immediately after a
    /// successful maximumtrainer.com authentication, if the user has stored
    /// their Intervals.icu credentials.  Falls through to completeLogin() if
    /// no credentials are configured.
    void fetchIntervalsIcuData();

    /// Fetch the athlete profile and training zones from Intervals.icu using
    /// the OAuth2 Bearer token obtained during the OAuth login flow.
    /// Called after a successful Intervals.icu OAuth2 authorization.
    void fetchIntervalsIcuDataOAuth();

    /// Provision and complete the login using an Intervals.icu OAuth identity.
    /// Sets up the Account object and loads any previously saved local data.
    void loginWithIntervalsIcuIdentity();

    /// Final step of the login flow: accept the dialog and hand control back
    /// to MainWindow.
    void completeLogin();

    Ui::DialogLogin *ui;
    QTranslator     m_translator;   /**< contains the translations for this application */
    QString         m_currLang;     /**< contains the currently loaded language */


    Account *account;
    Settings *settings;
    QString last_ip_tmp;
    bool firstLogin;


    QMovie* movie;  //loading gif

    QNetworkReply *replyGoogle;
    QNetworkReply *replyVersion;
    QNetworkReply *replyGetAccount;
    QNetworkReply *replyIntervalsIcuAthlete;
    QNetworkReply *replyIntervalsIcuSettings;
    QTimer        *m_versionTimeout;
    QTimer        *m_googleTimeout;
    QTimer        *m_intervalsIcuTimeout;

    bool gotUpdateDialog;
    int  m_pendingIntervalsIcuReplies;  ///< how many Intervals.icu replies we are still waiting for

    /// Keeps track of whether the current in-progress login is from the
    /// Intervals.icu OAuth2 flow (as opposed to the MaximumTrainer.com flow).
    bool m_loggingInViaIntervalsIcu = false;

};

#endif // DIALOGLOGIN_H
