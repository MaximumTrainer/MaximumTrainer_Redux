# CI exports GIT_TAG (actions/checkout leaves git describe unreliable in later
# steps); local builds fall back to git describe. Either way the result is a
# clean "vX.Y.Z", or v0.0.0 when no release tag is reachable.
function(mt_detect_app_version)
    set(version "$ENV{GIT_TAG}")
    if(NOT version)
        execute_process(
            COMMAND git describe --tags --match "v[0-9]*" --always
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE version
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET)
        string(REGEX REPLACE "-[0-9]+-g[0-9a-f]+$" "" version "${version}")
    endif()
    if(NOT version MATCHES "^[vV][0-9]")
        set(version "v0.0.0")
    endif()
    string(REGEX MATCH "^[vV]([0-9]+\\.[0-9]+(\\.[0-9]+)?)" numeric "${version}")
    set(numeric "${CMAKE_MATCH_1}")
    if(NOT numeric)
        set(numeric "0.0.0")
    endif()
    set(MT_GIT_VERSION "${version}" PARENT_SCOPE)
    set(MT_NUMERIC_VERSION "${numeric}" PARENT_SCOPE)
endfunction()

# A generated header rather than a global define: a per-release define changes
# every compile command, so each new tag would rebuild (and ccache-miss) the
# whole app instead of the one file that reads it. file(CONFIGURE) only
# rewrites the header when the version actually changes.
function(mt_write_app_version_header output_dir)
    file(CONFIGURE OUTPUT "${output_dir}/app_version.h"
        CONTENT "#pragma once\n#define APP_VERSION \"@MT_GIT_VERSION@\"\n"
        @ONLY)
endfunction()
