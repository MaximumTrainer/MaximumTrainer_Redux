###############################################################################
# tests/integration/login_screen_tests.pro
#
# Login Screen Full-Application Test -- MaximumTrainer
#
# Validates that a user can log into the MaximumTrainer application and
# access key functionality without error, across Windows, macOS, and Linux:
#
#   1. Offline login path: exercises the offline-mode account setup logic
#      end-to-end, then starts SimulatorHub to confirm key functionality
#      (sensor data) is accessible once logged in.
#   2. Intervals.icu OAuth URL validation: builds the OAuth2 authorization
#      URL via Environnement and verifies all required parameters are present.
#   3. Intervals.icu API login: makes a real HTTPS call to the intervals.icu
#      API using credentials from environment variables (QSKIP when absent).
#   4. DialogLogin initial state: shows the real widget and verifies that
#      widget_loading is visible and widget_bottom is hidden before network
#      activity completes.
#   5. DialogLogin offline flow: clicks checkBox_workOffline and
#      pushButton_startOffline on the real widget, verifies the Account
#      object is set to offline mode.
#   6. DialogLogin Intervals.icu button: verifies the button is visible and
#      enabled when the widget is in test mode.
#   7. DialogLogin Intervals.icu OAuth dialog: clicks the button and verifies
#      the OAuth child dialog is created with the correct URL.
#
# A labelled 1280×720 screenshot is saved as build evidence for each test.
#
# Build (Linux / macOS):
#   qmake login_screen_tests.pro && make
# Build (Windows -- MSVC developer prompt):
#   qmake login_screen_tests.pro && nmake
#
# Run headless (Linux CI):
#   Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
#   ../../build/tests/login_screen_tests -v2
# Run directly (Windows / macOS CI -- display is always available):
#   .\build\tests\login_screen_tests.exe -v2
###############################################################################

QT       += core gui widgets network webenginewidgets testlib
CONFIG   += qt c++17
CONFIG   -= app_bundle

TARGET   = login_screen_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

# ── QWT: platform-specific include / link ───────────────────────────────────
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
    ../../src/btle/simulator_hub.cpp \
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
    ../../src/ui/dialoglogin.cpp \
    ../../src/ui/updatedialog.cpp \
    ../../src/ui/dialoginfowebview.cpp \
    ../../src/ui/components/languagecombobox.cpp \
    ../../src/ui/workout_editor/repeatwidget.cpp \
    ../intervals_icu/credential_store_stub.cpp \
    tst_login_screen.cpp

HEADERS += \
    ../../src/btle/simulator_hub.h \
    ../../src/model/account.h \
    ../../src/model/settings.h \
    ../../src/persistence/file/xmlutil.h \
    ../../src/ui/dialoglogin.h \
    ../../src/ui/updatedialog.h \
    ../../src/ui/dialoginfowebview.h \
    ../../src/ui/components/languagecombobox.h \
    ../../src/ui/components/myqwebenginepage.h \
    ../../src/ui/workout_editor/repeatwidget.h

FORMS += \
    ../../src/ui/dialoglogin.ui \
    ../../src/ui/updatedialog.ui \
    ../../src/ui/dialoginfowebview.ui \
    ../../src/ui/workout_editor/repeatwidget.ui
