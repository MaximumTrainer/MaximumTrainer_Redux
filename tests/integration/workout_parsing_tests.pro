###############################################################################
# tests/integration/workout_parsing_tests.pro
#
# Live Intervals.icu Workout Pull-and-Load Test -- MaximumTrainer
#
# Contains a single live integration test that fetches a ZWO workout from
# intervals.icu, parses it, and verifies all intervals have positive duration.
# QSKIP when credentials are absent.
#
# ZWO inline-string parsing tests and programmatic model tests were removed;
# canonical coverage lives in:
#   • tst_workout_io.cpp      (ZWO parsing, XML round-trip, mock JSON)
#   • tst_workout_creator.cpp (Workout / Interval model construction)
#
# Dependencies
# ──────────────────────────────────────────────────────────────────────
# The local stub directories are prepended to INCLUDEPATH so that:
#   • tests/intervals_icu/util.h     shadows src/app/util.h     (no QWT)
#   • tests/intervals_icu/QApplication shadows <QApplication>  (no QtWidgets)
#
# This means QT -= gui is safe and no display is required at runtime.
#
# Build:
#   cd tests/integration
#   qmake workout_parsing_tests.pro && make -j$(nproc)
# Run (headless, no display needed):
#   ../../build/tests/workout_parsing_tests -v2
###############################################################################

QT       += core network testlib xml
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = workout_parsing_tests
TEMPLATE = app

DESTDIR     = ../../build/tests

# Use a private object directory so that workout.o compiled here (with the
# stub util.h, where convertQTimeToSecD is inline) is never reused by
# workout_ui_tests.pro (which uses the real util.h and leaves an external
# symbol reference in workout.o that would require util.cpp at link time).
OBJECTS_DIR = ../../build/tests/obj_workout_parsing

# ── Local stub headers MUST come FIRST so that util.h / QApplication are
#    shadowed without pulling in QWT or QtWidgets. ──────────────────────────
INCLUDEPATH += \
    ../intervals_icu \
    ../../src/persistence/file \
    ../../src/persistence/db \
    ../../src/model \
    ../../src/app

# ── Sources ──────────────────────────────────────────────────────────────────
SOURCES += \
    ../../src/persistence/file/importerworkoutzwo.cpp \
    ../../src/model/workout.cpp \
    ../../src/model/interval.cpp \
    ../../src/model/account.cpp \
    ../../src/model/repeatdata.cpp \
    ../../src/model/powercurve.cpp \
    ../intervals_icu/credential_store_stub.cpp \
    tst_workout_parsing.cpp

HEADERS += \
    ../../src/persistence/file/importerworkoutzwo.h \
    ../../src/model/workout.h \
    ../../src/model/interval.h \
    ../../src/model/account.h \
    ../../src/model/repeatdata.h \
    ../../src/model/powercurve.h \
    ../intervals_icu/util.h
