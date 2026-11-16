# glide_add_dll(<target> OBJECTS <object library> OUTPUT_NAME <glide2x|glide3x>)
#
# Creates the Glide shared library <target> from the objects of
# <object library>.
#
# On Windows the DLL is named <OUTPUT_NAME>.dll, the name games load.
#
# With MinGW, the exports are additionally listed in a generated .def file:
# the original Glide DLLs are built with MSVC, which exports the __stdcall
# API functions as e.g. "_grGlideInit@0", and programs linked against the
# 3dfx import libraries import exactly these names. GNU ld only exports
# "grGlideInit@0", so the .def file adds the MSVC names as aliases.

set(GLIDE_DLL_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR})

function(glide_add_dll target)
    cmake_parse_arguments(ARG "" "OBJECTS;OUTPUT_NAME" "" ${ARGN})

    set_target_properties(${ARG_OBJECTS} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(sources $<TARGET_OBJECTS:${ARG_OBJECTS}>)

    if(MINGW)
        if(NOT CMAKE_OBJCOPY)
            message(FATAL_ERROR "objcopy is required to generate the exports of ${target}")
        endif()

        set(def_file ${CMAKE_CURRENT_BINARY_DIR}/${ARG_OUTPUT_NAME}.def)
        add_custom_command(
            OUTPUT ${def_file}
            COMMAND ${CMAKE_COMMAND} -DOBJCOPY=${CMAKE_OBJCOPY}
                    "-DOBJECTS=$<TARGET_OBJECTS:${ARG_OBJECTS}>"
                    -DDEF_FILE=${def_file} -DLIBRARY=${ARG_OUTPUT_NAME}.dll
                    -P ${GLIDE_DLL_MODULE_DIR}/GlideDefFile.cmake
            DEPENDS ${ARG_OBJECTS} $<TARGET_OBJECTS:${ARG_OBJECTS}>
                    ${GLIDE_DLL_MODULE_DIR}/GlideDefFile.cmake
            COMMENT "Generating ${ARG_OUTPUT_NAME}.def"
            VERBATIM)
        list(APPEND sources ${def_file})
    endif()

    add_library(${target} SHARED ${sources})

    if(WIN32)
        set_target_properties(${target} PROPERTIES
            PREFIX ""
            OUTPUT_NAME ${ARG_OUTPUT_NAME})
    endif()
endfunction()
