###############################################################################
# tests/apptheme/apptheme_tests.pro
#
# Standalone Qt Test project for the AppTheme dark/light theming helper.
# AppTheme is header-only (src/ui/apptheme.h) and depends only on
# Qt Gui + Widgets — no QWT, Bluetooth, or VLC-Qt.
#
# Build:
#   qmake apptheme_tests.pro && make
# Run (headless Linux CI):
#   Xvfb :99 -screen 0 1280x800x24 & export DISPLAY=:99
#   ../../build/tests/apptheme_tests -v2
###############################################################################

QT       += core gui widgets testlib

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = apptheme_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += ../../src/ui

HEADERS += ../../src/ui/apptheme.h
SOURCES += tst_apptheme.cpp
