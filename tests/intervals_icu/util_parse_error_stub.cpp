// Minimal Util stub for the OAuth exchange test.
//
// Provides the definition of Util::parseJsonIntervalsIcuOAuthErrorPayload
// declared inline in tst_intervals_icu_oauth_exchange.cpp.  The body is
// copied verbatim from src/app/util.cpp so the test exercises the same
// parsing logic without linking the full Util class (which pulls in QWT,
// Account, Settings, XmlUtil, etc.).
//
// If the production implementation changes, update the copy here to keep the
// test aligned with reality.
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>

class Util
{
public:
    static QString parseJsonIntervalsIcuOAuthErrorPayload(const QString &data);
};

QString Util::parseJsonIntervalsIcuOAuthErrorPayload(const QString &data)
{
    if (data.isEmpty())
        return QString();
    QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8());
    if (doc.isNull() || !doc.isObject())
        return QString();
    return doc.object().value(QStringLiteral("error")).toString();
}
