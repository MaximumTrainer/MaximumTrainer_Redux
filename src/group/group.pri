# Group workouts: rider connection to the group-relay Worker (workers/group-relay).
QT += websockets

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/groupprotocol.h \
    $$PWD/groupsession.h \
    $$PWD/groupsimtest.h

SOURCES += \
    $$PWD/groupprotocol.cpp \
    $$PWD/groupsession.cpp \
    $$PWD/groupsimtest.cpp
