#include "updatenotifier.h"

#include <QNetworkReply>
#include <QRegularExpression>

#include "environnement.h"
#include "logger.h"
#include "util.h"
#include "versiondao.h"

UpdateNotifier::UpdateNotifier(QObject *parent)
    : QObject(parent)
{
}

void UpdateNotifier::check()
{
#ifndef Q_OS_WASM
    // Source builds report 0.0.0 and would always look outdated.
    if (Environnement::getVersion().remove(QRegularExpression("^[vV]")) == QLatin1String("0.0.0"))
        return;

    QNetworkReply *reply = VersionDAO::getVersion();
    if (!reply)
        return;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            LOG_WARN("UpdateNotifier", QStringLiteral("Version check failed: ") + reply->errorString());
            return;
        }
        const QString latest = newerVersion(Environnement::getVersion(),
                                            QString::fromUtf8(reply->readAll()));
        if (latest.isEmpty())
            return;
        LOG_INFO("UpdateNotifier", QStringLiteral("Update available: ") + latest);
        emit updateAvailable(latest);
    });
#endif
}

QString UpdateNotifier::newerVersion(const QString &currentVersion, const QString &releaseJson)
{
    QString latest = Util::parseJsonObjectVersion(releaseJson);
    if (latest.isEmpty() || !Util::isVersionNewer(currentVersion, latest))
        return {};
    return latest;
}
