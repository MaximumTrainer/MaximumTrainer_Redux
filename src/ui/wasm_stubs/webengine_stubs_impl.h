// WebAssembly implementations for Qt WebEngine classes.
// Qt WebEngine is unavailable for WASM builds; these classes replace it with
// browser-native equivalents:
//   - QWebEngineView::setUrl() opens the URL in a new browser tab via
//     QDesktopServices::openUrl(), which Qt WASM routes through the browser.
//   - QWebEnginePage / QWebEngineProfile are lightweight no-ops whose API
//     surface is just enough to satisfy callers in the application code.
#pragma once

#include <QObject>
#include <QWidget>
#include <QMenu>
#include <QUrl>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QAction>
#include <QDesktopServices>
#include <functional>

// ─────────────────────────────── Forward declarations ────────────────────────
class QWebEnginePage;
class QWebEngineProfile;
class QWebEngineScriptCollection;

// ─────────────────────────────── QWebEngineSettings ──────────────────────────
class QWebEngineSettings {
public:
    enum WebAttribute { PluginsEnabled = 0, JavascriptEnabled = 1, FullScreenSupportEnabled = 2 };
    void setAttribute(WebAttribute, bool) {}
    static QWebEngineSettings *defaultSettings() { static QWebEngineSettings s; return &s; }
};

// ─────────────────────── QWebEngineFullScreenRequest ─────────────────────────
class QWebEngineFullScreenRequest {
public:
    void accept() {}
    void reject() {}
    bool toggleOn() const { return false; }
};

// ─────────────────────────────── QWebEngineScript ────────────────────────────
class QWebEngineScript {
public:
    enum ScriptWorldId { MainWorld = 0 };
    enum InjectionPoint { DocumentCreation = 0, DocumentReady = 1, Deferred = 2 };
    enum SavePageFormat { UnknownSaveFormat = -1, SingleHtmlSaveFormat = 0, CompleteHtmlSaveFormat = 1, MimeHtmlSaveFormat = 2 };
    void setName(const QString &) {}
    void setSourceCode(const QString &) {}
    void setWorldId(ScriptWorldId) {}
    void setInjectionPoint(InjectionPoint) {}
    void setRunsOnSubFrames(bool) {}
};

// ─────────────────────── QWebEngineScriptCollection ──────────────────────────
class QWebEngineScriptCollection {
public:
    void insert(const QWebEngineScript &) {}
    void clear() {}
};

// ─────────────────────────────── QWebEngineProfile ───────────────────────────
class QWebEngineProfile : public QObject {
public:
    enum PersistentCookiesPolicy { NoPersistentCookies = 0 };
    explicit QWebEngineProfile(QObject *parent = nullptr) : QObject(parent) {}
    static QWebEngineProfile *defaultProfile() { static QWebEngineProfile p; return &p; }
    QWebEngineSettings *settings() { return QWebEngineSettings::defaultSettings(); }
    void setPersistentCookiesPolicy(PersistentCookiesPolicy) {}
    void setHttpUserAgent(const QString &) {}
};

// ─────────────────────────────── QWebEnginePage ──────────────────────────────
class QWebEnginePage : public QObject {
public:
    enum NavigationType { NavigationTypeLinkClicked = 0 };
    enum WebAction { Back = 0, Forward = 1, Reload = 2, Stop = 3 };
    explicit QWebEnginePage(QObject *parent = nullptr)
        : QObject(parent), m_profile(new QWebEngineProfile(this)) {}
    explicit QWebEnginePage(QWebEngineProfile *profile, QObject *parent = nullptr)
        : QObject(parent), m_profile(profile ? profile : new QWebEngineProfile(this)) {}

    virtual bool acceptNavigationRequest(const QUrl &, NavigationType, bool) { return true; }

    void runJavaScript(const QString &) {}
    void setUrl(const QUrl &) {}
    QUrl url() const { return {}; }
    QWebEngineProfile *profile() const { return m_profile; }
    QWebEngineScriptCollection &scripts() { return m_scripts; }
    QAction *action(WebAction) { return &m_action; }
    void setWebChannel(QObject *) {}
    void toPlainText(std::function<void(const QString &)>) {}

private:
    QWebEngineProfile *m_profile;
    QWebEngineScriptCollection m_scripts;
    QAction m_action;
};

// ─────────────────────────────── QWebEngineView ──────────────────────────────
class QWebEngineView : public QWidget {
public:
    explicit QWebEngineView(QWidget *parent = nullptr)
        : QWidget(parent), m_page(new QWebEnginePage(this))
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        m_label = new QLabel(
            "<center><i>No content loaded</i></center>", this);
        m_label->setWordWrap(true);
        m_label->setTextFormat(Qt::RichText);
        m_label->setOpenExternalLinks(true);
        layout->addWidget(m_label, 0, Qt::AlignCenter);
    }

    void load(const QUrl &url) { setUrl(url); }
    void setUrl(const QUrl &url) {
        m_currentUrl = url;
        // Open in the browser — Qt WASM routes QDesktopServices::openUrl()
        // through window.open(), which is the native browser equivalent of
        // an embedded web view.
        QDesktopServices::openUrl(url);
        m_label->setText(QString(
            "<center><b>%1</b><br><br>"
            "<a href='%2'>Open in browser tab</a><br><br>"
            "<small>This content requires the desktop app for inline display.<br>"
            "Click the link to open it in a browser tab.</small></center>"
        ).arg(url.host(), url.toString()));
    }
    void setHtml(const QString &html, const QUrl & = QUrl()) {
        m_label->setText(html);
    }
    QUrl url() const { return m_currentUrl; }
    void setPage(QWebEnginePage *page) { if (page) m_page = page; }
    QWebEnginePage *page() const { return m_page; }
    QWebEngineSettings *settings() { return QWebEngineSettings::defaultSettings(); }
    QAction *pageAction(QWebEnginePage::WebAction action) { return m_page->action(action); }
    void back() {}
    void forward() {}
    void reload() {}
    void stop() {}

private:
    QWebEnginePage *m_page;
    QLabel         *m_label   = nullptr;
    QUrl            m_currentUrl;
};

// ─────────────────── QWebEngineDownloadRequest (Qt 6) ────────────────────────
class QWebEngineDownloadRequest : public QObject {
public:
    enum SavePageFormat { UnknownSaveFormat = -1, SingleHtmlSaveFormat = 0, CompleteHtmlSaveFormat = 1, MimeHtmlSaveFormat = 2 };
    explicit QWebEngineDownloadRequest(QObject *parent = nullptr) : QObject(parent) {}
    QUrl url() const { return {}; }
    void accept() {}
    void cancel() {}
    QString downloadFileName() const { return {}; }
    void setDownloadFileName(const QString &) {}
    QString downloadDirectory() const { return {}; }
    void setDownloadDirectory(const QString &) {}
    SavePageFormat savePageFormat() const { return UnknownSaveFormat; }
};

// ─────────────────────── QWebEngineDownloadItem (Qt 5) ───────────────────────
class QWebEngineDownloadItem : public QObject {
public:
    explicit QWebEngineDownloadItem(QObject *parent = nullptr) : QObject(parent) {}
    QUrl url() const { return {}; }
    void accept() {}
    void cancel() {}
    QString path() const { return {}; }
    void setPath(const QString &) {}
};

// ─────────────────────────────── QWebChannel ─────────────────────────────────
class QWebChannel : public QObject {
public:
    explicit QWebChannel(QObject *parent = nullptr) : QObject(parent) {}
    void registerObject(const QString &, QObject *) {}
    void deregisterObject(QObject *) {}
};
