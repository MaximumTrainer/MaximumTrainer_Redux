###############################################################################
# tests/plan_adherence/plan_adherence_tests.pro
#
# Standalone Qt Test project for PlanAdherenceStore.
#
# Build:
#   qmake plan_adherence_tests.pro && make
# Run:
#   ./../../build/tests/plan_adherence_tests -v2
###############################################################################

QT       += core testlib
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = plan_adherence_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += \
    . \
    ../../src/model

SOURCES += \
    ../../src/model/planadherencestore.cpp \
    tst_plan_adherence.cpp

HEADERS += \
    ../../src/model/planadherence.h \
    ../../src/model/planadherencestore.h
