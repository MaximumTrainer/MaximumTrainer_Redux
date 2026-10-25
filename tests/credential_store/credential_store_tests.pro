###############################################################################
# tests/credential_store/credential_store_tests.pro
#
# Standalone Qt Test project for CredentialStore.
#
# Tests exercise the store/load/remove interface.  The underlying implementation
# (DPAPI / Keychain / AES-GCM) is transparent; the tests verify behaviour, not
# the cryptographic primitive.
#
# Build:
#   qmake credential_store_tests.pro && make
# Run:
#   ./../../build/tests/credential_store_tests -v2
###############################################################################

QT       += core network testlib
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = credential_store_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += \
    . \
    ../../src/app

# ── CredentialStore: platform-specific backend ────────────────────────────────
contains(QMAKE_PLATFORM, wasm) {
    SOURCES += ../../src/app/credential_store_wasm.cpp
} else: win32 {
    SOURCES += ../../src/app/credential_store_win.cpp
    LIBS    += -lCrypt32
} else: macx {
    OBJECTIVE_SOURCES += ../../src/app/credential_store_mac.mm
    LIBS              += -framework Security
} else {
    SOURCES += ../../src/app/credential_store_linux.cpp
    LIBS    += -lcrypto
}

HEADERS += ../../src/app/credential_store.h \
           ../../src/app/logger.h

SOURCES += ../../src/app/logger.cpp

SOURCES += tst_credential_store.cpp

macx {
    LIBS += -lobjc -framework Foundation
}
