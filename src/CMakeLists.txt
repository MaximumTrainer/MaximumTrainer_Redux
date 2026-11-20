# ── Garmin FIT SDK ───────────────────────────────────────────────────────────
add_library(mt_fit OBJECT)
file(GLOB MT_FIT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/fitness/fit/*.cpp")
target_sources(mt_fit PRIVATE ${MT_FIT_SOURCES})
target_include_directories(mt_fit PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/fitness/fit")
set_target_properties(mt_fit PROPERTIES POSITION_INDEPENDENT_CODE ON AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

# ── Retro ghost race: compiled QML module (MaximumTrainer.Game) ──────────────
add_subdirectory(game)

mt_write_app_version_header("${CMAKE_CURRENT_BINARY_DIR}/generated")

if(NOT MT_BUILD_APP)
    return()
endif()

# ── Application code ─────────────────────────────────────────────────────────
# Everything except main.cpp. An object library rather than a static one, so
# self-registering objects are never dropped by the linker.
add_library(mt_core OBJECT)

set(MT_SOURCE_DIRS
    app btle btle/zwift fitness fitness/fit fitness/achievements game group model
    persistence/db persistence/file ui ui/components ui/plots ui/workout_editor workout)
foreach(dir IN LISTS MT_SOURCE_DIRS)
    list(APPEND MT_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}")
endforeach()

target_sources(mt_core PRIVATE
    # app
    app/logger.cpp app/config_bootstrap.cpp app/startup_trace.cpp app/stallwatchdog.cpp app/webengine_safe_mode.cpp
    app/util.cpp app/updatenotifier.cpp app/globalvars.cpp app/simplecrypt.cpp app/networkmonitor.cpp
    app/qtmediaplayer.h
    # fitness
    fitness/mmpcalculator.cpp fitness/pmccalculator.cpp fitness/ftpestimator.cpp fitness/weeklysummary.cpp fitness/ridesummary.cpp fitness/powerzones.cpp
    fitness/achievements/achievement.cpp fitness/achievements/managerachievement.cpp
    fitness/achievements/achievementchecker.cpp fitness/achievements/achievementcatalog.cpp
    fitness/achievements/achievementstore.cpp
    # model
    model/interval.cpp model/workout.cpp model/sortfilterproxymodel.cpp model/workouttablemodel.cpp
    model/sensor.cpp model/settings.cpp model/dataworkout.cpp model/intervaldata.cpp
    model/intervaltablemodel.cpp model/repeatdata.cpp model/account.cpp model/radiotablemodel.cpp
    model/radio.cpp model/trackpoint.cpp model/userstudio.cpp
    # persistence
    persistence/db/extrequest.cpp persistence/db/versiondao.cpp persistence/db/environnement.cpp
    persistence/db/intervalsicudao.cpp persistence/db/intervalsicuftpsync.cpp persistence/db/intervalsicuservice.cpp
    persistence/db/intervals_icu_api.cpp persistence/db/intervals_icu_oauth_flow.cpp
    persistence/db/strava_service.cpp persistence/db/strava_oauth_flow.cpp
    persistence/file/xmlutil.cpp persistence/file/fitactivitycreator.cpp persistence/file/fitworkoutwriter.cpp
    persistence/file/fitactivityreader.cpp persistence/file/fitactivitydetailreader.cpp
    persistence/file/importerworkout.cpp persistence/file/importerworkoutzwo.cpp
    # workout
    workout/workoututil.cpp workout/sensorsecondaccumulator.cpp workout/maptesttracker.cpp workout/targetalert.cpp
    # group workouts
    group/groupprotocol.cpp group/groupsession.cpp group/groupridecoordinator.cpp
    group/groupsimtest.cpp group/groupworkoutcodec.cpp group/grouppanel.cpp
    group/joingroupdialog.cpp group/groupridersmodel.cpp group/groupriderdelegate.cpp
    # ui
    ui/mainwindow.cpp ui/mainwindow_harness.cpp ui/workoutdialog.cpp ui/main_workoutpage.cpp
    ui/dialogconfig.cpp ui/dialoglogin.cpp ui/z_stylesheet.cpp
    ui/splashscreen.cpp ui/dialogmainwindowconfig.cpp
    ui/tab_intervals_icu.cpp ui/workouthistorymodel.cpp ui/historywidget.cpp
    ui/historyfilterproxymodel.cpp ui/activitydetaildialog.cpp ui/dialogkeyboardshortcuts.cpp
    ui/criticalpowerdialog.cpp ui/pmcdialog.cpp ui/achievementswidget.cpp ui/weeklysummarywidget.cpp
    ui/sensorswidget.cpp ui/studiowidget.cpp
    ui/apptheme.h ui/asyncdialogs.h
    ui/mainwindow.ui ui/workoutdialog.ui ui/main_workoutpage.ui ui/dialogconfig.ui
    ui/dialoglogin.ui ui/z_stylesheet.ui ui/dialogmainwindowconfig.ui
    ui/tab_intervals_icu.ui
    # ui/components
    ui/components/delegaterowhover.cpp
    ui/components/mytableview.cpp ui/components/shapefactory.cpp ui/components/infowidget.cpp
    ui/components/bottombar.cpp ui/components/topmenuworkout.cpp ui/components/balancewidget.cpp
    ui/components/postworkoutpanel.cpp ui/components/workoutraceview.cpp ui/components/achievementtoast.cpp ui/components/metriccard.cpp ui/components/trendchart.cpp ui/components/sensorstatusstrip.cpp
    ui/components/clickablelabel.cpp ui/components/fancytab.cpp ui/components/fancytabbar.cpp
    ui/components/stylehelper.cpp ui/components/faderlabel.cpp
    ui/components/tableviewhover.cpp ui/components/clock.cpp
    ui/components/infosworkout.cpp ui/components/oxygenwidget.cpp
    ui/components/userstudiowidget.cpp ui/components/toggleswitch.cpp
    ui/components/webbrowserview.cpp ui/components/myqwebenginepage.h
    ui/components/infowidget.ui ui/components/bottombar.ui ui/components/topmenuworkout.ui
    ui/components/balancewidget.ui ui/components/infosworkout.ui
    ui/components/oxygenwidget.ui ui/components/userstudiowidget.ui
    # ui/plots
    ui/plots/workoutplot.cpp ui/plots/workoutplotlist.cpp ui/plots/markeritem.cpp
    ui/plots/iconitem.cpp ui/plots/zoneitem.cpp ui/plots/workoutplotzoomer.cpp
    ui/plots/curvedatacadence.cpp ui/plots/curvedatapower.cpp ui/plots/curvedataheartrate.cpp
    ui/plots/curvedataspeed.cpp
    # ui/workout_editor
    ui/workout_editor/repeatwidget.cpp ui/workout_editor/mycreatorplot.cpp
    ui/workout_editor/workoutcreator.cpp ui/workout_editor/intervaldelegate.cpp
    ui/workout_editor/powereditor.cpp ui/workout_editor/cadenceeditor.cpp
    ui/workout_editor/hreditor.cpp ui/workout_editor/tableviewinterval.cpp
    ui/workout_editor/intervalviewstyle.cpp ui/workout_editor/myqwtpickermachine.cpp
    ui/workout_editor/myqwtplotpicker.cpp ui/workout_editor/repeatincreaseeditor.cpp
    ui/workout_editor/repeatwidget.ui ui/workout_editor/workoutcreator.ui
    ui/workout_editor/powereditor.ui ui/workout_editor/cadenceeditor.ui
    ui/workout_editor/hreditor.ui ui/workout_editor/repeatincreaseeditor.ui
    # btle (shared)
    btle/simulator_hub.cpp btle/btle_scanner_dialog.ui btle/zwift/virtual_gear.h
    # resources
    ${CMAKE_SOURCE_DIR}/MyResources.qrc)

if(EMSCRIPTEN)
    target_sources(mt_core PRIVATE
        app/soundplayer.h app/soundplayer_wasm.cpp app/credential_store_wasm.cpp
        btle/btle_hub_wasm.cpp btle/webbluetooth_bridge.cpp btle/btle_scanner_dialog_wasm.cpp)
else()
    target_sources(mt_core PRIVATE
        app/soundplayer.cpp
        btle/btle_hub.cpp btle/btle_scanner_dialog.cpp btle/btle_sensor_store.cpp
        btle/sensor_connect_dialog.cpp btle/sensor_connector.cpp
        btle/zwift/zwift_click_protocol.cpp btle/zwift/zwift_click_hub.cpp
        btle/zwift/zwift_click_manager.cpp btle/zwift/zwift_click_test.cpp)
    if(WIN32)
        target_sources(mt_core PRIVATE app/credential_store_win.cpp)
    elseif(APPLE)
        target_sources(mt_core PRIVATE app/credential_store_mac.mm app/MacUtils.mm)
    else()
        target_sources(mt_core PRIVATE app/credential_store_linux.cpp)
    endif()
endif()

target_include_directories(mt_core PUBLIC ${MT_INCLUDE_DIRS} "${CMAKE_CURRENT_BINARY_DIR}/generated")
set_target_properties(mt_core PROPERTIES AUTORCC ON AUTOUIC_SEARCH_PATHS "${MT_INCLUDE_DIRS}")
target_compile_definitions(mt_core PUBLIC
    INTERVALS_OAUTH_CLIENT_ID="${INTERVALS_OAUTH_CLIENT_ID}")
mt_set_warnings(mt_core)

target_link_libraries(mt_core PUBLIC
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::PrintSupport
    Qt6::Quick Qt6::QuickWidgets Qt6::Qml Qt6::WebSockets
    Qwt::Qwt mt_fit mt_game)
foreach(module Concurrent Bluetooth WebEngineWidgets WebEngineCore)
    if(TARGET Qt6::${module})
        target_link_libraries(mt_core PUBLIC Qt6::${module})
    endif()
endforeach()
if(TARGET Qt6::UiTools)
    target_link_libraries(mt_core PUBLIC Qt6::UiTools)
else()
    target_compile_definitions(mt_core PUBLIC QT_NO_UITOOLS)
endif()

if(EMSCRIPTEN)
    # Stubs make #include <QWebEngineView> etc. resolve to no-op classes.
    target_include_directories(mt_core BEFORE PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/wasm_stubs"
        "${CMAKE_CURRENT_SOURCE_DIR}/ui/wasm_stubs/QtWebEngineWidgets")
    target_compile_definitions(mt_core PUBLIC GC_WASM_BUILD)
    target_link_options(mt_core PUBLIC -lembind)
elseif(TARGET Qt6::Multimedia AND TARGET Qt6::MultimediaWidgets)
    target_sources(mt_core PRIVATE app/qtmediaplayer.cpp)
    target_compile_definitions(mt_core PUBLIC GC_HAVE_QTMULTIMEDIA)
    target_link_libraries(mt_core PUBLIC Qt6::Multimedia Qt6::MultimediaWidgets)
endif()

if(WIN32)
    target_link_libraries(mt_core PUBLIC Gdi32 User32 Crypt32)
elseif(APPLE)
    target_link_libraries(mt_core PUBLIC "-framework IOKit" "-framework AppKit" "-framework Security" objc)
elseif(NOT EMSCRIPTEN)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    target_link_libraries(mt_core PUBLIC OpenSSL::Crypto m)
endif()

# ── Executable ───────────────────────────────────────────────────────────────
qt_add_executable(MaximumTrainer WIN32 MACOSX_BUNDLE app/main.cpp)
target_link_libraries(MaximumTrainer PRIVATE mt_core mt_gameplugin)
mt_set_warnings(MaximumTrainer)
set_target_properties(MaximumTrainer PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${MT_APP_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${MT_APP_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${MT_APP_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/debug")

if(WIN32)
    configure_file("${CMAKE_SOURCE_DIR}/cmake/MaximumTrainer.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/MaximumTrainer.rc" @ONLY)
    target_sources(MaximumTrainer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/MaximumTrainer.rc")
    if(MSVC)
        # Release builds keep a PDB for crash reports (#382); objects carry
        # /Z7 debug info (CMAKE_MSVC_DEBUG_INFORMATION_FORMAT in the root).
        # /DEBUG turns off the linker's dead-code and identical-code folding
        # unless they are requested explicitly.
        target_link_options(MaximumTrainer PRIVATE /DEBUG /OPT:REF /OPT:ICF)
    endif()
elseif(APPLE)
    set(MT_ICON "${CMAKE_SOURCE_DIR}/resources/myappico.icns")
    target_sources(MaximumTrainer PRIVATE "${MT_ICON}")
    set_source_files_properties("${MT_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(MaximumTrainer PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/mac/Info.plist"
        XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/mac/MaximumTrainer.entitlements")
endif()
