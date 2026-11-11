###############################################################################
# tests/integration/offline_mode_tests.pro
#
# Offline Mode Integration Test -- MaximumTrainer
#
# Validates that the application's core offline functionality works correctly
# across Windows, macOS, and Linux:
#
#   1. Local workout file loading: Creates a workout XML file on disk and
#      parses it using only the local file system (no network module linked).
#   2. BTLE cycling simulation: SimulatorHub broadcasts CSC (0x1816) and
#      Power (0x1818) data over the same signal contract as BtleHub.
#   3. Offline UI screenshot: Captures a labelled 1280x720 screenshot showing
#      the "OFFLINE MODE" badge, live BTLE metrics, and a recording counter.
#   4. Data integrity: Accumulated data points are written to a TSV activity
#      file, confirming offline data persistence without any network.
#
# No Qt network module is linked; all sensor data comes from SimulatorHub.
#
# Build (Linux / macOS):
#   qmake offline_mode_tests.pro && make
# Build (Windows -- MSVC developer prompt):
#   qmake offline_mode_tests.pro && nmake
#
# Run headless (Linux CI):
#   Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
#   ../../build/tests/offline_mode_tests -v2
# Run directly (Windows / macOS CI -- display is always available):
#   .\build\tests\offline_mode_tests.exe -v2
###############################################################################

QT       += core gui widgets testlib
QT       -= network
CONFIG   += qt c++20
CONFIG   -= app_bundle

TARGET   = offline_mode_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += ../../src/btle

SOURCES += \
    ../../src/btle/simulator_hub.cpp \
    tst_offline_mode.cpp

HEADERS += \
    ../../src/btle/simulator_hub.h
