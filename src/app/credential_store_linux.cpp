// Linux implementation of CredentialStore using AES-256-GCM via OpenSSL.
//
// Key management strategy:
//   A random 256-bit key is generated on first use and written to a
//   chmod-600 protected file in the application's local data directory
//   (e.g. ~/.local/share/MaximumTrainer/.sk).  Subsequent runs load this
//   key.  The key file is never exposed in QSettings.
//
// Credential storage:
//   Encrypted values (base64-encoded IV‖ciphertext‖GCM-tag) are stored in
//   QSettings under the "credentials" group.  The GCM tag provides
//   authentication — a tampered ciphertext returns an empty QString.

#include "credential_store.h"
#include "logger.h"

#include <QSettings>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QFileInfo>

#include <openssl/evp.h>
#include <openssl/rand.h>

// ── Key file helpers ──────────────────────────────────────────────────────────

static QString keyFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return dir + QStringLiteral("/.sk");
}

static QByteArray loadOrCreateKey()
{
    const QString path = keyFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile f(path);

    // Attempt to load an existing 32-byte key.
    if (f.exists()) {
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray k = f.readAll();
            f.close();
            if (k.size() == 32)
                return k;
        }
    }

    // Generate a new random 256-bit key.
    QByteArray key(32, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char *>(key.data()), 32) != 1)
        return {}; // Should never happen; OpenSSL RAND is always seeded on Linux.

    f.setFileName(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};

    // Restrict access to owner read/write only before writing sensitive data.
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    f.write(key);
    f.close();
    return key;
}

// ── AES-256-GCM encrypt / decrypt ────────────────────────────────────────────

// Returns base64(IV[12] || ciphertext || tag[16]), or empty on error.
static QString encryptGCM(const QByteArray &plaintext, const QByteArray &key)
{
    if (key.size() != 32)
        return {};

    QByteArray iv(12, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char *>(iv.data()), 12) != 1)
        return {};

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return {};

    QByteArray ct(plaintext.size() + 16, '\0'); // extra headroom
    QByteArray tag(16, '\0');
    int len = 0, ctLen = 0;

    bool ok =
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) == 1 &&
        EVP_EncryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData())) == 1 &&
        EVP_EncryptUpdate(ctx,
                          reinterpret_cast<unsigned char *>(ct.data()), &len,
                          reinterpret_cast<const unsigned char *>(plaintext.constData()),
                          plaintext.size()) == 1;

    ctLen = len;

    ok = ok &&
         EVP_EncryptFinal_ex(ctx,
                             reinterpret_cast<unsigned char *>(ct.data()) + ctLen, &len) == 1;
    ctLen += len;
    ct.resize(ctLen);

    ok = ok &&
         EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) == 1;

    EVP_CIPHER_CTX_free(ctx);

    if (!ok)
        return {};

    return QString::fromLatin1((iv + ct + tag).toBase64());
}

// Returns decrypted plaintext, or empty string if the tag fails or data is malformed.
static QString decryptGCM(const QString &b64, const QByteArray &key)
{
    if (key.size() != 32 || b64.isEmpty())
        return {};

    const QByteArray payload = QByteArray::fromBase64(b64.toLatin1());
    // Minimum: 12 (IV) + 0 (empty plaintext) + 16 (tag) = 28 bytes
    if (payload.size() < 28)
        return {};

    const QByteArray iv  = payload.left(12);
    const QByteArray tag = payload.right(16);
    const QByteArray ct  = payload.mid(12, payload.size() - 28);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx)
        return {};

    QByteArray pt(ct.size(), '\0');
    int len = 0, ptLen = 0;

    bool ok =
        EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr) == 1 &&
        EVP_DecryptInit_ex(ctx, nullptr, nullptr,
                           reinterpret_cast<const unsigned char *>(key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData())) == 1 &&
        EVP_DecryptUpdate(ctx,
                          reinterpret_cast<unsigned char *>(pt.data()), &len,
                          reinterpret_cast<const unsigned char *>(ct.constData()),
                          ct.size()) == 1;

    ptLen = len;

    // Set expected GCM tag before calling Final.
    ok = ok &&
         EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16,
                              const_cast<char *>(tag.constData())) == 1;

    // EVP_DecryptFinal_ex returns 1 only if the tag authenticates.
    ok = ok &&
         EVP_DecryptFinal_ex(ctx,
                             reinterpret_cast<unsigned char *>(pt.data()) + ptLen, &len) == 1;
    ptLen += len;
    pt.resize(ptLen);

    EVP_CIPHER_CTX_free(ctx);

    if (!ok)
        return {};

    return QString::fromUtf8(pt);
}

// ── CredentialStore API ───────────────────────────────────────────────────────

static QString settingsKey(const QString &service, const QString &key)
{
    return service + QLatin1Char('/') + key;
}

bool CredentialStore::store(const QString &service, const QString &key, const QString &value)
{
    const QByteArray aesKey = loadOrCreateKey();
    if (aesKey.isEmpty())
        return false;

    const QString encrypted = encryptGCM(value.toUtf8(), aesKey);
    if (encrypted.isEmpty())
        return false;

    QSettings s;
    s.beginGroup(QStringLiteral("credentials"));
    s.setValue(settingsKey(service, key), encrypted);
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

    const QByteArray aesKey = loadOrCreateKey();
    if (aesKey.isEmpty()) {
        LOG_WARN("CredentialStore", QStringLiteral("Encryption key unavailable — credential %1/%2 treated as empty")
                                        .arg(service, key));
        return {};
    }

    const QString value = decryptGCM(b64, aesKey);
    if (value.isEmpty())
        LOG_WARN("CredentialStore", QStringLiteral("Could not decrypt credential %1/%2 (corrupt or written with another key) — treating it as empty")
                                        .arg(service, key));
    return value;
}

void CredentialStore::remove(const QString &service, const QString &key)
{
    QSettings s;
    s.beginGroup(QStringLiteral("credentials"));
    s.remove(settingsKey(service, key));
    s.endGroup();
}
