###############################################################################
# tests/integration/ui_navigation_tests.pro
#
# UI Navigation & User Journey Tests -- MaximumTrainer
#
# Covers the primary UI screen interaction flows using Qt Test's
# mouse/keyboard simulation facilities:
#
#   1. ConnectionDialog (DialogConnectionMethod)
#      Verifies that clicking "Simulation" or "BTLE Device" accepts the
#      dialog and returns the correct ConnectionMethod enum value.
#      Uses QTest::mouseClick() and QSignalSpy.
#
#   2. WorkoutCreator — IntervalTableModel interactions
#      Exercises the model that backs the WorkoutCreator interval table:
#      insertRows(), removeRows(), copyRows(), setListInterval().
#
#   3. Post-workout summary screen
#      Constructs a WorkoutHistorySummary with known data, renders a
#      1280×720 summary window, and verifies all metric fields.
#
#   4. Visual evidence screenshots (1280×720, platform-tagged, PNG)
#
# Dependencies follow the same pattern as workout_ui_tests.pro:
#   • Real util.h / util.cpp (requires QWT for QwtPlot types)
#   • Real app sources (logger, simplecrypt, account, interval, workout …)
#   • credential_store_stub.cpp (avoids the OpenSSL/platform keychain dep)
#
# QWT: platform-specific include / link (same as workout_ui_tests.pro)
#
# Build (Linux):
#   cd tests/integration
#   qmake ui_navigation_tests.pro && make -j$(nproc)
# Build (macOS):
#   qmake ui_navigation_tests.pro QWT_INSTALL=/path/to/qwt && make -j$(sysctl -n hw.logicalcpu)
# Build (Windows — MSVC developer prompt):
#   qmake ui_navigation_tests.pro QWT_INSTALL=C:/qwt && jom /J %NUMBER_OF_PROCESSORS%
#
# Run headless (Linux CI):
#   Xvfb :99 -screen 0 1280x800x24 &
#   sleep 1
#   export DISPLAY=:99
#   ../../build/tests/ui_navigation_tests -v2
# Run directly (Windows / macOS CI — display is always available):
#   .\build\tests\ui_navigation_tests.exe -v2
###############################################################################

QT       += core gui widgets network sql testlib
CONFIG   += qt c++17
CONFIG   -= app_bundle

TARGET   = ui_navigation_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

# ── QWT: platform-specific include / link ────────────────────────────────────
linux {
    INCLUDEPATH += /usr/include/qwt
    LIBS        += -lqwt-qt5
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
# ── End QWT ──────────────────────────────────────────────────────────────────

INCLUDEPATH += \
    ../../src/app \
    ../../src/btle \
    ../../src/model \
    ../../src/fitness/achievements \
    ../../src/persistence/db \
    ../../src/persistence/file \
    ../../src/ui \
    ../../src/ui/workout_editor

SOURCES += \
    ../../src/app/logger.cpp \
    ../../src/app/util.cpp \
    ../../src/app/simplecrypt.cpp \
    ../../src/model/account.cpp \
    ../../src/model/settings.cpp \
    ../../src/model/workout.cpp \
    ../../src/model/interval.cpp \
    ../../src/model/intervaltablemodel.cpp \
    ../../src/model/sensor.cpp \
    ../../src/model/radio.cpp \
    ../../src/model/repeatdata.cpp \
    ../../src/model/course.cpp \
    ../../src/model/userstudio.cpp \
    ../../src/model/trackpoint.cpp \
    ../../src/model/powercurve.cpp \
    ../../src/fitness/achievements/achievement.cpp \
    ../../src/persistence/db/environnement.cpp \
    ../../src/persistence/db/extrequest.cpp \
    ../../src/persistence/db/userdao.cpp \
    ../../src/persistence/db/versiondao.cpp \
    ../../src/persistence/db/intervalsicudao.cpp \
    ../../src/persistence/file/xmlutil.cpp \
    ../../src/persistence/file/gpxparser.cpp \
    ../../src/ui/dialog_connection_method.cpp \
    ../../src/ui/workout_editor/repeatwidget.cpp \
    ../intervals_icu/credential_store_stub.cpp \
    tst_ui_navigation.cpp

HEADERS += \
    ../../src/app/logger.h \
    ../../src/app/util.h \
    ../../src/model/account.h \
    ../../src/model/settings.h \
    ../../src/model/workout.h \
    ../../src/model/interval.h \
    ../../src/model/intervaltablemodel.h \
    ../../src/model/workouthistorysummary.h \
    ../../src/ui/dialog_connection_method.h \
    ../../src/persistence/file/xmlutil.h \
    ../../src/ui/workout_editor/repeatwidget.h

FORMS += \
    ../../src/ui/workout_editor/repeatwidget.ui
