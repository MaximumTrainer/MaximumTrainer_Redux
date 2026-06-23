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
    $$PWD/splashscreen.cpp \
    $$PWD/dialogmainwindowconfig.cpp \
    $$PWD/dialog_connection_method.cpp \
    $$PWD/tab_intervals_icu.cpp \
    $$PWD/workouthistorymodel.cpp \
    $$PWD/historywidget.cpp \
    $$PWD/historyfilterproxymodel.cpp \
    $$PWD/activitydetaildialog.cpp \
    $$PWD/dialogkeyboardshortcuts.cpp \
    $$PWD/planadherencewidget.cpp \
    $$PWD/criticalpowerdialog.cpp \
    $$PWD/pmcdialog.cpp \
    $$PWD/workoutcountdowndialog.cpp \
    $$PWD/queuepanelwidget.cpp

HEADERS += $$PWD/mainwindow.h\
$$PWD/workoutdialog.h \
    $$PWD/main_workoutpage.h \
    $$PWD/dialogconfig.h \
    $$PWD/dialoglogin.h \
    $$PWD/z_stylesheet.h \
    $$PWD/updatedialog.h \
    $$PWD/splashscreen.h \
    $$PWD/dialogmainwindowconfig.h \
    $$PWD/dialog_connection_method.h \
    $$PWD/tab_intervals_icu.h \
    $$PWD/workouthistorymodel.h \
    $$PWD/historywidget.h \
    $$PWD/historyfilterproxymodel.h \
    $$PWD/activitydetaildialog.h \
    $$PWD/dialogkeyboardshortcuts.h \
    $$PWD/criticalpowerdialog.h \
    $$PWD/pmcdialog.h \
    $$PWD/workoutcountdowndialog.h \
    $$PWD/queuepanelwidget.h \
    $$PWD/apptheme.h \
    $$PWD/planadherencewidget.h

# Sensors main-window page. Compiled on all platforms (the .cpp guards its
# BLE-scanner use with GC_WASM_BUILD) so the promoted widget in mainwindow.ui
# always links.
SOURCES += $$PWD/sensorswidget.cpp
HEADERS += $$PWD/sensorswidget.h

# Studio main-window page — native replacement for the dead server-hosted
# QWebEngineView studio page.
SOURCES += $$PWD/studiowidget.cpp
HEADERS += $$PWD/studiowidget.h

FORMS    += $$PWD/mainwindow.ui \
    $$PWD/workoutdialog.ui \
    $$PWD/main_workoutpage.ui \
    $$PWD/dialogconfig.ui \
    $$PWD/dialoglogin.ui \
    $$PWD/z_stylesheet.ui \
    $$PWD/updatedialog.ui \
    $$PWD/dialogmainwindowconfig.ui \
    $$PWD/tab_intervals_icu.ui


