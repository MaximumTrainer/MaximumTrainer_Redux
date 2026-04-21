#ifndef CREDENTIAL_STORE_H
#define CREDENTIAL_STORE_H

#include <QString>

///
/// Secure, platform-native credential storage for OAuth tokens and passwords.
///
/// Each platform uses its native secure storage mechanism:
///   Windows  — DPAPI (CryptProtectData / CryptUnprotectData), user-bound.
///   macOS    — Security.framework Keychain (SecItemAdd / SecItemCopyMatching).
///   Linux    — AES-256-GCM via OpenSSL with a randomly-generated key persisted
///              in a chmod-600 protected file in the application data directory.
///   WASM     — browser localStorage via Emscripten JS interop (not encrypted;
///              consistent with standard browser-based web app security model).
///
/// Credentials are keyed by (service, key), e.g.:
///   store("strava", "access_token", token)
///   load ("strava", "access_token")
///
/// All methods are synchronous and intended to be called from the main thread.
///
class CredentialStore
{
public:
    /// Encrypt and persist a credential value.
    /// @return true on success, false on failure.
    static bool store(const QString &service, const QString &key, const QString &value);

    /// Retrieve and decrypt a credential value.
    /// @return the stored value, or an empty QString if not found or decryption failed.
    static QString load(const QString &service, const QString &key);

    /// Remove a stored credential.
    static void remove(const QString &service, const QString &key);
};

#endif // CREDENTIAL_STORE_H
