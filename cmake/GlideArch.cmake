# The target architecture of the Glide libraries, shared by the root project
# and the projects of the single libraries (e.g. glide2x/cvg) when they are
# built on their own. Included once, the subdirectories inherit the settings.
#
# BUILD_32BIT selects the 32-bit (x86) configuration of the Glide sources.
# It follows the pointer size of the compiler. It can additionally be turned
# on for 64-bit GCC on Linux, which then builds with -m32.
#
# Projects with assembly sources enable ASM_NASM themselves, the rule to
# assemble them is set here.
include_guard(GLOBAL)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(GLIDE_COMPILER_IS_32BIT ON)
else()
    set(GLIDE_COMPILER_IS_32BIT OFF)
endif()

option(BUILD_32BIT "Build for 32-bit targets, otherwise x64" ${GLIDE_COMPILER_IS_32BIT})

if(GLIDE_COMPILER_IS_32BIT AND NOT BUILD_32BIT)
    message(FATAL_ERROR
        "The compiler targets 32-bit x86 but BUILD_32BIT is OFF, this would build a "
        "32-bit binary with the 64-bit configuration. Configure with -DBUILD_32BIT=ON.")
endif()

if(BUILD_32BIT AND NOT GLIDE_COMPILER_IS_32BIT AND WIN32)
    message(FATAL_ERROR
        "BUILD_32BIT=ON requires a 32-bit compiler: use -A Win32 for Visual Studio "
        "or the toolchain file mingw-w64-x86_32.cmake for MinGW.")
endif()

# Set globally, so that the flags also reach the fxgasm header generators.
# MSVC builds 32-bit with a different generator platform (-A Win32).
if(BUILD_32BIT AND NOT MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -m32")
endif()

# GCC 15 compiles C23 by default, in which f() declares a function without
# parameters and bool is a keyword. The Glide sources predate that.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
    add_compile_options($<$<COMPILE_LANGUAGE:C>:-std=gnu17>)
endif()

# The *.asm files contain os-specific sections, which are switched with
# these defines: NASM does not define _WIN32 or unix.
if(BUILD_32BIT)
    if(WIN32)
        set(CMAKE_ASM_NASM_OBJECT_FORMAT win32)
    else()
        set(CMAKE_ASM_NASM_OBJECT_FORMAT elf32)
    endif()
else()
    if(WIN32)
        set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
    else()
        set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
    endif()
endif()

if(WIN32)
    set(CMAKE_ASM_NASM_DEFINITIONS ${CMAKE_ASM_NASM_DEFINITIONS} -D_NASM_BUILD_FOR_WIN32)
else()
    set(CMAKE_ASM_NASM_DEFINITIONS ${CMAKE_ASM_NASM_DEFINITIONS} -D_NASM_BUILD_FOR_LINUX)
endif()

set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> \
    <FLAGS> -DGLIDE_PACKED_RGB ${CMAKE_ASM_NASM_DEFINITIONS} -f ${CMAKE_ASM_NASM_OBJECT_FORMAT} -o <OBJECT> <SOURCE>")
