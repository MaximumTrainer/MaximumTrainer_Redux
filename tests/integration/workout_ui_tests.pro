###############################################################################
# tests/integration/workout_ui_tests.pro
#
# Workout UI Integration Tests -- MaximumTrainer
#
# Covers user-login flow (offline), workout XML creation / retrieval,
# ERG cycle-trainer simulation (setLoad / setSlope), the full
# workout-session lifecycle (start / pause / resume / stop), and the
# following extended areas:
#
#   • QWT power-curve plot:  WorkoutExecutionWindow embeds a QwtPlot that
#     renders actual vs target power as the session runs.
#
#   • Workout model round-trip:  Workout and Interval objects are
#     constructed in-process, serialised with XmlUtil::createWorkoutXml,
#     and parsed back with XmlUtil::parseSingleWorkoutXml.  Every field
#     (plan, name, description, type, interval durations, power fractions,
#     cadence targets) is verified end-to-end.
#
#   • Workout metrics:  Workout::calculateWorkoutMetrics() is called on a
#     flat-power workout and the resulting averagePower is verified.
#
#   • Network connectivity:  An HTTPS GET to the intervals.icu REST API is
#     made using QNetworkAccessManager.  The test calls QSKIP gracefully
#     when INTERVALS_ICU_API_KEY / INTERVALS_ICU_ATHLETE_ID are absent.
#
#   • Network workout retrieval:  GET /athlete/{id}/workouts returns a
#     JSON array.  Field count and schema are validated.
#
#   • Power-on-target:  After a 3-second ERG session the last reported
#     actual power is within ±25 % of the target.
#
#   • A 1280×720 screenshot of the workout-execution window (including the
#     QWT power-curve plot) is saved as build evidence.
#
# Build (Linux):
#   qmake workout_ui_tests.pro [QWT_INSTALL=/path/to/qwt] && make -j$(nproc)
# Build (macOS):
#   qmake workout_ui_tests.pro [QWT_INSTALL=/path/to/qwt] && make -j$(sysctl -n hw.logicalcpu)
# Build (Windows -- MSVC developer prompt):
#   qmake workout_ui_tests.pro QWT_INSTALL=C:/qwt && jom /J %NUMBER_OF_PROCESSORS%
#
# Run headless (Linux CI):
#   Xvfb :99 -screen 0 1280x800x24 &
#   export DISPLAY=:99
#   ../../build/tests/workout_ui_tests -v2
# Run directly (Windows / macOS CI -- display is always available):
#   .\build\tests\workout_ui_tests.exe -v2
###############################################################################

QT       += core gui widgets network sql testlib
CONFIG   += qt c++17
CONFIG   -= app_bundle

TARGET   = workout_ui_tests
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
    ../../src/ui/workout_editor
SOURCES += \
    ../../src/app/logger.cpp \
    ../../src/app/util.cpp \
    ../../src/app/simplecrypt.cpp \
    ../../src/btle/simulator_hub.cpp \
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
    ../../src/persistence/file/xmlutil.cpp \
    ../../src/ui/workout_editor/repeatwidget.cpp \
    ../intervals_icu/credential_store_stub.cpp \
    tst_workout_ui.cpp

HEADERS += \
    ../../src/app/logger.h \
    ../../src/app/util.h \
    ../../src/btle/simulator_hub.h \
    ../../src/model/account.h \
    ../../src/model/settings.h \
    ../../src/model/workout.h \
    ../../src/model/interval.h \
    ../../src/persistence/file/xmlutil.h \
    ../../src/ui/workout_editor/repeatwidget.h

FORMS += \
    ../../src/ui/workout_editor/repeatwidget.ui
