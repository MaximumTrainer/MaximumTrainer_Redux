#include "webbrowserview.h"

#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineFullScreenRequest>
#include <QGridLayout>
#include <QToolBar>
#include <QLineEdit>
#include <QTimer>
#include <QAction>
#include <QKeyEvent>
#include <QSettings>

namespace {
constexpr int kToolbarHideMs = 7000;
const char *kDefaultHomePage = "https://www.youtube.com";
}

WebBrowserView::WebBrowserView(QWidget *parent) : QWidget(parent)
{
    gLayout = new QGridLayout(this);
    gLayout->setContentsMargins(0, 0, 0, 0);

    locationEdit = new QLineEdit(this);
    locationEdit->setSizePolicy(QSizePolicy::Expanding,
                                locationEdit->sizePolicy().verticalPolicy());

    webEngineView = new QWebEngineView(this);
    webEngineView->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    // QtWebEngine disables fullscreen by default; enable it and accept the
    // page's fullscreen requests below so YouTube's fullscreen button works.
    webEngineView->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);

#ifndef GC_WASM_BUILD
    // On WASM the QWebEngineView stub opens URLs in a browser tab and emits none
    // of these signals, so the connections are desktop-only.
    connect(webEngineView, &QWebEngineView::loadFinished, this, &WebBrowserView::adjustLocation);
    connect(webEngineView, &QWebEngineView::urlChanged, this, &WebBrowserView::updateUrlOfLineEdit);
    connect(webEngineView->page(), &QWebEnginePage::fullScreenRequested,
            this, &WebBrowserView::handleFullScreenRequested);
#endif

    toolBar = new QToolBar(this);
    toolBar->addAction(webEngineView->pageAction(QWebEnginePage::Back));
    toolBar->addAction(webEngineView->pageAction(QWebEnginePage::Forward));
    toolBar->addAction(webEngineView->pageAction(QWebEnginePage::Reload));
    toolBar->addAction(webEngineView->pageAction(QWebEnginePage::Stop));
    toolBar->addSeparator();
    QAction *actionFullScreen = toolBar->addAction(QIcon(QStringLiteral(":/image/icon/fullscreen")),
                                                   tr("Toggle Fullscreen"));
    // Toggle the web view's fullscreen via JS, which round-trips through the
    // same fullScreenRequested path as YouTube's own button.
    connect(actionFullScreen, &QAction::triggered, this, [this]() {
        webEngineView->page()->runJavaScript(QStringLiteral(
            "if (document.fullscreenElement) { document.exitFullscreen(); }"
            "else {"
            "  var v = document.getElementsByTagName('video');"
            "  if (v.length > 0) { v[0].requestFullscreen(); }"
            "  else { document.documentElement.requestFullscreen(); }"
            "}"));
    });
    toolBar->addWidget(locationEdit);

    gLayout->addWidget(toolBar, 0, 0, 1, 1);
    gLayout->addWidget(webEngineView, 1, 0, 1, 1);

    // The toolbar hides itself after a few idle seconds and reappears on any
    // mouse/keyboard activity, so it does not cover the video during playback.
    timerHideToolbar = new QTimer(this);
    connect(timerHideToolbar, &QTimer::timeout, this, &WebBrowserView::hideToolbar);
    hideToolbar();

    locationEdit->installEventFilter(this);
    toolBar->installEventFilter(this);
    webEngineView->installEventFilter(this);
}

WebBrowserView::~WebBrowserView() = default;

