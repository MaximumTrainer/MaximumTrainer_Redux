###############################################################################
# tests/integration/online_mode_tests.pro
#
# Online Mode Integration Test -- MaximumTrainer
#
# Simulates a user authenticating into the application on the target OS.
# The test opens a 1280x720 window, makes a real HTTPS authentication request
# to https://intervals.icu/api/v1 using credentials supplied via environment
# variables, shows the auth result in the window, and captures a screenshot
# as visual build evidence.
#
# Credentials are read from environment variables:
#   INTERVALS_ICU_API_KEY    – personal API key (intervals.icu Settings → API)
#   INTERVALS_ICU_ATHLETE_ID – athlete ID, e.g. i12345
#
# The test calls QSKIP when either variable is absent, so the suite degrades
# gracefully in local development and fork PRs without configured secrets.
#
# Build (Linux / macOS):
#   qmake online_mode_tests.pro && make
# Build (Windows -- MSVC developer prompt):
#   qmake online_mode_tests.pro && nmake
#
# Run headless (Linux CI):
#   Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
#   ../../build/tests/online_mode_tests -v2
# Run directly (Windows / macOS CI -- display is always available):
#   .\build\tests\online_mode_tests.exe -v2
###############################################################################

QT       += core gui widgets network testlib
CONFIG   += qt c++17
CONFIG   -= app_bundle

TARGET   = online_mode_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += \
    . \
    ../../src/persistence/db \
    ../../src/app \
    ../../src/btle

# ── Logger (required by intervals_icu_service.cpp) ───────────────────────────
SOURCES += \
    ../../src/app/logger.cpp

HEADERS += \
    ../../src/app/logger.h

# ── IntervalsIcuService (static helper used for live API calls) ───────────────
SOURCES += \
    ../../src/persistence/db/intervals_icu_service.cpp \
    ../../src/btle/simulator_hub.cpp \
    tst_online_mode.cpp

HEADERS += \
    ../../src/persistence/db/intervals_icu_service.h \
    ../../src/btle/simulator_hub.h \
    tst_online_mode.h
