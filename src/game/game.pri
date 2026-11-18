# Retro ghost-race (QML/QQuickWidget) — built on every target, including WASM.
QT += quick quickwidgets

INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/ghostreplay.h \
    $$PWD/cyclingphysics.h \
    $$PWD/retroracecontroller.h \
    $$PWD/roadsurface.h

SOURCES += \
    $$PWD/ghostreplay.cpp \
    $$PWD/retroracecontroller.cpp \
    $$PWD/roadsurface.cpp


