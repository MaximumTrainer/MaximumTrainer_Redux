###############################################################################
# tests/integration/workout_ui_tests.pro
#
# Workout UI Integration Tests -- MaximumTrainer
#
# Covers user-login flow (offline), workout XML creation, workout XML
# retrieval, ERG cycle-trainer simulation (setLoad / setSlope), and the
# full workout-session lifecycle (start / pause / resume / stop).  A
# 1280×720 screenshot of the simulated workout-execution screen is saved
# as build evidence.
#
# Dependencies kept intentionally lean:
#   • SimulatorHub  -- BTLE cycle-trainer simulator
#   • Qt core / gui / widgets / testlib only
#   No QWT, no network stack, no DataWorkout, no FIT SDK required.
#
# Build (Linux / macOS):
#   qmake workout_ui_tests.pro && make -j$(nproc)
# Build (Windows -- MSVC developer prompt):
#   qmake workout_ui_tests.pro && jom /J %NUMBER_OF_PROCESSORS%
#
# Run headless (Linux CI):
#   Xvfb :99 -screen 0 1280x800x24 &
#   export DISPLAY=:99
#   ../../build/tests/workout_ui_tests -v2
# Run directly (Windows / macOS CI -- display is always available):
#   .\build\tests\workout_ui_tests.exe -v2
###############################################################################

QT       += core gui widgets testlib
CONFIG   += qt c++17
CONFIG   -= app_bundle

TARGET   = workout_ui_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += ../../src/btle

SOURCES += \
    ../../src/btle/simulator_hub.cpp \
    tst_workout_ui.cpp

HEADERS += \
    ../../src/btle/simulator_hub.h
