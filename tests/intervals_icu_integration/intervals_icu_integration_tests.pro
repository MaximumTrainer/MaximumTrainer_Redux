###############################################################################
# tests/intervals_icu_integration/intervals_icu_integration_tests.pro
#
# Standalone Qt Test project for live Intervals.icu API integration tests.
#
# These tests make REAL HTTP requests to https://intervals.icu/api/v1.
# Credentials are read from environment variables:
#   INTERVALS_ICU_API_KEY    – personal API key (Settings → API on intervals.icu)
#   INTERVALS_ICU_ATHLETE_ID – athlete ID, e.g. i12345
#
# Tests call QSKIP when either variable is absent, so the suite degrades
# gracefully in local development and fork PRs without configured secrets.
#
# Build:
#   qmake intervals_icu_integration_tests.pro && make
# Run:
#   ./build/tests/intervals_icu_integration_tests -v2
###############################################################################

QT       += core network testlib
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = intervals_icu_integration_tests
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
    tst_intervals_icu_integration.cpp

HEADERS += \
    tst_intervals_icu_integration.h \
    ../../src/persistence/db/intervals_icu_api.h

# Qt 6.7.3's <QtCore> qyieldcpu.h calls the ARM intrinsic __yield() without
# including <arm_acle.h>; newer Xcode (macos-latest runner image) promotes
# -Wimplicit-function-declaration to a hard error. This is a core-only test
# (QT = core network testlib), so unlike the gui/widgets test targets nothing
# transitively pulls in <arm_acle.h> first. __yield is a clang builtin and
# still lowers correctly, so keep it a warning rather than a hard error.
macx {
    QMAKE_CXXFLAGS += -Wno-error=implicit-function-declaration
}
