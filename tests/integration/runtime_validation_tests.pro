###############################################################################
# tests/integration/runtime_validation_tests.pro
#
# Cross-Platform Runtime Validation Test – MaximumTrainer
#
# Validates that the application UI initialises and reaches a "Ready" state
# on Windows, macOS, and Linux.  Captures a labelled 1280×720 screenshot as
# visual build evidence and logs Bluetooth stack availability.
#
# Build (Linux / macOS):
#   qmake runtime_validation_tests.pro && make
# Build (Windows – MSVC developer prompt):
#   qmake runtime_validation_tests.pro && nmake
#
# Run headless (Linux CI):
#   Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
#   ../../build/tests/runtime_validation_tests -v2
# Run directly (Windows / macOS CI – display is available):
#   .\build\tests\runtime_validation_tests.exe -v2
###############################################################################

QT       += core gui widgets bluetooth testlib
CONFIG   += qt c++20
CONFIG   -= app_bundle

TARGET   = runtime_validation_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += ../../src/btle

SOURCES += \
    ../../src/btle/simulator_hub.cpp \
    tst_runtime_validation.cpp

HEADERS += \
    ../../src/btle/simulator_hub.h
