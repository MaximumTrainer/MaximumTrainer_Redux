#include "strava_oauth_flow.h"

#include "strava_service.h"
#include "environnement.h"
#include "util.h"
#include "account.h"
#include "logger.h"

#include <QCoreApplication>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHostAddress>
#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>
#include <QNetworkReply>

StravaOAuthFlow::StravaOAuthFlow(QObject *parent) : QObject(parent) {}

StravaOAuthFlow::~StravaOAuthFlow()
{
    if (m_server)
        m_server->close();
}

bool StravaOAuthFlow::start()
{
    m_server = new QTcpServer(this);
    // Loopback only — never expose this to the network.
    if (!m_server->listen(QHostAddress::LocalHost, 0)) {
        LOG_WARN("StravaOAuthFlow",
                 QStringLiteral("could not start loopback listener: ") + m_server->errorString());
        return false;
    }
    const quint16 port = m_server->serverPort();
    m_redirectUri = QStringLiteral("http://localhost:%1/").arg(port);

    connect(m_server, &QTcpServer::newConnection, this, &StravaOAuthFlow::onNewConnection);

    const QString authUrl = Environnement::getURLStravaAuthorize(m_redirectUri);
    LOG_INFO("StravaOAuthFlow",
             QStringLiteral("opening system browser for Strava login; redirect ") + m_redirectUri);
    QDesktopServices::openUrl(QUrl(authUrl));
    return true;
}

void StravaOAuthFlow::onNewConnection()
{
    QTcpSocket *socket = m_server->nextPendingConnection();
    if (!socket)
        return;
    connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        const QByteArray request = socket->readAll();
        // First line: "GET /?code=...&scope=... HTTP/1.1"
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
            LOG_WARN("StravaOAuthFlow",
                     QStringLiteral("Strava OAuth declined/error: ")
                     + query.queryItemValue(QStringLiteral("error")));
            finish(false);
            return;
        }
        exchangeCode(query.queryItemValue(QStringLiteral("code")));
    });
}

void StravaOAuthFlow::exchangeCode(const QString &code)
{
    QNetworkReply *reply = StravaService::exchangeAuthCode(code, m_redirectUri);
    if (!reply) {
        finish(false);
        return;
    }
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("StravaOAuthFlow",
                     QStringLiteral("token exchange failed: ") + reply->errorString());
            finish(false);
            return;
        }
        Util::parseJsonStravaObject(QString::fromUtf8(reply->readAll()));
        Account *account = qApp->property("Account").value<Account*>();
        if (account && !account->strava_access_token.isEmpty()) {
            account->saveStravaCredentials();
            LOG_INFO("StravaOAuthFlow", QStringLiteral("Strava linked successfully"));
            finish(true);
        } else {
            finish(false);
        }
    });
}

void StravaOAuthFlow::finish(bool linked)
{
    if (m_done)
        return;
    m_done = true;
    emit finished(linked);
}
