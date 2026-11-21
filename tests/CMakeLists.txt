# Qt Test suites. Each suite compiles only the sources it exercises (some swap
# in stubs from tests/intervals_icu), so they stay fast and independent of the
# full app link. Run them all with: ctest --test-dir build -j$(nproc)
set(SRC "${CMAKE_SOURCE_DIR}/src")
set(TST "${CMAKE_CURRENT_SOURCE_DIR}")

if(WIN32)
    set(MT_CREDENTIAL_STORE_SOURCES "${SRC}/app/credential_store_win.cpp")
    set(MT_CREDENTIAL_STORE_LIBS Crypt32)
elseif(APPLE)
    set(MT_CREDENTIAL_STORE_SOURCES "${SRC}/app/credential_store_mac.mm")
    set(MT_CREDENTIAL_STORE_LIBS "-framework Security" "-framework Foundation" objc)
else()
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
    set(MT_CREDENTIAL_STORE_SOURCES "${SRC}/app/credential_store_linux.cpp")
    set(MT_CREDENTIAL_STORE_LIBS OpenSSL::Crypto)
endif()

# mt_add_test(<name> SOURCES ... [QT <modules>] [INCLUDES <dirs>] [LIBS ...]
#             [DEFINES ...] [QWT])
# INCLUDES are searched before anything else, so a suite's local stubs (e.g.
# tests/intervals_icu/util.h) shadow the real headers.
function(mt_add_test name)
    cmake_parse_arguments(T "QWT" "" "SOURCES;QT;INCLUDES;LIBS;DEFINES" ${ARGN})
    qt_add_executable(${name} ${T_SOURCES})
    set_target_properties(${name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${MT_TEST_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${MT_TEST_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${MT_TEST_OUTPUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${MT_TEST_OUTPUT_DIR}"
        MACOSX_BUNDLE OFF
        WIN32_EXECUTABLE OFF
        AUTOUIC_SEARCH_PATHS "${SRC}/ui;${SRC}/ui/components;${SRC}/ui/workout_editor;${SRC}/btle")
    target_include_directories(${name} BEFORE PRIVATE ${T_INCLUDES})
    target_include_directories(${name} PRIVATE "${CMAKE_BINARY_DIR}/src/generated")
    target_compile_definitions(${name} PRIVATE
        INTERVALS_OAUTH_CLIENT_ID="${INTERVALS_OAUTH_CLIENT_ID}" ${T_DEFINES})
    target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Test ${T_LIBS})
    foreach(module IN LISTS T_QT)
        target_link_libraries(${name} PRIVATE Qt6::${module})
    endforeach()
    if(T_QWT)
        target_link_libraries(${name} PRIVATE Qwt::Qwt)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        WORKING_DIRECTORY "${MT_TEST_OUTPUT_DIR}"
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

# Source groups shared by several suites.
set(LOGGER "${SRC}/app/logger.cpp")
set(CRED_STUB "${TST}/intervals_icu/credential_store_stub.cpp")
set(WORKOUT_MODEL
    "${SRC}/model/account.cpp" "${SRC}/model/workout.cpp" "${SRC}/model/interval.cpp"
    "${SRC}/model/repeatdata.cpp")
set(APP_MODEL ${WORKOUT_MODEL}
    "${SRC}/app/util.cpp" "${SRC}/app/simplecrypt.cpp" "${SRC}/model/settings.cpp"
    "${SRC}/model/sensor.cpp" "${SRC}/model/radio.cpp" "${SRC}/model/userstudio.cpp"
    "${SRC}/model/trackpoint.cpp" "${SRC}/fitness/achievements/achievement.cpp")
set(APP_DAO
    "${SRC}/persistence/db/environnement.cpp" "${SRC}/persistence/db/extrequest.cpp"
    "${SRC}/persistence/db/versiondao.cpp" "${SRC}/persistence/db/intervalsicudao.cpp"
    "${SRC}/persistence/file/xmlutil.cpp")
set(APP_INCLUDES
    "${SRC}/app" "${SRC}/btle" "${SRC}/model" "${SRC}/fitness" "${SRC}/fitness/achievements"
    "${SRC}/persistence/db" "${SRC}/persistence/file" "${SRC}/ui" "${SRC}/ui/components"
    "${SRC}/ui/plots" "${SRC}/ui/workout_editor")
set(REPEAT_WIDGET
    "${SRC}/ui/workout_editor/repeatwidget.cpp" "${SRC}/ui/workout_editor/repeatwidget.ui")

# ── Unit suites ──────────────────────────────────────────────────────────────
mt_add_test(logger_tests
    SOURCES ${LOGGER} logger/tst_logger.cpp
    INCLUDES "${SRC}/app")

mt_add_test(apptheme_tests
    SOURCES "${SRC}/ui/apptheme.h" apptheme/tst_apptheme.cpp
    QT Gui Widgets
    INCLUDES "${SRC}/ui")

mt_add_test(config_bootstrap_tests
    SOURCES ${LOGGER} "${SRC}/app/config_bootstrap.cpp" ${MT_CREDENTIAL_STORE_SOURCES}
        config_bootstrap/tst_config_bootstrap.cpp
    LIBS ${MT_CREDENTIAL_STORE_LIBS}
    INCLUDES "${SRC}/app")

mt_add_test(credential_store_tests
    SOURCES ${LOGGER} ${MT_CREDENTIAL_STORE_SOURCES} credential_store/tst_credential_store.cpp
    QT Network
    LIBS ${MT_CREDENTIAL_STORE_LIBS}
    INCLUDES "${TST}/credential_store" "${SRC}/app")

mt_add_test(settings_tests
    SOURCES ${WORKOUT_MODEL} ${LOGGER} ${CRED_STUB} settings/tst_settings.cpp
    INCLUDES "${TST}/intervals_icu" "${SRC}/app" "${SRC}/model" "${SRC}/persistence/db")

mt_add_test(workout_export_tests
    SOURCES ${WORKOUT_MODEL} ${LOGGER} ${CRED_STUB}
        "${SRC}/persistence/file/fitworkoutwriter.cpp"
        workout_export/tst_workout_export.cpp
    LIBS mt_fit
    INCLUDES "${TST}/intervals_icu" "${SRC}/app" "${SRC}/model" "${SRC}/persistence/file"
        "${SRC}/persistence/db")

mt_add_test(achievements_tests
    SOURCES "${SRC}/fitness/achievements/achievementcatalog.cpp"
        "${SRC}/fitness/achievements/achievementlivetracker.cpp"
        "${SRC}/fitness/achievements/achievementstore.cpp"
        achievements/tst_achievements.cpp
    INCLUDES "${SRC}/fitness/achievements" "${SRC}/model")

mt_add_test(workout_logic_tests
    SOURCES "${SRC}/workout/sensorsecondaccumulator.cpp" "${SRC}/workout/maptesttracker.cpp"
        "${SRC}/workout/targetalert.cpp"
        workout/tst_workout_logic.cpp
    INCLUDES "${SRC}/workout")

mt_add_test(webengine_safe_mode_tests
    SOURCES ${LOGGER} "${SRC}/app/webengine_safe_mode.cpp"
        webengine_safe_mode/tst_webengine_safe_mode.cpp
    INCLUDES "${SRC}/app")

mt_add_test(history_tests
    SOURCES "${SRC}/fitness/pmccalculator.cpp"
        "${SRC}/persistence/file/fitactivityreader.cpp"
        "${SRC}/persistence/file/fitactivitydetailreader.cpp"
        "${SRC}/ui/workouthistorymodel.cpp" "${SRC}/ui/historyfilterproxymodel.cpp"
        "${SRC}/fitness/mmpcalculator.cpp" "${SRC}/fitness/ftpestimator.cpp"
        "${SRC}/fitness/weeklysummary.cpp" "${SRC}/fitness/ridesummary.cpp"
        "${SRC}/fitness/powerzones.cpp"
        history/tst_history.cpp
    QT Gui
    LIBS mt_fit
    INCLUDES "${TST}/history" "${SRC}/model" "${SRC}/persistence/file" "${SRC}/fitness" "${SRC}/ui")

mt_add_test(strava_tests
    SOURCES ${LOGGER} "${SRC}/persistence/db/strava_service.cpp" strava/tst_strava_service.cpp
    QT Network
    INCLUDES "${TST}/strava" "${SRC}/persistence/db" "${SRC}/app")

mt_add_test(studio_tests
    SOURCES ${LOGGER} "${SRC}/btle/simulator_hub.cpp" studio/tst_studio_mode.cpp
    INCLUDES "${SRC}/btle" "${SRC}/app")

if(TARGET Qt6::WebSockets)
mt_add_test(group_tests
    SOURCES "${SRC}/group/groupprotocol.cpp" "${SRC}/group/groupsession.cpp"
        "${SRC}/group/groupridecoordinator.cpp" "${SRC}/group/groupridersmodel.cpp"
        group/tst_group.cpp
    QT Network WebSockets
    INCLUDES "${TST}/group" "${SRC}/group")
endif()

mt_add_test(game_tests
    SOURCES game/tst_game.cpp
    QT Gui Quick Qml
    LIBS mt_game mt_gameplugin
    INCLUDES "${TST}/game")

# ── Intervals.icu / ZWO ──────────────────────────────────────────────────────
mt_add_test(intervals_icu_tests
    SOURCES ${LOGGER} "${SRC}/persistence/db/intervals_icu_api.cpp"
        intervals_icu/tst_intervals_icu_service.cpp
    QT Network
    INCLUDES "${TST}/intervals_icu" "${SRC}/persistence/db" "${SRC}/app")

mt_add_test(importer_workout_zwo_tests
    SOURCES "${SRC}/persistence/file/importerworkoutzwo.cpp" ${WORKOUT_MODEL} ${LOGGER}
        ${CRED_STUB} intervals_icu/tst_importer_workout_zwo.cpp
    QT Network
    DEFINES FIXTURES_DIR="${TST}/fixtures"
    INCLUDES "${TST}/intervals_icu" "${SRC}/persistence/db" "${SRC}/persistence/file"
        "${SRC}/model" "${SRC}/app")

mt_add_test(intervals_icu_dao_bearer_tests
    SOURCES ${LOGGER} "${SRC}/persistence/db/intervalsicudao.cpp"
        intervals_icu/tst_intervals_icu_dao_bearer.cpp
    QT Network
    INCLUDES "${TST}/intervals_icu" "${SRC}/persistence/db" "${SRC}/app")

mt_add_test(intervals_icu_oauth_exchange_tests
    SOURCES ${LOGGER} "${SRC}/persistence/db/extrequest.cpp"
        "${SRC}/persistence/db/environnement.cpp" ${CRED_STUB}
        intervals_icu/util_parse_error_stub.cpp intervals_icu/tst_intervals_icu_oauth_exchange.cpp
    QT Network
    INCLUDES "${TST}/intervals_icu" "${SRC}/persistence/db" "${SRC}/app")

mt_add_test(intervals_icu_integration_tests
    SOURCES ${LOGGER} "${SRC}/persistence/db/intervals_icu_api.cpp"
        intervals_icu_integration/tst_intervals_icu_integration.cpp
        intervals_icu_integration/tst_intervals_icu_integration.h
    QT Network
    INCLUDES "${TST}/intervals_icu_integration" "${SRC}/persistence/db" "${SRC}/app")

# ── Bluetooth ────────────────────────────────────────────────────────────────
if(TARGET Qt6::Bluetooth)
    mt_add_test(btle_tests
        SOURCES ${LOGGER} "${SRC}/btle/btle_hub.cpp" "${SRC}/btle/simulator_hub.cpp"
            "${SRC}/btle/sensor_connector.cpp" "${SRC}/btle/btle_sensor_store.cpp"
            btle/tst_btle_hub.cpp btle/btle_device_simulator.h
        QT Bluetooth
        INCLUDES "${SRC}/btle" "${SRC}/btle/zwift" "${SRC}/app")

    mt_add_test(btle_api_tests
        SOURCES ${LOGGER} "${SRC}/btle/btle_hub.cpp" integration/tst_btle_api.cpp
        QT Bluetooth
        INCLUDES "${SRC}/btle" "${SRC}/btle/zwift" "${SRC}/app")

    mt_add_test(btle_integration_tests
        SOURCES "${SRC}/btle/simulator_hub.cpp" integration/tst_btle_integration.cpp
        QT Gui Widgets Bluetooth
        INCLUDES "${SRC}/btle")

    mt_add_test(runtime_validation_tests
        SOURCES "${SRC}/btle/simulator_hub.cpp" integration/tst_runtime_validation.cpp
        QT Gui Widgets Bluetooth
        INCLUDES "${SRC}/btle")
endif()

# ── Integration / UI ─────────────────────────────────────────────────────────
mt_add_test(offline_mode_tests
    SOURCES "${SRC}/btle/simulator_hub.cpp" integration/tst_offline_mode.cpp
    QT Gui Widgets
    INCLUDES "${SRC}/btle")

mt_add_test(online_mode_tests
    SOURCES ${LOGGER} "${SRC}/persistence/db/intervals_icu_api.cpp"
        "${SRC}/btle/simulator_hub.cpp" integration/tst_online_mode.cpp integration/tst_online_mode.h
    QT Gui Widgets Network
    INCLUDES "${TST}/integration" "${SRC}/persistence/db" "${SRC}/app" "${SRC}/btle")

mt_add_test(workout_parsing_tests
    SOURCES "${SRC}/persistence/file/importerworkoutzwo.cpp" ${WORKOUT_MODEL} ${LOGGER}
        ${CRED_STUB} integration/tst_workout_parsing.cpp
    QT Network
    INCLUDES "${TST}/intervals_icu" "${SRC}/persistence/file" "${SRC}/persistence/db"
        "${SRC}/model" "${SRC}/app")

mt_add_test(workout_creator_tests
    SOURCES ${WORKOUT_MODEL} ${LOGGER} ${CRED_STUB} integration/tst_workout_creator.cpp
    INCLUDES "${TST}/intervals_icu" "${SRC}/app" "${SRC}/model" "${SRC}/persistence/db")

if(TARGET Qwt::Qwt)
mt_add_test(login_screen_tests QWT
    SOURCES ${LOGGER} ${APP_MODEL} ${APP_DAO} ${CRED_STUB} ${REPEAT_WIDGET}
        "${SRC}/btle/simulator_hub.cpp"
        "${SRC}/persistence/db/intervals_icu_oauth_flow.cpp"
        "${SRC}/ui/dialoglogin.cpp" "${SRC}/ui/dialoglogin.ui"
        "${SRC}/app/updatenotifier.cpp"
        integration/tst_login_screen.cpp
    QT Gui Widgets Network
    INCLUDES ${APP_INCLUDES})

mt_add_test(ui_navigation_tests QWT
    SOURCES ${LOGGER} ${APP_MODEL} ${APP_DAO} ${CRED_STUB} integration/tst_ui_navigation.cpp
    QT Gui Widgets Network
    INCLUDES ${APP_INCLUDES})

mt_add_test(ui_screens_tests QWT
    SOURCES ${LOGGER} ${APP_MODEL} ${CRED_STUB} ${REPEAT_WIDGET}
        "${SRC}/ui/components/faderlabel.cpp" "${SRC}/ui/components/shapefactory.cpp"
        "${SRC}/ui/components/sensorstatusstrip.cpp"
        "${SRC}/ui/plots/curvedatacadence.cpp" "${SRC}/ui/plots/curvedataheartrate.cpp"
        "${SRC}/ui/plots/curvedatapower.cpp" "${SRC}/ui/plots/curvedataspeed.cpp"
        "${SRC}/ui/plots/workoutplot.cpp" "${SRC}/ui/plots/zoneitem.cpp"
        "${SRC}/fitness/powerzones.cpp"
        "${SRC}/ui/workout_editor/myqwtpickermachine.cpp"
        "${SRC}/ui/workout_editor/myqwtplotpicker.cpp"
        integration/tst_ui_screens.cpp
    QT Gui Widgets Network
    INCLUDES ${APP_INCLUDES})

mt_add_test(workout_io_tests QWT
    SOURCES ${LOGGER} ${APP_MODEL} ${APP_DAO} ${CRED_STUB} ${REPEAT_WIDGET}
        "${SRC}/persistence/file/importerworkoutzwo.cpp" integration/tst_workout_io.cpp
    QT Gui Widgets Network
    INCLUDES ${APP_INCLUDES})
# The bundled plan files sit next to the binary, as in the app.
add_custom_command(TARGET workout_io_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources/included_workout" "${MT_TEST_OUTPUT_DIR}/included_workout")

mt_add_test(workout_ui_tests QWT
    SOURCES ${LOGGER} ${APP_MODEL} ${APP_DAO} ${CRED_STUB} ${REPEAT_WIDGET}
        "${SRC}/btle/simulator_hub.cpp" integration/tst_workout_ui.cpp
    QT Gui Widgets Network
    INCLUDES ${APP_INCLUDES})
endif()
