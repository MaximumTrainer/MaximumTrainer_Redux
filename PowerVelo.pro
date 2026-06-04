#                                                                             #
#              ***   CORE CONFIGURATION  ***                                  #
#                                                                             #
###############################################################################

# Embed the git version tag into the binary so the app can report its own
# version and compare it against the latest GitHub release at startup.
#
# In GitHub Actions CI, actions/checkout@v4 writes safe.directory only to a
# temporary gitconfig used during the checkout step, not to the real
# ~/.gitconfig seen by subsequent build steps.  This causes qmake's
# $$system(git describe ...) to return an empty string, triggering the
# "v0.0.0" fallback even when the correct tag is available.
#
# The CI fix: each workflow exports GIT_TAG to the environment before running
# qmake (using a "Determine build version" step that calls git describe in
# bash, where it works reliably).  PowerVelo.pro reads that env-var first and
# only falls back to $$system(git describe ...) for local development builds
# where GIT_TAG is not set.
GIT_VERSION = $$(GIT_TAG)
isEmpty(GIT_VERSION) {
    # Local development fallback: run git describe directly.
    GIT_VERSION = $$system(git describe --tags --match "v[0-9]*" --always)
    # Strip any trailing carriage-return that qmake's $$system() may capture
    # on Windows when git writes "\r\n" to stdout.
    GIT_VERSION = $$replace(GIT_VERSION, \\r, )
    # If this is not an exact tag commit, git describe appends "-N-gHASH" to
    # the nearest tag (e.g. "v0.0.53-3-gabcdef1").  Strip that suffix so
    # APP_VERSION is always a clean "vX.Y.Z" string.
    GIT_VERSION = $$section(GIT_VERSION, -, 0, 0)
    isEmpty(GIT_VERSION): GIT_VERSION = "v0.0.0"
    # If git describe found no matching tag it falls back to a bare commit
    # hash (e.g. "8f6e749").  A commit hash does not start with "v" followed
    # by a digit, so treat it as an unknown version.
    !contains(GIT_VERSION, ^[vV][0-9]): GIT_VERSION = "v0.0.0"
}
DEFINES += APP_VERSION=\\\"$$GIT_VERSION\\\"

# TrainingPeaks client secret — supplied at build time via environment variable.
# In GitHub Actions CI, set the TP_CLIENT_SECRET secret and expose it as an
# environment variable in the qmake step.  For local development, export
# TP_CLIENT_SECRET before running qmake.  An empty secret disables the token
# refresh flow but does not prevent file uploads with an existing access token.
TP_SECRET_VAL = $$(TP_CLIENT_SECRET)
!isEmpty(TP_SECRET_VAL) {
    DEFINES += TP_CLIENT_SECRET=\\\"$$TP_SECRET_VAL\\\"
} else {
    DEFINES += TP_CLIENT_SECRET=\\\"\\\"
}

# Intervals.icu OAuth2 client credentials — supplied at build time via env vars.
# In GitHub Actions CI, add INTERVALS_OAUTH_CLIENT_ID and INTERVALS_OAUTH_CLIENT_SECRET
# as repository secrets and expose them on the qmake step.  For local development,
# export the variables before running qmake.
# Fallback: client_id=259 (existing public client registration), client_secret=""
# (public client — no secret required).  The fallbacks preserve existing behaviour
# when the secrets are absent.
ICV_CLIENT_ID_VAL = $$(INTERVALS_OAUTH_CLIENT_ID)
!isEmpty(ICV_CLIENT_ID_VAL) {
    DEFINES += INTERVALS_OAUTH_CLIENT_ID=\\\"$$ICV_CLIENT_ID_VAL\\\"
} else {
    DEFINES += INTERVALS_OAUTH_CLIENT_ID=\\\"259\\\"
}

ICV_CLIENT_SECRET_VAL = $$(INTERVALS_OAUTH_CLIENT_SECRET)
!isEmpty(ICV_CLIENT_SECRET_VAL) {
    DEFINES += INTERVALS_OAUTH_CLIENT_SECRET=\\\"$$ICV_CLIENT_SECRET_VAL\\\"
} else {
    DEFINES += INTERVALS_OAUTH_CLIENT_SECRET=\\\"\\\"
}

# Derive qmake's VERSION variable from GIT_VERSION.  This is used by qmake to
# populate the Windows RC file so the version appears in the .exe "Details" tab
# of the file-properties dialog.  The leading "v" is stripped and any
# git-describe suffix (e.g. "-14-gabcdef") is removed, leaving "MAJOR.MINOR.PATCH".
# If the result is not a proper dotted-numeric version (e.g. a bare commit hash
# is returned by git-describe when no tag is found), fall back to "0.0.0" so the
# MSVC linker /VERSION option never receives a non-numeric argument.
NUMERIC_VERSION = $$replace(GIT_VERSION, ^[vV], "")
NUMERIC_VERSION = $$section(NUMERIC_VERSION, -, 0, 0)
!contains(NUMERIC_VERSION, ^[0-9]+\\.[0-9]+): NUMERIC_VERSION = "0.0.0"
VERSION = $$NUMERIC_VERSION

