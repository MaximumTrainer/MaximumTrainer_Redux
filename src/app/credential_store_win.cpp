// Windows implementation of CredentialStore using DPAPI.
// CryptProtectData encrypts with the current user's credentials as the key,
// making stored tokens user-bound and machine-local — no application-managed
// AES key or OpenSSL headers are required.

#include "credential_store.h"
#include "logger.h"

#include <QSettings>
#include <QString>

#include <windows.h>
#include <wincrypt.h>

static QString settingsKey(const QString &service, const QString &key)
{
    return service + QLatin1Char('/') + key;
}

bool CredentialStore::store(const QString &service, const QString &key, const QString &value)
{
    QByteArray plaintext = value.toUtf8();

    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE *>(plaintext.data());
    in.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB out{};
    if (!CryptProtectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out))
        return false;

    QByteArray encrypted(reinterpret_cast<const char *>(out.pbData),
                         static_cast<int>(out.cbData));
    LocalFree(out.pbData);

    QSettings s;
    s.beginGroup(QStringLiteral("credentials"));
    s.setValue(settingsKey(service, key), QString::fromLatin1(encrypted.toBase64()));
    s.endGroup();
    return true;
}

QString CredentialStore::load(const QString &service, const QString &key)
{
    QSettings s;
    s.beginGroup(QStringLiteral("credentials"));
    const QString b64 = s.value(settingsKey(service, key)).toString();
    s.endGroup();

    if (b64.isEmpty())
        return {};

    const auto decoded = QByteArray::fromBase64Encoding(b64.toLatin1());
    if (!decoded || decoded.decoded.isEmpty()) {
        LOG_WARN("CredentialStore", QStringLiteral("Stored credential %1/%2 is not valid base64 — treating it as empty")
                                        .arg(service, key));
        return {};
    }
    QByteArray encrypted = decoded.decoded;

    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE *>(encrypted.data());
    in.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out)) {
        LOG_WARN("CredentialStore", QStringLiteral("DPAPI could not decrypt credential %1/%2 (error %3) — treating it as empty")
                                        .arg(service, key).arg(GetLastError()));
        return {};
    }

    QString result = QString::fromUtf8(reinterpret_cast<const char *>(out.pbData),
                                       static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return result;
}

void CredentialStore::remove(const QString &service, const QString &key)
{
    QSettings s;
    s.beginGroup(QStringLiteral("credentials"));
    s.remove(settingsKey(service, key));
    s.endGroup();
}
