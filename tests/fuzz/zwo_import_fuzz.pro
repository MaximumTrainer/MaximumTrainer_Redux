include(fuzz.pri)
TARGET = zwo_import_fuzz
QT    += core network xml
INCLUDEPATH += \
    ../intervals_icu \
    ../../src/persistence/db \
    ../../src/persistence/file \
    ../../src/model \
    ../../src/app
SOURCES += \
    ../../src/persistence/file/importerworkoutzwo.cpp \
    ../../src/model/workout.cpp \
    ../../src/model/interval.cpp \
    ../../src/model/account.cpp \
    ../../src/model/repeatdata.cpp \
    ../../src/app/logger.cpp \
    ../intervals_icu/credential_store_stub.cpp \
    zwo_import_fuzz.cpp
HEADERS += \
    ../../src/model/account.h \
    ../../src/model/workout.h
