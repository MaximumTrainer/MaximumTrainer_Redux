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
CONFIG   += qt c++20
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
        DEFINES     += QWT_DLL
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
    ../../src/ui/plots \
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
    ../../src/ui/components/faderlabel.cpp \
    ../../src/ui/components/shapefactory.cpp \
    ../../src/ui/dialog_connection_method.cpp \
    ../../src/ui/plots/curvedatacadence.cpp \
    ../../src/ui/plots/curvedataheartrate.cpp \
    ../../src/ui/plots/curvedatapower.cpp \
    ../../src/ui/plots/curvedataspeed.cpp \
    ../../src/ui/plots/workoutplot.cpp \
    ../../src/ui/plots/zoneitem.cpp \
    ../../src/ui/workout_editor/myqwtpickermachine.cpp \
    ../../src/ui/workout_editor/myqwtplotpicker.cpp \
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
    ../../src/ui/components/faderlabel.h \
    ../../src/ui/components/shapefactory.h \
    ../../src/ui/dialog_connection_method.h \
    ../../src/ui/plots/workoutplot.h \
    ../../src/ui/workout_editor/myqwtplotpicker.h \
    ../../src/ui/workout_editor/repeatwidget.h

FORMS += \
    ../../src/ui/workout_editor/repeatwidget.ui
