###############################################################################
# tests/intervals_icu/intervals_icu_tests.pro
#
# Standalone Qt Test project for IntervalsIcuApi (HTTP-request builder).
#
# Depends only on Qt Core + Qt Network + Qt Test — no GUI, Bluetooth, or QWT.
#
# Build:
#   qmake intervals_icu_tests.pro && make
# Run:
#   ./build/tests/intervals_icu_tests -v2
###############################################################################

QT       += core network testlib
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = intervals_icu_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += \
    . \
    ../../src/persistence/db \
    ../../src/app

# ── Logger (required by intervals_icu_api.cpp) ────────────────────────────
SOURCES += \
    ../../src/app/logger.cpp

HEADERS += \
    ../../src/app/logger.h

# ── IntervalsIcuApi sources ───────────────────────────────────────────────
SOURCES += \
    ../../src/persistence/db/intervals_icu_api.cpp \
    tst_intervals_icu_service.cpp

HEADERS += \
    ../../src/persistence/db/intervals_icu_api.h

