###############################################################################
# tests/integration/workout_parsing_tests.pro
#
# Workout Parsing & User Journey Tests -- MaximumTrainer
#
# Exercises the ZWO (Zwift workout XML) parsing pipeline and the workout
# model construction workflow end-to-end with no display requirement:
#
#   1. ZWO inline string parsing:
#      Raw ZWO XML strings embedded in C++ source code are fed to
#      ImporterWorkoutZwo::importFromByteArray() and the resulting
#      Workout model is verified for interval count, step type, and
#      FTP fraction.  Structures covered: SteadyState, Ramp, IntervalsT,
#      mixed Warmup/SteadyState/Cooldown, and FreeRide.
#
#   2. Workout model from programmatic intervals:
#      A three-interval workout (Warmup / SteadyState / Cooldown) is
#      constructed entirely in C++ using Interval objects and the 8-
#      parameter Workout constructor.  Every field and the calculated
#      average-power metric are verified.
#
#   3. Intervals.icu pull-and-load workflow (live, optional):
#      A real HTTPS request retrieves the athlete's 90-day workout list,
#      downloads the ZWO for the first entry, parses it, and asserts that
#      every interval has a positive duration.  QSKIP when credentials
#      are absent.
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

DESTDIR  = ../../build/tests

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