qtHaveModule(uitools):!embedded: QT += uitools
else: DEFINES += QT_NO_UITOOLS

# Used if you want console output on windows
#CONFIG += console

# QWT: on Linux the apt qwt package ships a qmake feature (qwt.prf) enabled via
# "CONFIG += qwt". For Qt6 there is no qwt apt package, so QWT must be built
# from source and its location passed via QWT_INSTALL=... — in that case we
# wire the include/lib paths directly and must NOT add "CONFIG += qwt" (no
# feature file exists). See the QWT_INSTALL block further down.
isEmpty(QWT_INSTALL): CONFIG += qwt
CONFIG += qt thread
CONFIG += release
CONFIG += c++17

#INCLUDEPATH	+= /usr/lib/x86_64-linux-gnu/qt5

# include
QT       += core gui widgets
QT       += network printsupport
# Only add modules that exist in the current Qt installation; this makes the
# .pro file work for both full Qt builds and the WASM singlethread target
# (which lacks concurrent, bluetooth, and webenginewidgets).
qtHaveModule(concurrent):       QT += concurrent
qtHaveModule(bluetooth):        QT += bluetooth
qtHaveModule(webenginewidgets): QT += webenginewidgets
qtHaveModule(webenginecore):    QT += webenginecore

# QtMultimedia provides the native video + radio-stream player (QMediaPlayer /
# QVideoWidget / QAudioOutput). Guarded so the WASM target (no multimedia) and
# any Qt without the module are unaffected. The GC_HAVE_QTMULTIMEDIA define is
# set further below once the platform blocks have run.
qtHaveModule(multimedia):        QT += multimedia
qtHaveModule(multimediawidgets): QT += multimediawidgets

#QT += serialport
#QT += multimedia
#QT += multimediawidgets
#QT += webengine
#QT += webenginecore

# ─── WebAssembly overrides ───────────────────────────────────────────────────
# Qt 6.5+ names the singlethread WASM mkspec "wasm-emscripten-singlethread"
# (scope: wasm_emscripten_singlethread); older versions use "wasm-emscripten"
# (scope: wasm_emscripten).  contains(QMAKE_PLATFORM, wasm) covers both.
contains(QMAKE_PLATFORM, wasm) | wasm_emscripten | wasm_emscripten_singlethread {
    # Stubs make #include <QWebEngineView> etc. resolve to no-op classes
    INCLUDEPATH = $$PWD/src/ui/wasm_stubs $$INCLUDEPATH
    INCLUDEPATH += $$PWD/src/ui/wasm_stubs/QtWebEngineWidgets

    # The WebBluetooth bridge uses EM_JS with JavaScript async/await internally;
    # C++ never blocks waiting for async results (all results arrive via bleNotifyC
    # callbacks), so ASYNCIFY is not required.

    # libqwasm.a (Qt's WASM platform plugin) and webbluetooth_bridge.cpp both use
    # emscripten::val / emscripten/val.h, which requires the embind runtime library.
    # Without this, wasm-ld reports undefined symbols _emval_new, _emval_call_void_method etc.
    LIBS += -lembind

    # QWT for WASM is built from source and passed via QWT_INSTALL=... . The
    # platform blocks below are guarded against wasm, so wire the include/lib
    # paths here. (No qwt feature file exists for the from-source build, hence
    # the QWT_INSTALL gate at the top skips "CONFIG += qwt".)
    !isEmpty(QWT_INSTALL) {
        INCLUDEPATH += $${QWT_INSTALL}/include
        INCLUDEPATH += $${QWT_INSTALL}/include/qwt
        LIBS += -L$${QWT_INSTALL}/lib -lqwt
    }

    DEFINES += GC_WASM_BUILD
}
# ────────────────────────────────────────────────────────────────────────────

# QT_NO_DEBUG_OUTPUT was removed: the Logger message handler now controls
# verbosity at runtime, including qDebug() messages.  The default runtime
# level (Info) discards debug messages in production without a recompile.

TARGET = MaximumTrainer
TEMPLATE = app

!isEmpty(APP_NAME) { TARGET = $${APP_NAME} }
CONFIG(debug, debug|release) { QMAKE_CXXFLAGS += -DGC_DEBUG }

qtHaveModule(uitools):!embedded: QT += uitools
else: DEFINES += QT_NO_UITOOLS


###=============================================================
### Obuild directory
###=============================================================

CONFIG(debug, debug|release) {
    DESTDIR = build/debug
}
CONFIG(release, debug|release) {
    DESTDIR = build/release
}

OBJECTS_DIR = $$DESTDIR/.obj
MOC_DIR = $$DESTDIR/.moc
RCC_DIR = $$DESTDIR/.qrc
UI_DIR = $$DESTDIR/.u


###=======================================================================
### Directory Structure - Split into subdirs to be more manageable
###=======================================================================
QMAKE_CFLAGS_ISYSTEM =



