#ifndef WEBBROWSERVIEW_H
#define WEBBROWSERVIEW_H

#include <QWidget>

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

private:
    QWebEngineView *webEngineView = nullptr;
    QLineEdit      *locationEdit  = nullptr;
    QToolBar       *toolBar       = nullptr;
    QGridLayout    *gLayout       = nullptr;
    QTimer         *timerHideToolbar = nullptr;

    bool    homePageLoaded = false;
};

#endif // WEBBROWSERVIEW_H
