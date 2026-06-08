#ifndef DIALOGINFOWEBVIEW_H
#define DIALOGINFOWEBVIEW_H

#include <QDialog>
#include <QNetworkReply>
#include <QUrl>

namespace Ui {
class DialogInfoWebView;
}

class DialogInfoWebView : public QDialog
{
    Q_OBJECT

public:
    explicit DialogInfoWebView(QWidget *parent = 0);
    ~DialogInfoWebView();


    void setUrlWebView(QString url);
    void setTitle(QString title);
    void setUsedForStrava(bool used);
    /// Configure this dialog as an Intervals.icu OAuth2 flow.
    /// When set, the dialog watches for the unified github.io
    /// /oauth_callback.html redirect and exchanges the authorization code
    /// for a token pair via the Cloudflare CORS proxy.
    void setUsedForIntervalsIcu(bool used);
    /// Store the CSRF state token that was included in the authorization URL.
    /// When set, the state is validated against the value returned in the
    /// redirect URL (client-side fallback path) before the code is exchanged.
    void setExpectedOAuthState(const QString &state);


signals:
    void stravaLinked(bool linked);
    /// Emitted when the Intervals.icu OAuth2 flow completes (or fails).
    void intervalsIcuLinked(bool linked);




private slots:
    void pageLoaded(bool ok);
    void onLoadStarted();
    /// Handles the direct token-exchange reply when the app falls back to
    /// exchanging the authorization code client-side.
    void slotIntervalsTokenExchangeFinished();

private:
    /// Build a user-friendly error page HTML for OAuth flow failures.
    QString buildErrorPageHtml(const QString &failedUrl) const;

private:
    Ui::DialogInfoWebView *ui;


    bool usedForStrava;
    bool usedForIntervalsIcu;
    QString m_expectedOAuthState; ///< CSRF state token sent in the authorize URL
    QString emailUser;

    /// Pending reply for the client-side Intervals.icu token exchange (fallback).
    QNetworkReply *m_intervalsTokenReply = nullptr;

    /// Guard flag set while an error-page HTML is being loaded into the webview
    /// to prevent the resulting pageLoaded(true) callback from re-entering the
    /// normal OAuth-callback handling logic.
    bool m_showingErrorPage = false;
};

#endif // DIALOGINFOWEBVIEW_H
