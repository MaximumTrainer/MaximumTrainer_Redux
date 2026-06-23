###############################################################################
# tests/history/history_tests.pro
#
# Standalone Qt Test project for the Workout History dashboard logic:
#   - FIT activity indexing (FitActivityReader / FitActivityDetailReader)
#   - TSS aggregation (PmcCalculator)
#
# Build:
#   qmake6 history_tests.pro && make
# Run:
#   ./../../build/tests/history_tests -v2
###############################################################################

QT       += core testlib
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = history_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += \
    . \
    ../../src/model \
    ../../src/persistence/file \
    ../../src/fitness \
    ../../src/fitness/fit

# FIT SDK (encode/decode + all message types)
include(../../src/fitness/fit/fit.pri)

SOURCES += \
    ../../src/fitness/pmccalculator.cpp \
    ../../src/persistence/file/fitactivityreader.cpp \
    ../../src/persistence/file/fitactivitydetailreader.cpp \
    tst_history.cpp

HEADERS += \
    ../../src/model/workouthistorysummary.h \
    ../../src/model/workouthistorydetail.h \
    ../../src/fitness/pmccalculator.h \
    ../../src/persistence/file/fitactivityreader.h \
    ../../src/persistence/file/fitactivitydetailreader.h
