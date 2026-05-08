#ifndef DIALOGLOGIN_H
#define DIALOGLOGIN_H

#include <QDialog>
#include <QLabel>
#include <QMovie>
#include <QNetworkReply>
#include <QTimer>

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
    ///                  and immediately shows the form in an "offline
    ///                  ready" state.  Only use this in unit/integration tests.
    explicit DialogLogin(QWidget *parent = nullptr, bool testMode = false);

    void changeEvent(QEvent *event);

    bool getGotUpdate() {
        return this->gotUpdateDialog;
    }

private slots:
    void slotFinishedGetVersion();
    void onVersionTimeout();

    void slotFinishedIntervalsIcuAthlete();
    void slotFinishedIntervalsIcuSettings();

    /// Called when the user clicks "Login with Intervals.icu" or presses
    /// Return in one of the form fields.
    void onLoginWithIntervalsIcuClicked();
    /// Called when the Intervals.icu OAuth2 dialog reports success or failure.
    void onIntervalsIcuOAuthLinked(bool linked);
    /// Called when the Intervals.icu OAuth2 dialog is rejected (user closed it).
    void onIntervalsIcuOAuthDialogRejected();

    void on_comboBox_language_currentIndexChanged(int index);
    void on_checkBox_autoLogin_clicked(bool checked);
    void on_checkBox_workOffline_clicked(bool checked);
    void on_pushButton_startOffline_clicked();

signals:
    /// Emitted synchronously just before the Intervals.icu OAuth2 dialog
    /// calls exec().  Integration tests connect to this to detect and
    /// dismiss the dialog without relying on post-exec() state.
    void intervalsIcuOAuthDialogCreated(DialogInfoWebView *dialog);

private:
    void loginOffline();

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

    QMovie* movie;  //loading gif

    QNetworkReply *replyVersion;
    QNetworkReply *replyIntervalsIcuAthlete;
    QNetworkReply *replyIntervalsIcuSettings;
    QTimer        *m_versionTimeout;
    QTimer        *m_intervalsIcuTimeout;

    bool gotUpdateDialog;
    int  m_pendingIntervalsIcuReplies;  ///< how many Intervals.icu replies we are still waiting for

    /// Keeps track of whether the current in-progress login is from the
    /// Intervals.icu OAuth2 flow.
    bool m_loggingInViaIntervalsIcu = false;

};

#endif // DIALOGLOGIN_H
