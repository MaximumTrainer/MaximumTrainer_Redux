###############################################################################
# tests/studio/studio_tests.pro
#
# Studio Mode unit tests – MaximumTrainer (#135)
#
# Tests verify:
#   1. Multiple SimulatorHub instances run and emit signals simultaneously.
#   2. Each hub's userID is correctly propagated in emitted signals.
#   3. The ERG FTP-scaling formula (load = %FTP × riderFTP) is correct.
#   4. Studio Mode QSettings keys persist across simulated save/load cycles.
#   5. setLoad() nudges the simulated power toward the requested target.
#   6. Stopping one hub does not affect the other.
#
# Build:
#   qmake studio_tests.pro && make
# Run:
#   ../../build/tests/studio_tests -v2
###############################################################################

QT       += core testlib
QT       -= gui

CONFIG   += qt c++17 console
CONFIG   -= app_bundle

TARGET   = studio_tests
TEMPLATE = app

DESTDIR  = ../../build/tests

INCLUDEPATH += ../../src/btle ../../src/app

SOURCES += \
    ../../src/app/logger.cpp \
    ../../src/btle/simulator_hub.cpp \
    tst_studio_mode.cpp

HEADERS += \
    ../../src/app/logger.h \
    ../../src/btle/simulator_hub.h
