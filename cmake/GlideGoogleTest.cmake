# Makes GoogleTest available (targets GTest::gtest, GTest::gtest_main) and
# provides glide_add_gtest().
#
# GoogleTest is built from source with the same compiler and flags as Glide,
# so it also works for the 32-bit builds, where a system wide GoogleTest is
# 64-bit only.
#
#   glide_add_gtest(<target> SOURCES <files...> LIBRARIES <libraries...>)
#
# Creates the test program <target>, linked with GoogleTest's main(), and
# registers its tests with CTest. If one of the LIBRARIES is a Glide DLL on
# Windows, it is copied next to the program so that it can be started.

if(CMAKE_VERSION VERSION_LESS 3.14)
    message(FATAL_ERROR "The tests need CMake 3.14 or newer (FetchContent_MakeAvailable)")
endif()

include(FetchContent)
if(POLICY CMP0135)
    # Extracted files get the extraction time, so changing the URL rebuilds.
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    URL_HASH SHA256=8ad598c73ad796e0d8280b082cebd82a630d73e73cd3c70057938a6501bba5d7
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
# Glide uses the DLL C runtime of MSVC (/MD), GoogleTest has to match.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

function(glide_add_gtest target)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBRARIES" ${ARGN})

    add_executable(${target} ${ARG_SOURCES})
    target_compile_features(${target} PRIVATE cxx_std_14)
    target_link_libraries(${target} PRIVATE ${ARG_LIBRARIES} GTest::gtest_main)

    if(WIN32)
        foreach(library IN LISTS ARG_LIBRARIES)
            if(TARGET ${library})
                get_target_property(type ${library} TYPE)
                if(type STREQUAL "SHARED_LIBRARY")
                    add_custom_command(TARGET ${target} POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                $<TARGET_FILE:${library}> $<TARGET_FILE_DIR:${target}>
                        VERBATIM)
                endif()
            endif()
        endforeach()
    endif()

    # The tests check the swapped images, the simulator window (CSIM_WINDOW,
    # swlibs/csim) would only slow them down.
    # Listing the tests at test time, not after the build, also allows to
    # build the tests when cross compiling.
    if(CMAKE_VERSION VERSION_LESS 3.18)
        gtest_discover_tests(${target} PROPERTIES TIMEOUT 60 ENVIRONMENT CSIM_WINDOW=0)
    else()
        gtest_discover_tests(${target} DISCOVERY_MODE PRE_TEST PROPERTIES TIMEOUT 60 ENVIRONMENT CSIM_WINDOW=0)
    endif()
endfunction()
