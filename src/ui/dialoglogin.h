#ifndef DIALOGLOGIN_H
#define DIALOGLOGIN_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMovie>
#include <QNetworkReply>
#include <QPushButton>
#include <QTimer>

#include "account.h"
#include "settings.h"

class IntervalsIcuOAuthFlow;

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

    /// Called when the user clicks "Sign in with Intervals.icu".
    void onLoginWithIntervalsIcuClicked();

    // ── System-browser OAuth flow callbacks ─────────────────────────────────
    void onOAuthSucceeded();
    void onOAuthFailed();
    void onOAuthCancelRequested();

    // ── Account switching ────────────────────────────────────────────────────
    void onSwitchAccountClicked();
    void onUseDifferentAccountClicked(); ///< Alias for the loading-page link.

    // ── Silent session restore ───────────────────────────────────────────────
    void onSilentAuthFinished();
    void onTokenRefreshFinished();
    void onSilentAuthTimeout();

    void on_checkBox_workOffline_clicked(bool checked);
    void on_pushButton_startOffline_clicked();

#ifdef Q_OS_WASM
    /// Opens a browser popup to the Intervals.icu OAuth authorization page.
    void onWasmOAuthLoginClicked();
    /// Called (via QMetaObject) when the OAuth popup posts the authorization code back.
    void onWasmOAuthCodeReceived(const QString &code, const QString &state);
    /// Called when the server's token-exchange reply finishes.
    void onWasmOAuthTokenExchangeFinished();
#endif

signals:
    /// Emitted when the Intervals.icu OAuth browser-wait page becomes active.
    /// Integration tests connect to this to detect the start of the OAuth flow.
    void intervalsIcuOAuthStarted();

private:
    void loginOffline();

    /// Fetch the athlete profile and training zones from Intervals.icu using
    /// the OAuth2 Bearer token. Switches to the loading page (index 2).
    void fetchIntervalsIcuDataOAuth();

    /// Provision and complete the login using an Intervals.icu OAuth identity.
    void loginWithIntervalsIcuIdentity();

    /// Final step: accept the dialog and hand control back to MainWindow.
    void completeLogin();

    /// Switch to the login form page (index 0) and show/hide the session-
    /// expired notice.
    void showLoginForm(bool showExpiredMessage);

    /// Clear OAuth tokens from CredentialStore and Account in-memory state.
    void clearTokens();

    /// Abort and discard the current system-browser OAuth flow, if any.
    void resetOAuthFlow();

    Ui::DialogLogin          *ui;

    Account  *account;
    Settings *settings;

    QMovie *movie;

    IntervalsIcuOAuthFlow *m_oauthFlow = nullptr;

    QNetworkReply *replyVersion;
    QNetworkReply *replyIntervalsIcuAthlete;
    QNetworkReply *replyIntervalsIcuSettings;
    QNetworkReply *m_silentAuthReply;
    QNetworkReply *m_tokenRefreshReply;

    QTimer *m_versionTimeout;
    QTimer *m_intervalsIcuTimeout;
    QTimer *m_silentAuthTimeout;

    bool gotUpdateDialog;
    bool m_testMode = false;
    bool m_silentAuthCancelled = false;
    int  m_pendingIntervalsIcuReplies;

    bool m_loggingInViaIntervalsIcu = false;

#ifdef Q_OS_WASM
    QString        m_wasmOAuthState;           ///< CSRF state for the current OAuth popup.
    QNetworkReply *m_wasmTokenReply = nullptr; ///< In-flight token exchange request.
#endif
};

#endif // DIALOGLOGIN_H
