// macOS implementation of CredentialStore using the Security.framework Keychain.
// SecItemAdd / SecItemCopyMatching / SecItemDelete provide user-keychain-backed
// storage — no AES key management is required in application code.

#include "credential_store.h"
#include "logger.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

bool CredentialStore::store(const QString &service, const QString &key, const QString &value)
{
    NSString *svc  = service.toNSString();
    NSString *acct = key.toNSString();
    NSData   *data = [value.toNSString() dataUsingEncoding:NSUTF8StringEncoding];

    // Remove any existing item with the same service + account before adding.
    NSDictionary *query = @{
        (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: svc,
        (__bridge id)kSecAttrAccount: acct,
    };
    SecItemDelete((__bridge CFDictionaryRef)query);

    NSDictionary *attrs = @{
        (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: svc,
        (__bridge id)kSecAttrAccount: acct,
        (__bridge id)kSecValueData:   data,
    };
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)attrs, nullptr);
    return status == errSecSuccess;
}

QString CredentialStore::load(const QString &service, const QString &key)
{
    NSString *svc  = service.toNSString();
    NSString *acct = key.toNSString();

    NSDictionary *query = @{
        (__bridge id)kSecClass:            (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService:      svc,
        (__bridge id)kSecAttrAccount:      acct,
        (__bridge id)kSecReturnData:       @YES,
        (__bridge id)kSecMatchLimit:       (__bridge id)kSecMatchLimitOne,
    };

    CFTypeRef result = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
    if (status != errSecSuccess || !result) {
        if (status != errSecItemNotFound)
            LOG_WARN("CredentialStore", QStringLiteral("Keychain read of credential %1/%2 failed (OSStatus %3) — treating it as empty")
                                            .arg(service, key).arg(static_cast<int>(status)));
        return {};
    }

    // Built without ARC: release the copied keychain item and the string
    // explicitly (a __bridge_transfer cast does nothing here and leaked both).
    NSData   *data = (__bridge NSData *)result;
    NSString *str  = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    const QString value = QString::fromNSString(str);
    [str release];
    CFRelease(result);
    return value;
}

void CredentialStore::remove(const QString &service, const QString &key)
{
    NSString *svc  = service.toNSString();
    NSString *acct = key.toNSString();

    NSDictionary *query = @{
        (__bridge id)kSecClass:       (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: svc,
        (__bridge id)kSecAttrAccount: acct,
    };
    SecItemDelete((__bridge CFDictionaryRef)query);
}
