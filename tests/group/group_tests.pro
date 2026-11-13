###############################################################################
# tests/group/group_tests.pro
#
# Group-workout protocol + session tests. Offline by default; the live session
# test runs only when GROUP_RELAY_URL points at a relay, e.g. a local
#   (cd workers/group-relay && npx wrangler dev)  ->  GROUP_RELAY_URL=ws://127.0.0.1:8787
#
# Build:  qmake group_tests.pro && make
# Run:    ./../../build/tests/group_tests -v2
###############################################################################

QT       += core network websockets testlib
QT       -= gui

CONFIG   += qt c++20 console
CONFIG   -= app_bundle

TARGET   = group_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += . ../../src/group

SOURCES += \
    ../../src/group/groupprotocol.cpp \
    ../../src/group/groupsession.cpp \
    tst_group.cpp

HEADERS += \
    ../../src/group/groupprotocol.h \
    ../../src/group/groupsession.h
