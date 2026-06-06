###############################################################################
# tests/integration/workout_io_tests.pro
#
# Workout I/O Tests -- MaximumTrainer
#
# Validates the workout import/export pipeline:
#   • ZWO parsing from inline XML strings (no file I/O):
#       - SteadyState, Ramp (PROGRESSIVE), IntervalsT expansion, FreeRide,
#         mixed workout, name extraction, edge cases (empty/malformed input,
#         power = 0, power > 1).
#   • Intervals.icu mock API response workflow:
#       - JSON → ZWO extraction → ImporterWorkoutZwo → Workout model.
#       - Empty list and missing workoutContent field handled gracefully.
#   • XmlUtil round-trip (native .xml format):
#       - Workout::createWorkoutXml + parseSingleWorkoutXml.
#       - Interval count, power fractions, durations, workout name, and plan.
#
# No display required — tests run headlessly (no Xvfb needed).
#
# Build (Linux / macOS):
#   qmake workout_io_tests.pro && make
# Build (Windows -- MSVC developer prompt):
#   qmake workout_io_tests.pro && nmake
# Run:
#   ../../build/tests/workout_io_tests -v2
###############################################################################

QT       += core gui widgets network sql testlib
CONFIG   += qt c++17
CONFIG   -= app_bundle

TARGET   = workout_io_tests
TEMPLATE = app

DESTDIR     = ../../build/tests
OBJECTS_DIR = .obj_workout_io
MOC_DIR     = .moc_workout_io

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
    ../../src/model/powercurve.cpp \
    ../../src/fitness/achievements/achievement.cpp \
    ../../src/persistence/db/environnement.cpp \
    ../../src/persistence/db/extrequest.cpp \
    ../../src/persistence/db/userdao.cpp \
    ../../src/persistence/db/versiondao.cpp \
    ../../src/persistence/db/intervalsicudao.cpp \
    ../../src/persistence/file/importerworkoutzwo.cpp \
    ../../src/persistence/file/xmlutil.cpp \
    ../../src/ui/workout_editor/repeatwidget.cpp \
    ../intervals_icu/credential_store_stub.cpp \
    tst_workout_io.cpp

HEADERS += \
    ../../src/app/logger.h \
    ../../src/app/util.h \
    ../../src/model/account.h \
    ../../src/model/settings.h \
    ../../src/model/workout.h \
    ../../src/model/interval.h \
    ../../src/persistence/file/importerworkoutzwo.h \
    ../../src/persistence/file/xmlutil.h \
    ../../src/ui/workout_editor/repeatwidget.h

FORMS += \
    ../../src/ui/workout_editor/repeatwidget.ui
