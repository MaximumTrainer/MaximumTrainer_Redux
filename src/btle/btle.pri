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

    # Zwift Click/Play/Ride controller INPUT (its own BLE peripheral — never
    # touches the trainer connection, so ERG/FTMS stays live). Buttons only.
    SOURCES += $$PWD/zwift/zwift_click_protocol.cpp \
               $$PWD/zwift/zwift_click_relay.cpp \
               $$PWD/zwift/zwift_click_hub.cpp \
               $$PWD/zwift/zwift_click_manager.cpp \
               $$PWD/zwift/zwift_click_test.cpp \
               $$PWD/zwift/trainer_click_probe.cpp
    HEADERS += $$PWD/zwift/zwift_click_protocol.h \
               $$PWD/zwift/zwift_click_relay.h \
               $$PWD/zwift/zwift_click_hub.h \
               $$PWD/zwift/zwift_click_manager.h \
               $$PWD/zwift/zwift_click_test.h \
               $$PWD/zwift/trainer_click_probe.h
}

# Virtual-shifting gear table (FTMS resistance/power) — header-only, platform-independent.
HEADERS += $$PWD/zwift/virtual_gear.h

FORMS += \
    $$PWD/btle_scanner_dialog.ui
