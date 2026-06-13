# Retro ghost-race (QML/QQuickWidget) — built on every target, including WASM.
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
