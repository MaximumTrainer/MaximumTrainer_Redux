# src/fitness/fitness.pri — Fitness domain (FIT protocol + achievements)
INCLUDEPATH += $$PWD
include($$PWD/fit/fit.pri)
include($$PWD/achievements/achievements.pri)

SOURCES += $$PWD/pmccalculator.cpp
HEADERS += $$PWD/pmccalculator.h
