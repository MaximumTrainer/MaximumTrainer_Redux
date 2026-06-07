# tests/integration/ui_screens_tests.pro
#
# UI Screen Navigation Tests -- MaximumTrainer
#
# Tests the primary UI screens:
#   • DialogConnectionMethod — "Simulation" vs "BTLE Device" selection,
#     QTest::mouseClick on real QPushButton widgets, QSignalSpy on accepted().
#   • PostWorkoutSummary — lightweight post-workout summary widget; verifies
#     that metric labels are visible and non-empty.
#
# ZWO inline-string parsing tests were removed; canonical coverage lives in
# tst_workout_io.cpp (workout_io_tests.pro).
#
# Build (Linux / macOS):
#   qmake ui_screens_tests.pro && make
# Build (Windows -- MSVC developer prompt):
#   qmake ui_screens_tests.pro && nmake
#
# Run headless (Linux CI):
#   Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
#   ../../build/tests/ui_screens_tests -v2
# Run directly (Windows / macOS CI):
#   .\build\tests\ui_screens_tests.exe -v2
###############################################################################

QT       += core gui widgets network testlib
CONFIG   += qt c++17
CONFIG   -= app_bundle

TARGET   = ui_screens_tests
TEMPLATE = app

DESTDIR     = ../../build/tests
OBJECTS_DIR = .obj_ui_screens
MOC_DIR     = .moc_ui_screens

# ── QWT: platform-specific include / link ───────────────────────────────────
linux {
    # Qt6 has no qwt apt package: build QWT from source and pass QWT_INSTALL=...
    # Qt5 apt build leaves QWT_INSTALL empty and links the system -lqwt-qt5.
    !isEmpty(QWT_INSTALL) {
        INCLUDEPATH += $${QWT_INSTALL}/include
        LIBS        += -L$${QWT_INSTALL}/lib -lqwt
    } else {
        INCLUDEPATH += /usr/include/qwt
        LIBS        += -lqwt-qt5
    }
}
win32 {
    !isEmpty(QWT_INSTALL) {
        INCLUDEPATH += $${QWT_INSTALL}/include
        CONFIG(release, debug|release): LIBS += -L$${QWT_INSTALL}/lib -lqwt
        CONFIG(debug,   debug|release): LIBS += -L$${QWT_INSTALL}/lib -lqwtd
    }
}
macx {
    !isEmpty(QWT_INSTALL) {
        INCLUDEPATH += $${QWT_INSTALL}/include
        LIBS        += -L$${QWT_INSTALL}/lib -lqwt
    }
}
# ── End QWT ─────────────────────────────────────────────────────────────────

INCLUDEPATH += \
    ../../src/app \
    ../../src/btle \
    ../../src/model \
    ../../src/fitness/achievements \
    ../../src/persistence/db \
    ../../src/persistence/file \
    ../../src/ui \
    ../../src/ui/components \
    ../../src/ui/workout_editor

SOURCES += \
    ../../src/app/logger.cpp \
    ../../src/app/util.cpp \
    ../../src/app/simplecrypt.cpp \
    ../../src/model/account.cpp \
    ../../src/model/settings.cpp \
    ../../src/model/workout.cpp \
    ../../src/model/interval.cpp \
    ../../src/model/sensor.cpp \
    ../../src/model/radio.cpp \
    ../../src/model/repeatdata.cpp \
    ../../src/model/userstudio.cpp \
    ../../src/model/trackpoint.cpp \
    ../../src/fitness/achievements/achievement.cpp \
    ../../src/ui/dialog_connection_method.cpp \
    ../../src/ui/workout_editor/repeatwidget.cpp \
    ../intervals_icu/credential_store_stub.cpp \
    tst_ui_screens.cpp

HEADERS += \
    ../../src/app/logger.h \
    ../../src/app/util.h \
    ../../src/model/account.h \
    ../../src/model/settings.h \
    ../../src/model/workout.h \
    ../../src/model/interval.h \
    ../../src/ui/dialog_connection_method.h \
    ../../src/ui/workout_editor/repeatwidget.h

FORMS += \
    ../../src/ui/workout_editor/repeatwidget.ui
