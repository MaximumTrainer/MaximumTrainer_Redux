include(fuzz.pri)
TARGET = ble_notification_fuzz
QT    += core bluetooth
INCLUDEPATH += ../../src/btle ../../src/btle/zwift ../../src/app
SOURCES += \
    ../../src/app/logger.cpp \
    ../../src/btle/btle_hub.cpp \
    ble_notification_fuzz.cpp
HEADERS += \
    ../../src/app/logger.h \
    ../../src/btle/btle_hub.h
