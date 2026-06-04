#ifndef WEBBROWSERVIEW_H
#define WEBBROWSERVIEW_H

#include <QWidget>
#include <QWebEngineFullScreenRequest>  // by-value slot param needs the complete type for moc

class QWebEngineView;
class QLineEdit;
class QToolBar;
class QTimer;
class QGridLayout;

/// In-frame web browser shown in the Workout dialog as an alternative to the
/// VLC video player (Preferences -> Video Player -> "WebView").  Wraps a
/// QWebEngineView with a minimal auto-hiding toolbar (navigation + address bar)
/// so the user can watch YouTube and other DRM-free sites during a workout.
///
/// Note: DRM-protected services (Netflix, Disney+, etc.) require the proprietary
/// Widevine module, which cannot be bundled in this open-source build, so they
/// will not play.  YouTube and other HTML5 sources work.
class WebBrowserView : public QWidget
{
    Q_OBJECT
public:
    explicit WebBrowserView(QWidget *parent = nullptr);
    ~WebBrowserView() override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    /// Loads the user's configured home page (QSettings "webBrowserWorkout").
    /// No-op after the first successful load so switching display modes during
    /// a workout does not reset the page the user navigated to.
    void loadHomePageIfNeeded();

public slots:
    void playVideo();
    void pauseVideo();

private slots:
    void changeLocation();
    void adjustLocation();
    void updateUrlOfLineEdit(const QUrl &url);
    void hideToolbar();

    /// Honors fullscreen requests coming from the page itself (e.g. clicking
    /// YouTube's fullscreen button). Keeps the view docked so the page's
    /// fullscreen element fills the web-view widget rather than the whole screen.
    void handleFullScreenRequested(QWebEngineFullScreenRequest request);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    QWebEngineView *webEngineView = nullptr;
    QLineEdit      *locationEdit  = nullptr;
    QToolBar       *toolBar       = nullptr;
    QGridLayout    *gLayout       = nullptr;
    QTimer         *timerHideToolbar = nullptr;

    bool    homePageLoaded = false;
    bool    isFullScreen   = false;
};

#endif // WEBBROWSERVIEW_H
