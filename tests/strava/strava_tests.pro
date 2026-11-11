###############################################################################
# tests/strava/strava_tests.pro
#
# Standalone Qt Test project for StravaService (HTTP-request builder).
#
# Depends only on Qt Core + Qt Network + Qt Test — no GUI, Bluetooth, or QWT.
#
# Build:
#   qmake strava_tests.pro && make
# Run:
#   ./../../build/tests/strava_tests -v2
#
# Optional live tests:
#   Set STRAVA_ACCESS_TOKEN env var to run tests that hit the real Strava API.
###############################################################################

QT       += core network testlib
QT       -= gui

CONFIG   += qt c++20 console
CONFIG   -= app_bundle

TARGET   = strava_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += \
    . \
    ../../src/persistence/db \
    ../../src/app

# ── Logger (required by strava_service.cpp) ───────────────────────────────────
SOURCES += \
    ../../src/app/logger.cpp

HEADERS += \
    ../../src/app/logger.h

# ── StravaService sources ─────────────────────────────────────────────────────
SOURCES += \
    ../../src/persistence/db/strava_service.cpp \
    tst_strava_service.cpp

HEADERS += \
    ../../src/persistence/db/strava_service.h
