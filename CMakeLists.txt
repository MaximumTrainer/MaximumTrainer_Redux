cmake_minimum_required(VERSION 3.25)

# Before project(): the MSVC toolchain otherwise initialises it to Debug.
if(NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED ENV{CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()
# Debug info inside each .obj (/Z7): no shared PDB for parallel compiles to
# fight over, and every object is cacheable by sccache.
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)

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

# With "cmake -B build" this keeps the familiar layout: build/release/MaximumTrainer
# and build/tests/<suite>, which scripts, CI artifacts and test docs rely on.
set(MT_APP_OUTPUT_DIR "${CMAKE_BINARY_DIR}/release" CACHE PATH "App binary output directory")
set(MT_TEST_OUTPUT_DIR "${CMAKE_BINARY_DIR}/tests" CACHE PATH "Test binary output directory")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network)
if(MT_BUILD_APP)
    find_package(Qt6 REQUIRED COMPONENTS PrintSupport WebSockets)
endif()
find_package(Qt6 QUIET OPTIONAL_COMPONENTS
    PrintSupport WebSockets Concurrent Bluetooth WebEngineWidgets WebEngineCore
    Multimedia MultimediaWidgets UiTools Test DBus)
qt_standard_project_setup(REQUIRES 6.5)

include(cmake/Qwt.cmake)
include(cmake/Warnings.cmake)

add_subdirectory(src)

if(MT_BUILD_TESTS AND TARGET Qt6::Test AND NOT EMSCRIPTEN)
    enable_testing()
    # Its own binary dir, so build/tests holds only the suite executables.
    add_subdirectory(tests "${CMAKE_BINARY_DIR}/test-suites")
endif()
