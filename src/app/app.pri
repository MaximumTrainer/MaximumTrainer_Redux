INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

SOURCES += $$PWD/main.cpp\
    $$PWD/logger.cpp \
    $$PWD/util.cpp \
    $$PWD/globalvars.cpp \
    $$PWD/simplecrypt.cpp \
    $$PWD/reportutil.cpp \
    $$PWD/networkmonitor.cpp \

# SoundPlayer: full SFML implementation on native, no-op stub on Wasm
contains(QMAKE_PLATFORM, wasm) {
    SOURCES += $$PWD/soundplayer_wasm.cpp
} else {
    SOURCES += $$PWD/soundplayer.cpp
}

# CredentialStore: platform-native secure storage (DPAPI / Keychain / AES-GCM)
contains(QMAKE_PLATFORM, wasm) {
    SOURCES += $$PWD/credential_store_wasm.cpp
} else: win32 {
    SOURCES += $$PWD/credential_store_win.cpp
} else: macx {
    OBJECTIVE_SOURCES += $$PWD/credential_store_mac.mm
} else {
    SOURCES += $$PWD/credential_store_linux.cpp
    LIBS += -lcrypto
}
contains(DEFINES, GC_HAVE_VLCQT) {
    SOURCES += $$PWD/myvlcplayer.cpp
}
# Always include header so MOC generates the meta-object / vtable for the stub class
HEADERS += $$PWD/myvlcplayer.h




HEADERS += $$PWD/util.h \
    $$PWD/logger.h \
    $$PWD/soundplayer.h \
    $$PWD/globalvars.h \
    $$PWD/simplecrypt.h \
    $$PWD/reportutil.h \
    $$PWD/myconstants.h \
    $$PWD/networkmonitor.h \
    $$PWD/credential_store.h \
    $$PWD/env_config.h



macx {

    HEADERS += $$PWD/macutils.h \

    #Call to native objective-c for mac
    OBJECTIVE_SOURCES += $$PWD/MacUtils.mm \
}



