###############################################################################
# tests/integration/ui_navigation_tests.pro
#
# Full Application Journey Test -- MaximumTrainer
#
# Runs the MaximumTrainer binary out-of-process via QProcess with the
# --screenshots flag and verifies that all expected screenshot files are
# produced for each tab.
#
# Component-level tests (ConnectionDialog, IntervalTableModel,
# WorkoutHistorySummary) were removed; canonical coverage lives in:
#   • tst_ui_screens.cpp   (ConnectionDialog, PostWorkoutSummary)
#   • tst_workout_creator.cpp  (IntervalTableModel)
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
    ../intervals_icu/credential_store_stub.cpp \
    tst_ui_navigation.cpp

HEADERS += \
    ../../src/app/logger.h \
    ../../src/app/util.h \
    ../../src/model/account.h \
    ../../src/model/settings.h \
    ../../src/model/workout.h \
    ../../src/model/interval.h \
    ../../src/persistence/file/xmlutil.h
