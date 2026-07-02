###############################################################################
# tests/intervals_icu/intervals_icu_oauth_exchange_tests.pro
#
# Standalone Qt Test project for ExtRequest::intervalsIcuOAuthExchange /
# intervalsIcuOAuthRefresh and Util::parseJsonIntervalsIcuOAuthErrorPayload.
#
# Depends only on Qt Core + Qt Network + Qt Test — no GUI, Bluetooth, QWT,
# or SQL.  A minimal credential_store_stub.cpp and util_parse_error_stub.cpp
# replace the real modules to keep the link graph tiny.
#
# Build:
#   qmake6 intervals_icu_oauth_exchange_tests.pro && make
# Run:
#   ../../build/tests/intervals_icu_oauth_exchange_tests -v2
###############################################################################

QT       += core network testlib
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = intervals_icu_oauth_exchange_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += \
    . \
    ../../src/persistence/db \
    ../../src/app

# ── Logger (required by extrequest.cpp / environnement.cpp) ───────────────
SOURCES += \
    ../../src/app/logger.cpp

HEADERS += \
    ../../src/app/logger.h

# ── ExtRequest + Environnement under test ─────────────────────────────────
SOURCES += \
    ../../src/persistence/db/extrequest.cpp \
    ../../src/persistence/db/environnement.cpp

HEADERS += \
    ../../src/persistence/db/extrequest.h \
    ../../src/persistence/db/environnement.h

# ── Stubs to avoid pulling account.cpp / util.cpp / secret backends ───────
SOURCES += \
    credential_store_stub.cpp \
    util_parse_error_stub.cpp

# ── Test runner ───────────────────────────────────────────────────────────
SOURCES += \
    tst_intervals_icu_oauth_exchange.cpp