# to make sure we are toolchain neutral NEVER refer to a lib
# via file extensions .lib or .a in src.pro unless the section is
# platform specific. Instead we use directives -Ldir and -llib
win32 {
    # QWT: built from source against Qt6 and pointed at via QWT_INSTALL=... .
    # Fall back to the legacy ../qwt sibling layout when QWT_INSTALL is empty.
    !isEmpty(QWT_INSTALL) {
        INCLUDEPATH += $${QWT_INSTALL}/include
        INCLUDEPATH += $${QWT_INSTALL}/include/qwt
        DEFINES += QWT_DLL
        CONFIG(release, debug|release) {
            LIBS += -L$${QWT_INSTALL}/lib -lqwt
        }
        CONFIG(debug, debug|release) {
            LIBS += -L$${QWT_INSTALL}/lib -lqwtd
        }
    } else {
        #QWT is configured to build 2 libs (release/debug) on win32 (see qwtbuild.pri)
        INCLUDEPATH += ../qwt/include
        INCLUDEPATH += ../qwt/include/qwt

        CONFIG(release, debug|release){
            LIBS += -L$${PWD}/../qwt/lib -lqwt
        }
        CONFIG(debug, debug|release) {
            LIBS += -L$${PWD}/../qwt/lib -lqwtd
        }
    }
}

# compress and math libs must be defined in gcconfig.pri
# if they're not part of the QT include
INCLUDEPATH += $${LIBZ_INCLUDE}
LIBS += $${LIBZ_LIBS}

# GNU Scientific Library
INCLUDEPATH += $${GSL_INCLUDES}
LIBS += $${GSL_LIBS}

###===============================
### PLATFORM SPECIFIC DEPENDENCIES
###===============================

# Microsoft Visual Studion toolchain dependencies
win32-msvc* {

    # Gdi32 and User32 are standard system libs resolved automatically by the
    # MSVC linker via the LIB environment variable.  No explicit -L path needed.
    # Crypt32 is required for DPAPI (CryptProtectData) in CredentialStore.
    LIBS += -lGdi32 -lUser32 -lCrypt32
    CONFIG += force_debug_info

    # Embed the application icon into the Windows executable.
    RC_ICONS = $$PWD/resources/myappico.ico


} else {

    # gnu toolchain wants math libs
    LIBS += -lm

    unix:!macx:!contains(QMAKE_PLATFORM, wasm) {
        # Linux gcc 5 grumbles about unused static globals and leads
        # to a gazillion warnings that are harmless so lets remove them
        QMAKE_CXXFLAGS += -Wno-unused-variable

        # Linux Flex compiler grumbles about unsigned comparisons
        QMAKE_CXXFLAGS += -Wno-sign-compare

        # QWT: when built from source for Qt6 (no qwt apt package exists for
        # Qt6), point qmake at it with QWT_INSTALL=... . The Qt5 apt build
        # leaves QWT_INSTALL empty and uses "CONFIG += qwt" instead (see top).
        !isEmpty(QWT_INSTALL) {
            INCLUDEPATH += $${QWT_INSTALL}/include
            LIBS += -L$${QWT_INSTALL}/lib -lqwt
            DEFINES += QWT_DLL
        }
    }
}

#////////////////////////////////////////////////////////////////////////////////////////////////////////

macx:!wasm_emscripten {

    # Embed the Info.plist so macOS 12+ grants CoreBluetooth access
    # (NSBluetoothAlwaysUsageDescription is required; without it BLE scanning
    # silently returns an empty device list on Monterey and later).
    QMAKE_INFO_PLIST = $$PWD/mac/Info.plist

    # Embed the application icon into the macOS bundle.
    ICON = $$PWD/resources/myappico.icns

    # Bluetooth entitlement required for App Store distribution
    QMAKE_CODE_SIGN_ENTITLEMENTS = $$PWD/mac/MaximumTrainer.entitlements

    # on mac we use native buttons and video, but have native fullscreen support
    LIBS    += -lobjc -framework IOKit -framework AppKit -framework Security

    # QWT: configure directly against the flat (non-framework) install.
    !isEmpty(QWT_INSTALL) {
        INCLUDEPATH += $${QWT_INSTALL}/include
        LIBS += -L$${QWT_INSTALL}/lib -lqwt
        DEFINES += QWT_DLL
    }

}


# ── Video/radio player backend ───────────────────────────────────────────────
# The embedded video player and internet radio use QtMultimedia
# (QMediaPlayer/QVideoWidget/QAudioOutput) on every desktop platform. Enable it
# whenever the module is present and we are not building the WASM target.
qtHaveModule(multimedia):!contains(QMAKE_PLATFORM, wasm) {
    DEFINES += GC_HAVE_QTMULTIMEDIA
}


# X11
if (defined(GC_WANT_X11)) {
    LIBS += -lX11
}



include(src/app/app.pri)
include(src/btle/btle.pri)
include(src/model/model.pri)
include(src/persistence/persistence.pri)
include(src/fitness/fitness.pri)
include(src/ui/ui.pri)
include(src/workout/workout.pri)


RESOURCES += MyResources.qrc
