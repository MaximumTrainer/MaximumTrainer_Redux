#include "intervals_icu_oauth_flow.h"

#include "environnement.h"
#include "extrequest.h"
#include "util.h"
#include "account.h"
#include "logger.h"

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QDesktopServices>
#include <QNetworkReply>

IntervalsIcuOAuthFlow::IntervalsIcuOAuthFlow(QObject *parent) : QObject(parent) {}

IntervalsIcuOAuthFlow::~IntervalsIcuOAuthFlow()
{
    abort();
}

void IntervalsIcuOAuthFlow::setOpenExternalBrowser(bool open)
{
    m_openExternalBrowser = open;
}

quint16 IntervalsIcuOAuthFlow::listenPort() const
{
    return m_server ? m_server->serverPort() : 0;
}

bool IntervalsIcuOAuthFlow::start()
{
    m_server = new QTcpServer(this);
    // Loopback only — never expose this to the network.
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        LOG_WARN("IntervalsIcuOAuthFlow",
                 QStringLiteral("could not start loopback listener: ") + m_server->errorString());
        return false;
    }
    const quint16 port = m_server->serverPort();
    m_redirectUri = QStringLiteral("http://localhost:%1/").arg(port);
    m_csrfState   = QUuid::createUuid().toString(QUuid::Id128).left(16);

    connect(m_server, &QTcpServer::newConnection, this, &IntervalsIcuOAuthFlow::onNewConnection);

    m_authUrl = Environnement::getURLIntervalsIcuAuthorize(m_csrfState, m_redirectUri);
    LOG_INFO("IntervalsIcuOAuthFlow",
             QStringLiteral("opening system browser for Intervals.icu login; redirect ") + m_redirectUri);
    if (m_openExternalBrowser)
        QDesktopServices::openUrl(QUrl(m_authUrl));
    return true;
}

void IntervalsIcuOAuthFlow::abort()
{
    if (m_server)
        m_server->close();
    if (m_tokenReply) {
        auto *r = m_tokenReply;
        m_tokenReply = nullptr;
        r->abort();
        r->deleteLater();
    }
    m_done = true;
}

void IntervalsIcuOAuthFlow::onNewConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    if (!socket)
        return;
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        const QByteArray request = socket->readAll();
        // First line: "GET /?code=...&state=... HTTP/1.1"
        const QString firstLine = QString::fromUtf8(request).section(QStringLiteral("\r\n"), 0, 0);
        const QString target    = firstLine.section(' ', 1, 1);   // "/?code=..."

        const QUrl url(QStringLiteral("http://localhost") + target);
        const QUrlQuery query(url);
        const bool hasCode  = query.hasQueryItem(QStringLiteral("code"));
        const bool hasError = query.hasQueryItem(QStringLiteral("error"));

        // Browsers also fetch /favicon.ico etc. — ignore anything without the
        // OAuth params and keep listening for the real redirect.
        if (!hasCode && !hasError) {
            socket->write("HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n");
            socket->disconnectFromHost();
            return;
        }

        const QByteArray html =
            "<html><head><meta charset='utf-8'><title>MaximumTrainer</title></head>"
            "<body style='font-family:sans-serif;text-align:center;padding-top:60px;'>"
            "<h2>You can close this tab and return to MaximumTrainer.</h2></body></html>";
        QByteArray response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n";
        response += "Content-Length: " + QByteArray::number(html.size()) + "\r\nConnection: close\r\n\r\n";
        response += html;
        socket->write(response);
        socket->flush();
        socket->disconnectFromHost();

        m_server->close();   // got our redirect — stop listening

        if (hasError) {
            const QString error = query.queryItemValue(QStringLiteral("error"));
            LOG_WARN("IntervalsIcuOAuthFlow",
                     QStringLiteral("Intervals.icu OAuth declined/error: ") + error);
            settle(error == QLatin1String("access_denied")
                       ? &IntervalsIcuOAuthFlow::cancelled
                       : &IntervalsIcuOAuthFlow::failed);
            return;
        }

        if (query.queryItemValue(QStringLiteral("state")) != m_csrfState) {
            LOG_WARN("IntervalsIcuOAuthFlow",
                     QStringLiteral("state mismatch — possible CSRF attack"));
            settle(&IntervalsIcuOAuthFlow::failed);
            return;
        }

        exchangeCode(query.queryItemValue(QStringLiteral("code")));
    });
}

void IntervalsIcuOAuthFlow::exchangeCode(const QString &code)
{
    m_tokenReply = ExtRequest::intervalsIcuOAuthExchange(code, m_redirectUri);
    if (!m_tokenReply) {
        settle(&IntervalsIcuOAuthFlow::failed);
        return;
    }
    connect(m_tokenReply, &QNetworkReply::finished, this, [this]() {
        if (!m_tokenReply)
            return; // aborted
        auto *reply = m_tokenReply;
        m_tokenReply = nullptr;
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("IntervalsIcuOAuthFlow",
                     QStringLiteral("token exchange failed: ") + reply->errorString());
            settle(&IntervalsIcuOAuthFlow::failed);
            return;
        }
        Util::parseJsonIntervalsIcuOAuthToken(QString::fromUtf8(reply->readAll()));
        Account *account = qApp->property("Account").value<Account*>();
        if (account && !account->intervals_icu_access_token.isEmpty()) {
            account->saveIntervalsIcuCredentials();
            LOG_INFO("IntervalsIcuOAuthFlow", QStringLiteral("Intervals.icu login succeeded"));
            settle(&IntervalsIcuOAuthFlow::succeeded);
        } else {
            LOG_WARN("IntervalsIcuOAuthFlow",
                     QStringLiteral("token missing from exchange response"));
            settle(&IntervalsIcuOAuthFlow::failed);
        }
    });
}

void IntervalsIcuOAuthFlow::settle(void (IntervalsIcuOAuthFlow::*signal)())
{
    if (m_done)
        return;
    m_done = true;
    emit (this->*signal)();
}
