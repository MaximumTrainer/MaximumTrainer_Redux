INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

include($$PWD/components/components.pri)
include($$PWD/plots/plots.pri)
include($$PWD/workout_editor/workout_editor.pri)

SOURCES += $$PWD/mainwindow.cpp\
$$PWD/workoutdialog.cpp \
    $$PWD/main_workoutpage.cpp \
    $$PWD/dialogconfig.cpp \
    $$PWD/dialoglogin.cpp \
    $$PWD/z_stylesheet.cpp \
    $$PWD/updatedialog.cpp \
    $$PWD/savingwindow.cpp \
    $$PWD/splashscreen.cpp \
    $$PWD/dialogmainwindowconfig.cpp \
    $$PWD/dialoginfowebview.cpp \
    $$PWD/dialog_connection_method.cpp \
    $$PWD/tab_intervals_icu.cpp \
    $$PWD/dialogkeyboardshortcuts.cpp \
    #$$PWD/main_coursepage.cpp

HEADERS += $$PWD/mainwindow.h\
$$PWD/workoutdialog.h \
    $$PWD/main_workoutpage.h \
    $$PWD/dialogconfig.h \
    $$PWD/dialoglogin.h \
    $$PWD/z_stylesheet.h \
    $$PWD/updatedialog.h \
    $$PWD/savingwindow.h \
    $$PWD/splashscreen.h \
    $$PWD/dialogmainwindowconfig.h \
    $$PWD/dialoginfowebview.h \
    $$PWD/dialog_connection_method.h \
    $$PWD/tab_intervals_icu.h \
    $$PWD/dialogkeyboardshortcuts.h \
    #$$PWD/main_coursepage.h

FORMS    += $$PWD/mainwindow.ui \
    $$PWD/workoutdialog.ui \
    $$PWD/main_workoutpage.ui \
    $$PWD/dialogconfig.ui \
    $$PWD/dialoglogin.ui \
    $$PWD/z_stylesheet.ui \
    $$PWD/updatedialog.ui \
    $$PWD/savingwindow.ui \
    $$PWD/dialogmainwindowconfig.ui \
    $$PWD/dialoginfowebview.ui \
    $$PWD/tab_intervals_icu.ui \
    #$$PWD/main_coursepage.ui


