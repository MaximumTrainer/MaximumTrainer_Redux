###############################################################################
# tests/webengine_safe_mode/webengine_safe_mode_tests.pro
#
# Qt Test project for WebEngineSafeMode (#381): the crash marker around the
# QtWebEngine initialisation and the safe-graphics decision it drives.
# Depends only on Qt Core + Qt Test.
#
# Build:
#   qmake webengine_safe_mode_tests.pro && make
# Run:
#   ./../../build/tests/webengine_safe_mode_tests -v2
###############################################################################

QT       += core testlib
QT       -= gui

CONFIG   += qt c++20 console
CONFIG   -= app_bundle

TARGET   = webengine_safe_mode_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += ../../src/app

SOURCES += \
    ../../src/app/logger.cpp \
    ../../src/app/webengine_safe_mode.cpp \
    tst_webengine_safe_mode.cpp

HEADERS += \
    ../../src/app/logger.h \
    ../../src/app/webengine_safe_mode.h
