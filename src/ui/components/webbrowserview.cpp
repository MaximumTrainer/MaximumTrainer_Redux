#include "webbrowserview.h"
#include "setting_keys.h"

#include <QWebEngineView>
#include <QWebEnginePage>
#ifndef GC_WASM_BUILD
#include <QWebEngineProfile>
#include <QWebEngineProfileBuilder>
#endif
#include <QWebEngineSettings>
#include <QWebEngineFullScreenRequest>
#include <QGridLayout>
#include <QToolBar>
#include <QLineEdit>
#include <QTimer>
#include <QAction>
#include <QKeyEvent>
#include <QSettings>
#include <QCursor>
#include <QApplication>

#include "logger.h"

namespace {
constexpr int kToolbarHideMs = 3000;
constexpr int kCursorPollMs  = 200;

#ifndef GC_WASM_BUILD
// Qt 6's default profile is off the record, so a YouTube sign-in was lost on
// every restart. One on-disk profile is shared by all workout browsers.
QWebEngineProfile *workoutBrowserProfile()
{
    static QWebEngineProfile *profile =
        QWebEngineProfileBuilder()
            .setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies)
            .createProfile(QStringLiteral("WorkoutBrowser"), qApp);
    return profile;
}

// Qt's default page forwards every console message of the site (YouTube logs
// hundreds per ride) into the app log as warnings. They say nothing about the
// app, so keep them out of the normal log.
class BrowserPage : public QWebEnginePage
{
public:
    using QWebEnginePage::QWebEnginePage;

protected:
    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level, const QString &message,
                                  int lineNumber, const QString &sourceID) override
    {
        const QString line = QStringLiteral("JS [%1:%2] %3").arg(sourceID).arg(lineNumber).arg(message);
        if (level == ErrorMessageLevel)
            LOG_DEBUG("WebBrowser", line);
        else
            LOG_VERBOSE("WebBrowser", line);
    }
};
#endif
}

WebBrowserView::WebBrowserView(QWidget *parent) : QWidget(parent)
{
    gLayout = new QGridLayout(this);
    gLayout->setContentsMargins(0, 0, 0, 0);

    locationEdit = new QLineEdit(this);
    locationEdit->setSizePolicy(QSizePolicy::Expanding,
                                locationEdit->sizePolicy().verticalPolicy());

    webEngineView = new QWebEngineView(this);
#ifndef GC_WASM_BUILD
    webEngineView->setPage(new BrowserPage(workoutBrowserProfile(), webEngineView));
#endif
    webEngineView->settings()->setAttribute(QWebEngineSettings::PluginsEnabled, true);
    // Without FullScreenSupportEnabled + an accepted fullScreenRequested,
    // YouTube greys out its fullscreen button ("Full screen is unavailable").
    webEngineView->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);

#ifndef GC_WASM_BUILD
    // On WASM the QWebEngineView stub opens URLs in a browser tab and emits none
    // of these signals, so the connections are desktop-only.
    connect(webEngineView, &QWebEngineView::loadFinished, this, &WebBrowserView::adjustLocation);
    connect(webEngineView, &QWebEngineView::urlChanged, this, &WebBrowserView::updateUrlOfLineEdit);
    connect(webEngineView->page(), &QWebEnginePage::fullScreenRequested,
            this, [this](QWebEngineFullScreenRequest request) {
                request.accept();
                // The video element now fills the webview; hide the browser
                // chrome so "fullscreen" means the whole workout video area
                // (it reappears on mouse/keyboard activity as usual).
                if (request.toggleOn())
                    hideToolbar();
            });
#endif

    toolBar = new QToolBar(this);
    toolBar->addAction(webEngineView->pageAction(QWebEnginePage::Back));
    toolBar->addAction(webEngineView->pageAction(QWebEnginePage::Forward));
    toolBar->addAction(webEngineView->pageAction(QWebEnginePage::Reload));
    toolBar->addAction(webEngineView->pageAction(QWebEnginePage::Stop));
    toolBar->addSeparator();
    toolBar->addWidget(locationEdit);

    toolBar->setAutoFillBackground(true);

    gLayout->addWidget(webEngineView, 0, 0, 1, 1);

    timerHideToolbar = new QTimer(this);
    timerHideToolbar->setSingleShot(true);
    connect(timerHideToolbar, &QTimer::timeout, this, &WebBrowserView::hideToolbar);

    // Mouse events over the page go to WebEngine's internal render widget, and
    // Enter/Hover events also fire when nothing moved (the toolbar hiding under
    // a still cursor, windows restacking), which kept re-showing the toolbar.
    // Polling the real cursor position only reacts to actual movement.
    timerCursorPoll = new QTimer(this);
    connect(timerCursorPoll, &QTimer::timeout, this, &WebBrowserView::pollCursorActivity);

    hideToolbar();

    locationEdit->installEventFilter(this);
}

WebBrowserView::~WebBrowserView() = default;

void WebBrowserView::loadHomePageIfNeeded()
{
    if (homePageLoaded)
        return;

    webEngineView->load(QUrl::fromUserInput(AppSettings::readWorkoutBrowserHomePage()));
    homePageLoaded = true;
}

void WebBrowserView::hideToolbar()
{
    // Never yank the address bar away while the user is typing in it.
    if (locationEdit->hasFocus() && toolBar->isVisible()) {
        timerHideToolbar->start(kToolbarHideMs);
        return;
    }
    timerHideToolbar->stop();
    toolBar->setVisible(false);
}

void WebBrowserView::showToolbarForActivity()
{
    timerHideToolbar->start(kToolbarHideMs);
    if (!toolBar->isVisible()) {
        toolBar->setVisible(true);
        toolBar->raise();
    }
}

void WebBrowserView::pollCursorActivity()
{
    const QPoint cursorGlobalPos = QCursor::pos();
    if (cursorGlobalPos == lastCursorGlobalPos)
        return;
    lastCursorGlobalPos = cursorGlobalPos;
    if (window()->isActiveWindow() && rect().contains(mapFromGlobal(cursorGlobalPos)))
        showToolbarForActivity();
}

void WebBrowserView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    toolBar->setGeometry(0, 0, width(), toolBar->sizeHint().height());
}

void WebBrowserView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    lastCursorGlobalPos = QCursor::pos();
    timerCursorPoll->start(kCursorPollMs);
}

void WebBrowserView::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    timerCursorPoll->stop();
}

bool WebBrowserView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == locationEdit && event->type() == QEvent::KeyPress) {
        showToolbarForActivity();
        auto *key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Enter || key->key() == Qt::Key_Return) {
            changeLocation();
            return true;
        }
    }
    return false;
}

void WebBrowserView::changeLocation()
{
    webEngineView->load(QUrl::fromUserInput(locationEdit->text()));
    // Hand focus to the page so the toolbar can auto-hide again.
    webEngineView->setFocus();
}

void WebBrowserView::adjustLocation()
{
    locationEdit->setText(webEngineView->url().toString());
}

void WebBrowserView::updateUrlOfLineEdit(const QUrl &url)
{
    locationEdit->setText(url.toString());
}

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
