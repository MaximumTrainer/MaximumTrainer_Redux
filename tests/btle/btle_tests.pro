###############################################################################
# tests/btle/btle_tests.pro
#
# Standalone Qt Test project for BtleHub (no GUI, no ANT+, no VLC-Qt, no QWT).
# Depends only on: Qt Core + Qt Bluetooth + Qt Test.
#
# Build:
#   qmake btle_tests.pro && make
# Run:
#   ./btle_tests -v2
###############################################################################

QT       += core bluetooth testlib
QT       -= gui

CONFIG   += qt c++11 console
CONFIG   -= app_bundle

TARGET   = btle_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

# ── BtleHub sources ──────────────────────────────────────────────────────────
INCLUDEPATH += ../../src/btle ../../src/btle/zwift ../../src/app

SOURCES += \
    ../../src/app/logger.cpp \
    ../../src/btle/btle_hub.cpp \
    ../../src/btle/simulator_hub.cpp \
    ../../src/btle/zwift/zwift_click_relay.cpp \
    ../../src/btle/zwift/zwift_protocol.cpp \
    tst_btle_hub.cpp

HEADERS += \
    ../../src/app/logger.h \
    ../../src/btle/btle_hub.h \
    ../../src/btle/simulator_hub.h \
    ../../src/btle/zwift/zwift_click_relay.h \
    ../../src/btle/zwift/zwift_protocol.h \
    btle_device_simulator.h
