# Group workouts: rider connection to the group-relay Worker (workers/group-relay).
QT += websockets

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/groupprotocol.h \
    $$PWD/groupsession.h \
    $$PWD/groupsimtest.h \
    $$PWD/groupconfig.h \
    $$PWD/groupworkoutcodec.h \
    $$PWD/grouppanel.h \
    $$PWD/joingroupdialog.h \
    $$PWD/groupridersmodel.h \
    $$PWD/groupriderdelegate.h

SOURCES += \
    $$PWD/groupprotocol.cpp \
    $$PWD/groupsession.cpp \
    $$PWD/groupsimtest.cpp \
    $$PWD/groupworkoutcodec.cpp \
    $$PWD/grouppanel.cpp \
    $$PWD/joingroupdialog.cpp \
    $$PWD/groupridersmodel.cpp \
    $$PWD/groupriderdelegate.cpp
