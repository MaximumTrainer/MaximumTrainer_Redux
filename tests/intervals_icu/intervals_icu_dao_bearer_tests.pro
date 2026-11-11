###############################################################################
# tests/intervals_icu/intervals_icu_dao_bearer_tests.pro
#
# Standalone Qt Test project for IntervalsIcuDAO Bearer-token methods.
#
# Tests the OAuth2 Bearer authentication overloads added to IntervalsIcuDAO:
#   - getAthleteBearer()
#   - getAthleteSettingsBearer()
#
# Depends only on Qt Core + Qt Network + Qt Test — no GUI, Bluetooth, or QWT.
#
# Build:
#   qmake intervals_icu_dao_bearer_tests.pro && make
# Run:
#   ./build/tests/intervals_icu_dao_bearer_tests -v2
###############################################################################

QT       += core network testlib
QT       -= gui

CONFIG   += qt c++20 console
CONFIG   -= app_bundle

TARGET   = intervals_icu_dao_bearer_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += \
    . \
    ../../src/persistence/db \
    ../../src/app

# ── Logger (required by intervalsicudao.cpp) ──────────────────────────────────
SOURCES += \
    ../../src/app/logger.cpp

HEADERS += \
    ../../src/app/logger.h

# ── IntervalsIcuDAO sources ───────────────────────────────────────────────────
SOURCES += \
    ../../src/persistence/db/intervalsicudao.cpp \
    tst_intervals_icu_dao_bearer.cpp

HEADERS += \
    ../../src/persistence/db/intervalsicudao.h
