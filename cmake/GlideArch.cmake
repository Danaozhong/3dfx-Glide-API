# BUILD_32BIT selects the 32-bit (x86) configuration of the Glide sources.
# It follows the pointer size of the compiler. It can additionally be turned
# on for 64-bit GCC on Linux, which then builds with -m32.

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
