include(fuzz.pri)
TARGET = fit_decode_fuzz
QT    += core
include(../../src/fitness/fit/fit.pri)
SOURCES += fit_decode_fuzz.cpp
