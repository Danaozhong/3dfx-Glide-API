# The test programs of the 3dfx Glide3 SDK for Voodoo4/5 (H5), each a small
# Glide application showing one feature. They share tlib, which parses the
# command line (-n <frames> -r <res> -d <dump file> ...), handles the
# keyboard and the console text, and loads the textures.
#
# They run until a key is pressed, or for -n <frames> without waiting for
# keys. With BUILD_HAL_CSIM, ctest runs them on the chip simulator for 3
# frames (label 3dfx) and the simulator writes the swapped images to
# images/<program>_<n>.ppm in this build directory. The textures (*.3df,
# *.txs) are copied next to the programs, they load them from the working
# directory.

set(GLIDE_3DFX_PROGRAMS
    test00 test01 test02 test03 test04 test04a test05 test06 test07 test08
    test09 test10 test11 test12 test13 test14 test15 test16 test17 test18
    test19 test20 test21 test22 test23 test24 test25 test26 test27 test28
    test29 test30 test31 test32 test33 test34 test35 test36 test37 test38
    test39 test40 test41 test42 test43 test44 test45 test46 test47 test48
    test49 sbench display
)

add_library(glide_tlib STATIC tlib.c)
target_include_directories(glide_tlib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(glide_tlib PUBLIC glide3x_voodoo345)
if(WIN32)
    # The Glide DLL only exports the Glide API, not fxos.c (timer() for
    # sbench), which the shared library on Linux has as well.
    target_sources(glide_tlib PRIVATE ${FXMISC_DIR}/fxos.c)
    target_link_libraries(glide_tlib PUBLIC user32 gdi32)
else()
    target_sources(glide_tlib PRIVATE ${FXMISC_DIR}/linutil.c)
    target_link_libraries(glide_tlib PUBLIC m)
endif()

file(GLOB GLIDE_3DFX_TEXTURES ${CMAKE_CURRENT_SOURCE_DIR}/*.3df ${CMAKE_CURRENT_SOURCE_DIR}/*.txs)

# The textures and, on Windows, the Glide DLL have to be next to the
# programs. They are copied once for all of them: copies after every
# program would run in parallel, and fail on the same files.
get_property(GLIDE_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(GLIDE_MULTI_CONFIG)
    set(GLIDE_3DFX_DIR ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>)
else()
    set(GLIDE_3DFX_DIR ${CMAKE_CURRENT_BINARY_DIR})
endif()
# The directory of a multi-config generator does not exist before the programs
# are linked into it, but this target runs first.
set(GLIDE_3DFX_COPY
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GLIDE_3DFX_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GLIDE_3DFX_TEXTURES} ${GLIDE_3DFX_DIR})
if(WIN32)
    list(APPEND GLIDE_3DFX_COPY
         COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:glide3x_voodoo345> ${GLIDE_3DFX_DIR})
endif()
add_custom_target(glide_3dfx_data ${GLIDE_3DFX_COPY} VERBATIM)
add_dependencies(glide_3dfx_data glide3x_voodoo345)
set_target_properties(glide_3dfx_data PROPERTIES FOLDER examples/3dfx)

foreach(program ${GLIDE_3DFX_PROGRAMS})
    add_executable(${program} ${program}.c)
    target_link_libraries(${program} PRIVATE glide_tlib)
    set_target_properties(${program} PROPERTIES FOLDER examples/3dfx)
    add_dependencies(${program} glide_3dfx_data)
endforeach()

# display only shows image files given on the command line, so there is
# nothing to run without them.
if(BUILD_HAL_CSIM)
    set(GLIDE_3DFX_IMAGE_DIR ${CMAKE_CURRENT_BINARY_DIR}/images)
    file(MAKE_DIRECTORY ${GLIDE_3DFX_IMAGE_DIR})

    foreach(program ${GLIDE_3DFX_PROGRAMS})
        if(program STREQUAL "display")
            continue()
        endif()
        add_test(NAME 3dfx_${program}
                 COMMAND ${program} -n 3
                 WORKING_DIRECTORY $<TARGET_FILE_DIR:${program}>)
        set_tests_properties(3dfx_${program} PROPERTIES
            LABELS 3dfx
            TIMEOUT 120
            ENVIRONMENT "CSIM_DUMP=${GLIDE_3DFX_IMAGE_DIR}/${program}_;CSIM_WINDOW=0")
    endforeach()
endif()
