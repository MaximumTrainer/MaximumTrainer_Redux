###############################################################################
# tests/zwift/zwift_tests.pro
#
# Standalone Qt Test project for the Zwift virtual-shifting protocol codec
# (Phase 0). Pure byte-level encode/decode — no QtBluetooth, no GUI.
#
# Build:
#   qmake6 zwift_tests.pro && make
# Run:
#   ./zwift_tests -v2
###############################################################################

QT       += core testlib
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = zwift_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += ../../src/btle/zwift

SOURCES += \
    ../../src/btle/zwift/zwift_protocol.cpp \
    tst_zwift_protocol.cpp

HEADERS += \
    ../../src/btle/zwift/zwift_protocol.h
