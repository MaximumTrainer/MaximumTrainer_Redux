###############################################################################
# tests/game/game_tests.pro
#
# Retro race: controller timing and the native road renderer.
#
# Build:  qmake game_tests.pro && make
# Run:    QT_QPA_PLATFORM=offscreen ./../../build/tests/game_tests -v2
###############################################################################

QT       += core gui quick qml testlib

CONFIG   += qt c++20 console
CONFIG   -= app_bundle

TARGET   = game_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += . ../../src/game

include(../../src/fitness/fit/fit.pri)

SOURCES += \
    ../../src/game/ghostreplay.cpp \
    ../../src/game/retroracecontroller.cpp \
    ../../src/game/roadsurface.cpp \
    tst_game.cpp

HEADERS += \
    ../../src/game/ghostreplay.h \
    ../../src/game/cyclingphysics.h \
    ../../src/game/retroracecontroller.h \
    ../../src/game/roadsurface.h
