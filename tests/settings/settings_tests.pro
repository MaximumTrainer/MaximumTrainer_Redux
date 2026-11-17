###############################################################################
# tests/settings/settings_tests.pro
#
# Account preference persistence through the typed setting keys
# (src/app/setting_keys.h): defaults on a fresh store, on-disk key names, and a
# save/load round trip. QSettings is redirected to a temporary directory.
#
# Uses the stub util.h / QApplication from tests/intervals_icu/ (same pattern as
# tests/integration/workout_creator_tests.pro).
###############################################################################
QT       += core testlib
QT       -= gui

CONFIG   += qt c++20 console
CONFIG   -= app_bundle

TARGET   = settings_tests
TEMPLATE = app

DESTDIR     = ../../build/tests
OBJECTS_DIR = .obj_settings
MOC_DIR     = .moc_settings

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
    ../../src/app/logger.cpp \
    ../../src/model/workout.cpp \
    ../../src/model/interval.cpp \
    ../../src/model/repeatdata.cpp \
    ../intervals_icu/credential_store_stub.cpp \
    tst_settings.cpp

HEADERS += \
    ../../src/model/account.h \
    ../../src/model/workout.h \
    ../../src/model/interval.h
