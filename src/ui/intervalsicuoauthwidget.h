#ifndef INTERVALSICUOAUTHWIDGET_H
#define INTERVALSICUOAUTHWIDGET_H

#include <QWidget>
#include <QNetworkReply>
#include <QWebEngineView>

/// Self-contained widget that runs the Intervals.icu OAuth2 Authorization Code
/// flow inside an embedded QWebEngineView.
///
/// Usage:
///   1. Create and embed in a layout.
///   2. Call startAuth(authUrl, csrfState) to load the authorization page.
///   3. Connect authSucceeded() / authFailed() to react to the result.
///   4. Connect cancelRequested() if you need to handle the user pressing Cancel.
///
/// The widget handles both token-exchange paths:
///   - Server-side (MaximumTrainer.com backend returns JSON body):
///     reads the page body directly.
///   - Client-side fallback (redirect still carries ?code=):
///     POSTs to the Intervals.icu token endpoint via ExtRequest.
///
/// On success the Account object's intervals_icu_access_token and
/// intervals_icu_refresh_token are populated (via Util::parseJsonIntervalsIcuOAuthToken).
class IntervalsIcuOAuthWidget : public QWidget
{
    Q_OBJECT

public:
    explicit IntervalsIcuOAuthWidget(QWidget *parent = nullptr);
    ~IntervalsIcuOAuthWidget() override;

    /// Load the authorization URL and begin the OAuth flow.
    /// @param authUrl   Full authorization URL (from Environnement::getURLIntervalsIcuAuthorize).
    /// @param csrfState Per-request CSRF state token that was embedded in @p authUrl.
    void startAuth(const QString &authUrl, const QString &csrfState);

    /// Reset the widget to a blank/idle state (abort any in-flight requests).
    void reset();

public slots:
    /// Abort the current OAuth flow and emit cancelRequested().
    /// Safe to call from test code or UI "Back" buttons.
    void cancel();

signals:
    void authSucceeded();
    void authFailed();
    void cancelRequested();

private slots:
    void onPageLoaded(bool ok);
    void onTokenExchangeFinished();

private:
    QString buildErrorPageHtml(const QString &failedUrl) const;

    QWebEngineView *m_webView       = nullptr;
    QNetworkReply  *m_tokenReply    = nullptr;
    QString         m_csrfState;
    bool            m_showingError  = false;
    bool            m_settled       = false; ///< true after authSucceeded/authFailed emitted
};

#endif // INTERVALSICUOAUTHWIDGET_H
