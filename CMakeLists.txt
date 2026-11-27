CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

PROJECT(Glide VERSION 1.0 DESCRIPTION "3dfx Glide2x and Glide3x APIs" LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(GlideArch)

# The Glide libraries, the names are also the names of their targets:
#   glide2x_voodoo2    glide2x/cvg  glide2x.dll
#   glide2x_voodoo3    glide2x/h3   glide2x.dll for Voodoo Banshee/3
#   glide3x_voodoo2    glide3x/cvg  glide3x.dll
#   glide3x_voodoo345  glide3x/h5   glide3x.dll for Voodoo Banshee/3/4/5,
#                                   also with the chip simulator and for DRI
# The single libraries can also be built from their directories, except
# glide3x_voodoo345, which only is built from here.
set(GLIDE_ALL_LIBRARIES glide2x_voodoo2 glide2x_voodoo3 glide3x_voodoo2 glide3x_voodoo345)
set(GLIDE_DIR_glide2x_voodoo2 glide2x/cvg)
set(GLIDE_DIR_glide2x_voodoo3 glide2x/h3)
set(GLIDE_DIR_glide3x_voodoo2 glide3x/cvg)
set(GLIDE_DIR_glide3x_voodoo345 glide3x/h5)

# DRI (Direct Rendering Infrastructure, here the original DRI1) builds Glide3x
# as the backend of Mesa's tdfx OpenGL driver on Linux. It needs the tdfx DRI
# driver of Mesa 7.11 or older (removed in Mesa 8.0, 2012) and the tdfx kernel
# module (removed in Linux 6.3, 2023), so it only works on old distributions.
# See "Linux DRI Build (Obsolete)" in README.md.
OPTION(USE_DRI "Build for Linux DRI, as backend of Mesa's tdfx driver (Mesa <= 7.11, Linux < 6.3)" OFF)


OPTION(USE_DGA "no idea what DGA mode stands for" OFF)
OPTION(EMBED_TEXUS2 "Embed Texus2 functions into Glide3" ON)
OPTION(BUILD_HAL_CSIM
       "Build Glide3x for Voodoo4/5 against the chip simulator (HAL_CSIM) instead of real hardware. Uses the stub in swlibs/csim."
       OFF)

OPTION(GLIDE_BUILD_TESTS
       "Build the tests in tests/, run them with ctest. Downloads GoogleTest."
       OFF)

OPTION(GLIDE_BUILD_3DFX_EXAMPLES
       "Build the test programs of the 3dfx Glide3 SDK in examples/3dfx. With BUILD_HAL_CSIM, ctest runs them on the chip simulator (label 3dfx)."
       OFF)

# By default all libraries for x86 hardware. The chip simulator and DRI are
# variants of glide3x_voodoo345 only, and the older libraries are x86 only.
if(BUILD_HAL_CSIM OR USE_DRI OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|ARM|aarch64)")
    set(GLIDE_DEFAULT_LIBRARIES glide3x_voodoo345)
else()
    set(GLIDE_DEFAULT_LIBRARIES ${GLIDE_ALL_LIBRARIES})
endif()
set(GLIDE_LIBRARIES "${GLIDE_DEFAULT_LIBRARIES}" CACHE STRING
    "The Glide libraries to build, a ;-list of: ${GLIDE_ALL_LIBRARIES}")

foreach(library ${GLIDE_LIBRARIES})
    if(NOT library IN_LIST GLIDE_ALL_LIBRARIES)
        message(FATAL_ERROR "GLIDE_LIBRARIES: unknown library ${library}, use some of ${GLIDE_ALL_LIBRARIES}")
    endif()
endforeach()
message(STATUS "Glide libraries: ${GLIDE_LIBRARIES}")

if("glide3x_voodoo345" IN_LIST GLIDE_LIBRARIES)
    set(GLIDE_BUILD_VOODOO345 ON)
else()
    set(GLIDE_BUILD_VOODOO345 OFF)
    if(BUILD_HAL_CSIM OR USE_DRI OR GLIDE_BUILD_TESTS OR GLIDE_BUILD_3DFX_EXAMPLES)
        message(FATAL_ERROR
            "BUILD_HAL_CSIM, USE_DRI, GLIDE_BUILD_TESTS and GLIDE_BUILD_3DFX_EXAMPLES "
            "need glide3x_voodoo345 in GLIDE_LIBRARIES")
    endif()
endif()

if(GLIDE_BUILD_TESTS OR GLIDE_BUILD_3DFX_EXAMPLES)
    enable_testing()
endif()


set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set (FXMISC_DIR ${CMAKE_SOURCE_DIR}/swlibs/fxmisc)
set (TEXUS2_DIR ${CMAKE_SOURCE_DIR}/swlibs/texus2/lib)

if(GLIDE_BUILD_VOODOO345)
    add_subdirectory(common)
    add_subdirectory(swlibs)
endif()

foreach(library ${GLIDE_LIBRARIES})
    add_subdirectory(${GLIDE_DIR_${library}})
endforeach()

if(BUILD_HAL_CSIM OR GLIDE_BUILD_3DFX_EXAMPLES)
    add_subdirectory(examples)
endif()

if(GLIDE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
