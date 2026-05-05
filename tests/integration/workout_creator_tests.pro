###############################################################################
# tests/integration/workout_creator_tests.pro
#
# Workout Creator Tests -- MaximumTrainer
#
# Validates the logic for constructing Workout objects from individual
# Interval domain objects (Warmup, SteadyState/FLAT, Cooldown, etc.) and
# verifies that the resulting Workout model is correctly populated:
#
#   • Interval count, step types (PROGRESSIVE / FLAT / NONE), power values,
#     durations, cadence targets, and cadence range.
#   • Workout metadata: name, plan, author, type, description.
#   • Total workout duration (sum of all interval QTimes).
#   • Computed metrics after calculateWorkoutMetrics(): averagePower for
#     a flat-power workout and a mixed-power workout.
#   • Edge cases: FTP = 0 (no crash), empty interval list, single interval,
#     high repeat (10 on/off pairs = 20 intervals), testInterval flag.
#
# No display required — tests run headlessly (no QWT or QtWebEngine).
#
# Uses the stub util.h and QApplication header from tests/intervals_icu/ to
# avoid pulling in QWT and QtWidgets (same pattern as
# importer_workout_zwo_tests.pro).
#
# Build (Linux / macOS):
#   qmake workout_creator_tests.pro && make
# Build (Windows -- MSVC developer prompt):
#   qmake workout_creator_tests.pro && nmake
# Run:
#   ../../build/tests/workout_creator_tests -v2
###############################################################################

QT       += core testlib
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = workout_creator_tests
TEMPLATE = app

DESTDIR     = ../../build/tests
OBJECTS_DIR = .obj_workout_creator
MOC_DIR     = .moc_workout_creator

# ── INCLUDEPATH — local stub dir MUST come FIRST so that:
#    • util.h        shadows the real util.h (which includes QWT)
#    • QApplication  shadows <QApplication> with a headless QCoreApplication
#                    wrapper (avoids pulling in QtWidgets) ─────────────────────
INCLUDEPATH += \
    ../intervals_icu \
    ../../src/app \
    ../../src/model \
    ../../src/persistence/db

SOURCES += \
    ../../src/model/account.cpp \
    ../../src/model/workout.cpp \
    ../../src/model/interval.cpp \
    ../../src/model/repeatdata.cpp \
    ../../src/model/powercurve.cpp \
    ../intervals_icu/credential_store_stub.cpp \
    tst_workout_creator.cpp

HEADERS += \
    ../../src/model/account.h \
    ../../src/model/workout.h \
    ../../src/model/interval.h
