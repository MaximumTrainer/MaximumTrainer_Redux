###############################################################################
# tests/integration/btle_api_tests.pro
#
# BLE API smoke tests — exercises the real QLowEnergyController code path
# in BtleHub without physical hardware.  On CI runners with no BT adapter
# the tests QSKIP gracefully.  On machines with hardware the OS BLE stack
# is invoked and a connectionError is expected for the fake device address.
#
# Build:
#   qmake btle_api_tests.pro && make      (Linux / macOS)
#   qmake btle_api_tests.pro && nmake     (Windows)
# Run:
#   ./build/tests/btle_api_tests -v2
###############################################################################

QT       += core bluetooth testlib
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = btle_api_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += ../../src/btle ../../src/btle/zwift ../../src/app

SOURCES += \
    ../../src/app/logger.cpp \
    ../../src/btle/btle_hub.cpp \
    tst_btle_api.cpp

HEADERS += \
    ../../src/app/logger.h \
    ../../src/btle/btle_hub.h \
    ../../src/btle/btle_uuids.h
