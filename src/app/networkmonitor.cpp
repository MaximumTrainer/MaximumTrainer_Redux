#include "networkmonitor.h"

#include <QCoreApplication>
#include <QDebug>

#include <QTimer>

#ifndef GC_WASM_BUILD
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QSslError>
#endif

// ─────────────────────────────────────────────────────────────────────────────
NetworkMonitor *NetworkMonitor::s_instance = nullptr;

NetworkMonitor *NetworkMonitor::instance()
{
    if (!s_instance)
        s_instance = new NetworkMonitor(qApp);
    return s_instance;
}

// ─────────────────────────────────────────────────────────────────────────────
NetworkMonitor::NetworkMonitor(QObject *parent)
    : QObject(parent)
    , m_isOnline(false)   // Start offline; the first probe will set the real state
{
#ifndef GC_WASM_BUILD
    m_manager = new QNetworkAccessManager(this);

    // Ignore SSL errors so a certificate issue does not falsely appear offline.
    connect(m_manager, &QNetworkAccessManager::sslErrors,
            [](QNetworkReply *reply, const QList<QSslError> &) {
                reply->ignoreSslErrors();
            });

    m_timer = new QTimer(this);
    m_timer->setInterval(30 * 1000); // probe every 30 seconds
    connect(m_timer, &QTimer::timeout, this, &NetworkMonitor::checkNow);
    m_timer->start();

    // Fire the first probe immediately (deferred to next event loop tick so the
    // constructor returns before any callbacks are invoked).
    QTimer::singleShot(0, this, &NetworkMonitor::checkNow);
#else
    // WASM always has network access — transition to online on the next event
    // loop tick so MainWindow's signal connections are in place before the
    // onlineChanged signal fires.
    QTimer::singleShot(0, this, [this]() { setOnline(true); });
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
void NetworkMonitor::checkNow()
{
#ifndef GC_WASM_BUILD
    if (m_checkReply)
        return; // already in flight — skip this round

    QNetworkRequest req(QUrl(QStringLiteral("https://intervals.icu")));
    // Do not follow redirects so the probe stays lightweight.
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::ManualRedirectPolicy);
    // Short timeout: we only need a TCP SYN / TLS hello response.
    req.setTransferTimeout(8000);

    m_checkReply = m_manager->head(req);
    connect(m_checkReply, &QNetworkReply::finished,
            this, &NetworkMonitor::onCheckReplyFinished);
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
#ifndef GC_WASM_BUILD
void NetworkMonitor::onCheckReplyFinished()
{
    if (!m_checkReply)
        return;

    const QNetworkReply::NetworkError err = m_checkReply->error();

    // Any HTTP-level response (200, 3xx, 4xx, 5xx) means the network stack
    // is functional.  Only connection-level errors signal an offline state.
    bool online;
    switch (err) {
    case QNetworkReply::NoError:
    case QNetworkReply::ContentAccessDenied:              // 403
    case QNetworkReply::AuthenticationRequiredError:      // 401
    case QNetworkReply::ContentNotFoundError:             // 404
    case QNetworkReply::ContentOperationNotPermittedError:
    case QNetworkReply::ProtocolInvalidOperationError:
    case QNetworkReply::TooManyRedirectsError:
    case QNetworkReply::InsecureRedirectError:
        online = true;
        break;
    default:
        // HostNotFoundError, NetworkSessionFailedError, TimeoutError, etc.
        online = false;
        break;
    }

    setOnline(online);

    m_checkReply->deleteLater();
    m_checkReply = nullptr;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
void NetworkMonitor::setOnline(bool online)
{
    if (m_isOnline == online)
        return;

    m_isOnline = online;
    qDebug() << "NetworkMonitor: online =" << online;
    emit onlineChanged(online);
}
