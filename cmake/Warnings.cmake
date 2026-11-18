# Project-wide compile settings shared by the app and the test suites.
add_compile_definitions(
    QT_DISABLE_DEPRECATED_UP_TO=0x060B00
    QT_NO_FOREACH
    "$<$<CONFIG:Debug>:GC_DEBUG>")

# Matches qmake's warn_on so the code base stays at zero warnings; CI turns
# them into errors with -DMT_WERROR=ON.
function(mt_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W3 $<$<BOOL:${MT_WERROR}>:/WX>)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra $<$<BOOL:${MT_WERROR}>:-Werror>)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_compile_options(${target} PRIVATE -Wno-unused-variable -Wno-sign-compare)
        endif()
    endif()
endfunction()

if(MT_SANITIZE)
    list(JOIN MT_SANITIZE "," _mt_sanitizers)
    add_compile_options(-fsanitize=${_mt_sanitizers} -fno-omit-frame-pointer)
    if("undefined" IN_LIST MT_SANITIZE)
        add_compile_options(-fno-sanitize-recover=undefined)
    endif()
    add_link_options(-fsanitize=${_mt_sanitizers})
endif()

