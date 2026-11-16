# glide_check_win9x_compatibility(<target>)
#
# For 32-bit Windows builds, checks after linking that Windows 9x is able to
# load <target>. The required compiler and linker flags are set in
# mingw-w64-x86_32.cmake.

option(GLIDE_CHECK_WIN9X "Check that 32-bit Windows binaries can be loaded by Windows 9x" ON)

set(GLIDE_WIN9X_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR})

function(glide_check_win9x_compatibility target)
    if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4 OR NOT GLIDE_CHECK_WIN9X)
        return()
    endif()

    if(MSVC)
        if(MSVC_VERSION GREATER 1400)
            message(WARNING
                "Visual C++ 2008 and newer can not create binaries for Windows 9x. "
                "Use MinGW (mingw-w64-x86_32.cmake) to build ${target} for Windows 98.")
        endif()
        return()
    endif()

    if(NOT CMAKE_OBJDUMP)
        message(WARNING "objdump not found, skipping the Windows 9x check of ${target}")
        return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -DOBJDUMP=${CMAKE_OBJDUMP} -DBINARY=$<TARGET_FILE:${target}>
                -P ${GLIDE_WIN9X_MODULE_DIR}/GlideCheckWin9xPE.cmake
        VERBATIM)
endfunction()
