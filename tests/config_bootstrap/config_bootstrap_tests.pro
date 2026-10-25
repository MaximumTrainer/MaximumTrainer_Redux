###############################################################################
# tests/config_bootstrap/config_bootstrap_tests.pro
#
# Qt Test project for ConfigBootstrap (first-run defaults, legacy migration,
# unreadable store handling) and CredentialStore failure logging (#377).
# Depends only on Qt Core + Qt Test.
#
# Build:
#   qmake config_bootstrap_tests.pro && make
# Run:
#   ./../../build/tests/config_bootstrap_tests -v2
###############################################################################

QT       += core testlib
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = config_bootstrap_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += ../../src/app

SOURCES += \
    ../../src/app/logger.cpp \
    ../../src/app/config_bootstrap.cpp \
    tst_config_bootstrap.cpp

HEADERS += \
    ../../src/app/logger.h \
    ../../src/app/config_bootstrap.h \
    ../../src/app/config_defaults.h \
    ../../src/app/credential_store.h

# ── CredentialStore: platform-specific backend ────────────────────────────────
win32 {
    SOURCES += ../../src/app/credential_store_win.cpp
    LIBS    += -lCrypt32
} else: macx {
    OBJECTIVE_SOURCES += ../../src/app/credential_store_mac.mm
    LIBS              += -framework Security -lobjc -framework Foundation
} else {
    SOURCES += ../../src/app/credential_store_linux.cpp
    LIBS    += -lcrypto
}
