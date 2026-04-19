// Stub implementation of CredentialStore for unit tests.
// Tests that compile account.cpp do not need real secure storage.
#include "credential_store.h"

bool CredentialStore::store(const QString &, const QString &, const QString &)
{
    return true;
}

QString CredentialStore::load(const QString &, const QString &)
{
    return {};
}

void CredentialStore::remove(const QString &, const QString &)
{
}
