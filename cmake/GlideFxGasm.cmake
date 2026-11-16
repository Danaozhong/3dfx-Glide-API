# Generates fxgasm.h and fxinline.h for a Glide DLL.
#
#   glide_add_fxgasm_headers(<name>
#       SOURCE <fxgasm.c>
#       CONFIG <interface library holding the DLL's defines and include dirs>
#       OUTPUT_DIR <directory for the generated headers>
#       GLIDE_SOURCE_DIR <directory of the Glide sources including them>)
#
# Creates the custom target <name>, the DLL target has to depend on it, and
# OUTPUT_DIR has to be part of the include directories in CONFIG.
#
# The headers contain struct offsets, which must match the ABI of the DLL.
# fxgasm.c is therefore built with the DLL's compiler and configuration:
#  - MSVC: built as a program and run.
#  - GCC/Clang: only compiled, fxgasm_emit.h writes the output into object
#    sections which are extracted with objcopy. This also works when the
#    target can not be executed, e.g. when cross compiling with MinGW.

set(GLIDE_FXGASM_MODULE_DIR ${CMAKE_CURRENT_LIST_DIR})
set(GLIDE_FXGASM_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../tools/fxgasm)

function(glide_add_fxgasm_headers name)
    cmake_parse_arguments(ARG "" "SOURCE;CONFIG;OUTPUT_DIR;GLIDE_SOURCE_DIR" "" ${ARGN})

    # Glide includes these headers with quotes, so copies next to the Glide
    # sources would take precedence over the generated ones.
    foreach(header fxgasm.h fxinline.h)
        if(EXISTS ${ARG_GLIDE_SOURCE_DIR}/${header})
            message(FATAL_ERROR
                "${ARG_GLIDE_SOURCE_DIR}/${header} is left over from the old fxgasm_tool "
                "build. It is now generated in the build directory, please delete it.")
        endif()
    endforeach()

    set(fxgasm_h ${ARG_OUTPUT_DIR}/fxgasm.h)
    set(fxinline_h ${ARG_OUTPUT_DIR}/fxinline.h)
    file(MAKE_DIRECTORY ${ARG_OUTPUT_DIR})

    if(MSVC)
        if(CMAKE_CROSSCOMPILING)
            message(FATAL_ERROR "fxgasm can not be run when cross compiling with MSVC")
        endif()

        add_executable(${name}_tool ${ARG_SOURCE})
        target_link_libraries(${name}_tool PRIVATE ${ARG_CONFIG})
        target_include_directories(${name}_tool PRIVATE ${GLIDE_FXGASM_INCLUDE_DIR})

        add_custom_command(
            OUTPUT ${fxgasm_h} ${fxinline_h}
            COMMAND ${CMAKE_COMMAND} -DTOOL=$<TARGET_FILE:${name}_tool> -DARGUMENT=-hex
                    -DOUTPUT=${fxgasm_h} -P ${GLIDE_FXGASM_MODULE_DIR}/GlideFxGasmRun.cmake
            COMMAND ${CMAKE_COMMAND} -DTOOL=$<TARGET_FILE:${name}_tool> -DARGUMENT=-inline
                    -DOUTPUT=${fxinline_h} -P ${GLIDE_FXGASM_MODULE_DIR}/GlideFxGasmRun.cmake
            DEPENDS ${name}_tool ${GLIDE_FXGASM_MODULE_DIR}/GlideFxGasmRun.cmake
            COMMENT "Generating fxgasm.h and fxinline.h"
            VERBATIM)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        if(NOT CMAKE_OBJCOPY)
            message(FATAL_ERROR "objcopy is required to generate fxgasm.h and fxinline.h")
        endif()

        add_library(${name}_object OBJECT ${ARG_SOURCE})
        target_link_libraries(${name}_object PRIVATE ${ARG_CONFIG})
        target_include_directories(${name}_object PRIVATE ${GLIDE_FXGASM_INCLUDE_DIR})
        target_compile_definitions(${name}_object PRIVATE FXGASM_EMIT_SECTIONS)
        # The emitted sections have to stay in the object file, in order.
        target_compile_options(${name}_object PRIVATE
            -O0 -g0 -fno-lto -fno-function-sections
            -fno-asynchronous-unwind-tables -fno-unwind-tables)

        add_custom_command(
            OUTPUT ${fxgasm_h} ${fxinline_h}
            COMMAND ${CMAKE_COMMAND} -DOBJCOPY=${CMAKE_OBJCOPY}
                    -DOBJECT=$<TARGET_OBJECTS:${name}_object>
                    -DFXGASM_H=${fxgasm_h} -DFXINLINE_H=${fxinline_h}
                    -P ${GLIDE_FXGASM_MODULE_DIR}/GlideFxGasmExtract.cmake
            DEPENDS ${name}_object $<TARGET_OBJECTS:${name}_object>
                    ${GLIDE_FXGASM_MODULE_DIR}/GlideFxGasmExtract.cmake
            COMMENT "Generating fxgasm.h and fxinline.h"
            VERBATIM)
    else()
        message(FATAL_ERROR "Generating fxgasm.h is not supported for ${CMAKE_C_COMPILER_ID}")
    endif()

    add_custom_target(${name} DEPENDS ${fxgasm_h} ${fxinline_h})
endfunction()
