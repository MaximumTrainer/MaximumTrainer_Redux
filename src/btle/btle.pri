INCLUDEPATH += $$PWD $$PWD/zwift

# On Wasm use the WebBluetooth bridge; on all other platforms use native Qt BLE
contains(QMAKE_PLATFORM, wasm) {
    SOURCES += \
        $$PWD/btle_hub_wasm.cpp \
        $$PWD/webbluetooth_bridge.cpp \
        $$PWD/btle_scanner_dialog_wasm.cpp \
        $$PWD/simulator_hub.cpp

    HEADERS += \
        $$PWD/btle_hub_wasm.h \
        $$PWD/webbluetooth_bridge.h \
        $$PWD/btle_scanner_dialog_wasm.h \
        $$PWD/btle_uuids.h \
        $$PWD/simulator_hub.h
} else {
    SOURCES += \
        $$PWD/btle_hub.cpp \
        $$PWD/btle_scanner_dialog.cpp \
        $$PWD/btle_sensor_store.cpp \
        $$PWD/sensor_connect_dialog.cpp \
        $$PWD/simulator_hub.cpp

    HEADERS += \
        $$PWD/btle_hub.h \
        $$PWD/btle_scanner_dialog.h \
        $$PWD/btle_sensor_config.h \
        $$PWD/btle_sensor_store.h \
        $$PWD/sensor_connect_dialog.h \
        $$PWD/btle_uuids.h \
        $$PWD/simulator_hub.h

    # Read-only Zwift exploration harness (Phase 1) needs native QtBluetooth.
    SOURCES += $$PWD/zwift/zwift_probe.cpp
    HEADERS += $$PWD/zwift/zwift_probe.h
}

# Zwift virtual-shifting protocol codec — pure bytes, platform-independent.
SOURCES += $$PWD/zwift/zwift_protocol.cpp
HEADERS += $$PWD/zwift/zwift_protocol.h

FORMS += \
    $$PWD/btle_scanner_dialog.ui
