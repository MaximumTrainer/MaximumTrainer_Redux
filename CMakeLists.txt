cmake_minimum_required(VERSION 3.25)

include(cmake/AppVersion.cmake)
mt_detect_app_version()

project(MaximumTrainer VERSION ${MT_NUMERIC_VERSION} LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

option(MT_WERROR "Treat compiler warnings as errors (CI)" OFF)
option(MT_BUILD_TESTS "Build the Qt Test suites under tests/" ON)
option(MT_BUILD_APP "Build the MaximumTrainer application" ON)
set(MT_SANITIZE "" CACHE STRING "Sanitizers for every target, e.g. address;undefined")
set(QWT_INSTALL "" CACHE PATH "QWT 6.3 install prefix (contains include/ and lib/)")
set(INTERVALS_OAUTH_CLIENT_ID "$ENV{INTERVALS_OAUTH_CLIENT_ID}" CACHE STRING "Intervals.icu OAuth client id")
if(NOT INTERVALS_OAUTH_CLIENT_ID)
    set(INTERVALS_OAUTH_CLIENT_ID "259")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Keep the qmake-era output layout: scripts, CI artifacts and test docs all
# point at build/release/MaximumTrainer and build/tests/<suite>.
set(MT_APP_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/release" CACHE PATH "App binary output directory")
set(MT_TEST_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/build/tests" CACHE PATH "Test binary output directory")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Quick Qml)
if(MT_BUILD_APP)
    find_package(Qt6 REQUIRED COMPONENTS PrintSupport QuickWidgets WebSockets)
endif()
find_package(Qt6 QUIET OPTIONAL_COMPONENTS
    PrintSupport QuickWidgets WebSockets Concurrent Bluetooth WebEngineWidgets WebEngineCore
    Multimedia MultimediaWidgets UiTools Test)
qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

include(cmake/Qwt.cmake)
include(cmake/Warnings.cmake)

add_subdirectory(src)

if(MT_BUILD_TESTS AND TARGET Qt6::Test AND NOT EMSCRIPTEN)
    enable_testing()
    add_subdirectory(tests)
endif()
