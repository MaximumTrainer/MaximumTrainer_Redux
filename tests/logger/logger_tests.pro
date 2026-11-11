###############################################################################
# tests/logger/logger_tests.pro
#
# Standalone Qt Test project for the Logger framework.
# Depends only on Qt Core + Qt Test — no GUI, Bluetooth, VLC-Qt, or QWT.
#
# Build:
#   qmake logger_tests.pro && make
# Run:
#   ./build/tests/logger_tests -v2
###############################################################################

QT       += core testlib
QT       -= gui

CONFIG   += qt c++20 console
CONFIG   -= app_bundle

TARGET   = logger_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

# ── Logger sources ────────────────────────────────────────────────────────────
INCLUDEPATH += ../../src/app

SOURCES += \
    ../../src/app/logger.cpp \
    tst_logger.cpp

HEADERS += \
    ../../src/app/logger.h