//----------------------------------------------------------------------------------------
void WebBrowserView::loadHomePageIfNeeded()
{
    if (homePageLoaded)
        return;

    QSettings settings;
    settings.beginGroup(QStringLiteral("webBrowserWorkout"));
    // One-time reset: clear any previously saved home page and force YouTube.
    // The old Netflix default needs Widevine DRM, which this embedded view
    // cannot bundle, so it never played. Guarded so the user can set their own
    // URL afterwards.
    if (!settings.value(QStringLiteral("youtubeResetDone"), false).toBool()) {
        settings.setValue(QStringLiteral("defaultUrl"), QString::fromLatin1(kDefaultHomePage));
        settings.setValue(QStringLiteral("youtubeResetDone"), true);
    }
    const QString urlSaved = settings.value(QStringLiteral("defaultUrl"),
                                            QString::fromLatin1(kDefaultHomePage)).toString();
    settings.endGroup();

    webEngineView->load(QUrl::fromUserInput(urlSaved));
    homePageLoaded = true;
}

//----------------------------------------------------------------------------------------
void WebBrowserView::handleFullScreenRequested(QWebEngineFullScreenRequest request)
{
    // Accept the request but keep the web view docked in our layout: the page's
    // fullscreen element then fills the existing web-view widget (the video area
    // within the workout dialog) rather than covering the whole screen.
    request.accept();

    isFullScreen = request.toggleOn();
    // Give the video the full widget by hiding our chrome while fullscreen.
    if (isFullScreen) {
        hideToolbar();
    } else {
        toolBar->setVisible(true);
        timerHideToolbar->start(kToolbarHideMs);
    }
}

//----------------------------------------------------------------------------------------
void WebBrowserView::keyPressEvent(QKeyEvent *event)
{
    // Esc leaves page fullscreen (mirrors a normal browser).
    if (event->key() == Qt::Key_Escape && isFullScreen) {
        webEngineView->page()->runJavaScript(QStringLiteral(
            "if (document.fullscreenElement) document.exitFullscreen();"));
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

//----------------------------------------------------------------------------------------
void WebBrowserView::hideToolbar()
{
    timerHideToolbar->stop();
    toolBar->setVisible(false);
}

//------------------------------------------------------------------------------------------------
bool WebBrowserView::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched);

    switch (event->type()) {
    case QEvent::MouseMove:
    case QEvent::Enter:
    case QEvent::HoverEnter:
    case QEvent::HoverMove:
    case QEvent::KeyPress:
        timerHideToolbar->start(kToolbarHideMs);
        toolBar->setVisible(true);
        break;
    default:
        break;
    }

    if (event->type() == QEvent::KeyPress) {
        // Enter/Return in the address bar must navigate, not bubble up to the
        // workout dialog (which would otherwise start/pause the workout).
        auto *key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Enter || key->key() == Qt::Key_Return) {
            changeLocation();
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------------------------
void WebBrowserView::changeLocation()
{
    webEngineView->load(QUrl::fromUserInput(locationEdit->text()));
}

//----------------------------------------------------------------------------------------
void WebBrowserView::adjustLocation()
{
    locationEdit->setText(webEngineView->url().toString());
}

//------------------------------------------------------------------------------
void WebBrowserView::updateUrlOfLineEdit(const QUrl &url)
{
    locationEdit->setText(url.toString());
}

//-----------------------------------------------------------------
// Finds the first <video> element on the page (or inside a same-origin iframe)
// and plays/pauses it, so the workout's start/pause controls also drive the
// embedded video.
namespace {
QString videoControlJs(const char *method)
{
    return QStringLiteral(
        "(function() {"
        "  var v = document.getElementsByTagName('video');"
        "  if (v.length > 0) { v[0].%1(); return; }"
        "  var frames = document.getElementsByTagName('iframe');"
        "  for (var i = 0; i < frames.length; ++i) {"
        "    try {"
        "      var fv = frames[i].contentDocument.getElementsByTagName('video');"
        "      if (fv.length > 0) { fv[0].%1(); return; }"
        "    } catch (e) { /* cross-origin frame, skip */ }"
        "  }"
        "})();").arg(QString::fromLatin1(method));
}
}

void WebBrowserView::playVideo()
{
    webEngineView->page()->runJavaScript(videoControlJs("play"));
}

void WebBrowserView::pauseVideo()
{
    webEngineView->page()->runJavaScript(videoControlJs("pause"));
}
