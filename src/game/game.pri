# Retro ghost-race spike (desktop only — see the guarded include in the .pro).
QT += quick quickwidgets

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/ghostreplay.h \
    $$PWD/cyclingphysics.h \
    $$PWD/retroracecontroller.h

SOURCES += \
    $$PWD/ghostreplay.cpp \
    $$PWD/retroracecontroller.cpp

RESOURCES += $$PWD/game.qrc
