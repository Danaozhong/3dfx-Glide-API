CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

PROJECT(Glide3x VERSION 1.0 DESCRIPTION "Glide3x API for Voodoo3/4/5" LANGUAGES C ASM_NASM)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(GlideArch)

OPTION(USE_DRI "no idea what DRI mode stands for" OFF)
OPTION(USE_DGA "no idea what DRI mode stands for" OFF)
OPTION(EMBED_TEXUS2 "Embed Texus2 functions into Glide3" ON)
OPTION(BUILD_HAL_CSIM
       "Build Glide3x for Voodoo4/5 against the chip simulator (HAL_CSIM) instead of real hardware. Uses the stub in swlibs/csim."
       OFF)

OPTION(GLIDE_BUILD_TESTS
       "Build the tests in tests/, run them with ctest. Downloads GoogleTest."
       OFF)

if(GLIDE_BUILD_TESTS)
    # GoogleTest is C++.
    enable_language(CXX)
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

    
set (FXMISC_DIR ${CMAKE_SOURCE_DIR}/swlibs/fxmisc)
set (TEXUS2_DIR ${CMAKE_SOURCE_DIR}/swlibs/texus2/lib)

add_subdirectory(common)
add_subdirectory(swlibs)
add_subdirectory(glide3x)

if(GLIDE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
