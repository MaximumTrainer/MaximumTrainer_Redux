// WASM credential store using browser localStorage via Emscripten JS interop.
//
// Desktop builds use platform keychain (DPAPI / Keychain / OpenSSL-AES).
// WASM has no native secure enclave, so credentials are stored in
// localStorage under the key "mt_cred_<service>_<key>".  The store is
// cleared when the user's browser data is wiped; values are not encrypted
// but this matches the security model of any other browser-based web app.
//
// This enables OAuth tokens for Strava, Intervals.icu, and TrainingPeaks to
// persist across browser sessions in the WASM build.

#include "credential_store.h"
#include <emscripten.h>
#include <cstdlib>

static std::string makeLocalStorageKey(const QString &service, const QString &key)
{
    return std::string("mt_cred_")
        + service.toStdString()
        + "_"
        + key.toStdString();
}

bool CredentialStore::store(const QString &service,
                             const QString &key,
                             const QString &value)
{
    const std::string k = makeLocalStorageKey(service, key);
    const std::string v = value.toStdString();
    const int stored = EM_ASM_INT({
        try {
            window.localStorage.setItem(
                UTF8ToString($0),
                UTF8ToString($1));
            return 1;
        } catch(e) {
            return 0;
        }
    }, k.c_str(), v.c_str());
    return stored != 0;
}

QString CredentialStore::load(const QString &service, const QString &key)
{
    const std::string k = makeLocalStorageKey(service, key);
    char *raw = reinterpret_cast<char *>(EM_ASM_PTR({
        try {
            const val = window.localStorage.getItem(UTF8ToString($0));
            if (!val) return 0;
            const len  = lengthBytesUTF8(val) + 1;
            const heap = _malloc(len);
            stringToUTF8(val, heap, len);
            return heap;
        } catch(e) {
            return 0;
        }
    }, k.c_str()));
    if (!raw)
        return {};
    const QString result = QString::fromUtf8(raw);
    // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
    free(raw);
    return result;
}

void CredentialStore::remove(const QString &service, const QString &key)
{
    const std::string k = makeLocalStorageKey(service, key);
    EM_ASM({
        try {
            window.localStorage.removeItem(UTF8ToString($0));
        } catch(e) {}
    }, k.c_str());
}
