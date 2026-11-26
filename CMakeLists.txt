CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

PROJECT(Glide3x VERSION 1.0 DESCRIPTION "Glide3x API for Voodoo3/4/5" LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(GlideArch)

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

if(GLIDE_BUILD_TESTS OR GLIDE_BUILD_3DFX_EXAMPLES)
    enable_testing()
endif()


set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Set the configuration for the assembler
if (${BUILD_32BIT})
    if(MSVC)
        # Nothing to do - MSVC supports building 32bit using a different cmake invokation
    else()
        # Set this flag for the compiler and linker
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -m32")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
    endif()
        
    # tell the assembler to generate 32 bit files
    if (WIN32)
        set(CMAKE_ASM_NASM_OBJECT_FORMAT win32)
    else()
        set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
    endif()
else()
    if (WIN32)
        set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
    else()
        set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
    endif()
endif()

if (WIN32)
    set(CMAKE_ASM_NASM_DEFINITIONS ${CMAKE_ASM_NASM_DEFINITIONS}
        -D_NASM_BUILD_FOR_WIN32
    )
else()
    set(CMAKE_ASM_NASM_DEFINITIONS ${CMAKE_ASM_NASM_DEFINITIONS}
        -D_NASM_BUILD_FOR_LINUX
    )
endif()


set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> \
    <FLAGS> -DGLIDE_PACKED_RGB ${CMAKE_ASM_NASM_DEFINITIONS} -f ${CMAKE_ASM_NASM_OBJECT_FORMAT} -o <OBJECT> <SOURCE>")

# Only the 32-bit Windows build has assembly sources (win9x.asm), elsewhere
# NASM is not needed, e.g. on ARM.
if(WIN32 AND BUILD_32BIT)
    enable_language(ASM_NASM)
endif()

    
set (FXMISC_DIR ${CMAKE_SOURCE_DIR}/swlibs/fxmisc)
set (TEXUS2_DIR ${CMAKE_SOURCE_DIR}/swlibs/texus2/lib)

add_subdirectory(common)
add_subdirectory(swlibs)
add_subdirectory(glide3x)

if(BUILD_HAL_CSIM OR GLIDE_BUILD_3DFX_EXAMPLES)
    add_subdirectory(examples)
endif()

if(GLIDE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
