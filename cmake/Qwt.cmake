# QWT 6.3 has no CMake package and no Qt6 distro package, so it is built from
# source and located through QWT_INSTALL (see README). Exposes Qwt::Qwt.
if(TARGET Qwt::Qwt)
    return()
endif()

find_path(QWT_INCLUDE_DIR qwt_plot.h
    HINTS "${QWT_INSTALL}/include" "${QWT_INSTALL}/include/qwt"
    PATH_SUFFIXES qwt qt6/qwt qwt-qt6
    NO_CMAKE_FIND_ROOT_PATH)
if(WIN32)
    set(_qwt_names qwt)
    set(_qwt_debug_names qwtd)
else()
    set(_qwt_names qwt qwt-qt6)
endif()
find_library(QWT_LIBRARY NAMES ${_qwt_names}
    HINTS "${QWT_INSTALL}/lib"
    NO_CMAKE_FIND_ROOT_PATH)
if(_qwt_debug_names)
    find_library(QWT_LIBRARY_DEBUG NAMES ${_qwt_debug_names}
        HINTS "${QWT_INSTALL}/lib"
        NO_CMAKE_FIND_ROOT_PATH)
endif()

if(NOT QWT_INCLUDE_DIR OR NOT QWT_LIBRARY)
    if(MT_BUILD_APP)
        message(FATAL_ERROR "QWT 6.3 not found. Build it from source and pass -DQWT_INSTALL=/path/to/qwt (see README).")
    endif()
    message(STATUS "QWT not found: skipping the plot-based test suites")
    return()
endif()

if(WIN32)
    add_library(Qwt::Qwt SHARED IMPORTED)
else()
    add_library(Qwt::Qwt UNKNOWN IMPORTED)
endif()
set_target_properties(Qwt::Qwt PROPERTIES
    IMPORTED_LOCATION "${QWT_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${QWT_INCLUDE_DIR}")
if(QWT_LIBRARY_DEBUG)
    set_target_properties(Qwt::Qwt PROPERTIES IMPORTED_LOCATION_DEBUG "${QWT_LIBRARY_DEBUG}")
endif()
if(WIN32)
    # On Windows the import library and qwt.dll both sit in lib/.
    set_target_properties(Qwt::Qwt PROPERTIES IMPORTED_IMPLIB "${QWT_LIBRARY}")
    get_filename_component(_qwt_lib_dir "${QWT_LIBRARY}" DIRECTORY)
    set_target_properties(Qwt::Qwt PROPERTIES IMPORTED_LOCATION "${_qwt_lib_dir}/qwt.dll")
    if(QWT_LIBRARY_DEBUG)
        set_target_properties(Qwt::Qwt PROPERTIES
            IMPORTED_IMPLIB_DEBUG "${QWT_LIBRARY_DEBUG}"
            IMPORTED_LOCATION_DEBUG "${_qwt_lib_dir}/qwtd.dll")
    endif()
endif()
if(NOT EMSCRIPTEN)
    set_property(TARGET Qwt::Qwt APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS QWT_DLL)
endif()
target_link_libraries(Qwt::Qwt INTERFACE Qt6::Widgets Qt6::PrintSupport)
