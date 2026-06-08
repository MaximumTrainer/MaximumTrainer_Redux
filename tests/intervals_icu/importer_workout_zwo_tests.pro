###############################################################################
# tests/intervals_icu/importer_workout_zwo_tests.pro
#
# Standalone Qt Test project for ImporterWorkoutZwo (ZWO XML parser).
#
# The local util.h stub in this directory shadows the real util.h so that
# workout.cpp and interval.cpp compile without pulling in QWT or Qt Gui.
# The local QApplication stub header satisfies the <QApplication> include in
# workout.cpp and interval.cpp without requiring QtWidgets to be installed.
#
# Build:
#   qmake importer_workout_zwo_tests.pro && make
# Run:
#   ./build/tests/importer_workout_zwo_tests -v2
###############################################################################

QT       += core network testlib xml
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = importer_workout_zwo_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

# ── Path to fixture files (made available via FIXTURES_DIR define) ────────────
FIXTURES_DIR = $$PWD/../fixtures
DEFINES += FIXTURES_DIR=\\\"$$FIXTURES_DIR\\\"

# ── INCLUDEPATH — local stub dir MUST come FIRST so that:
#    • util.h        shadows the real util.h (which includes QWT)
#    • QApplication  shadows <QApplication> with a headless QCoreApplication
#                    wrapper (avoids pulling in QtWidgets) ─────────────────────
INCLUDEPATH += \
    . \
    ../../src/persistence/db \
    ../../src/persistence/file \
    ../../src/model \
    ../../src/app

# ── Model sources needed by ImporterWorkoutZwo ─────────────────────────────────
SOURCES += \
    ../../src/persistence/file/importerworkoutzwo.cpp \
    ../../src/model/workout.cpp \
    ../../src/model/interval.cpp \
    ../../src/model/account.cpp \
    ../../src/model/repeatdata.cpp \
    credential_store_stub.cpp \
    tst_importer_workout_zwo.cpp

HEADERS += \
    ../../src/persistence/file/importerworkoutzwo.h \
    ../../src/model/workout.h \
    ../../src/model/interval.h \
    ../../src/model/account.h \
    ../../src/model/repeatdata.h \
    util.h
