# Sample toolchain file for building for Windows from an Ubuntu Linux system.
#
# Typical usage:
#    *) install cross compiler: `sudo apt-get install mingw-w64`
#    *) cd build
#    *) cmake -DCMAKE_TOOLCHAIN_FILE=~/mingw-w64-x86_32.cmake ..

set(CMAKE_SYSTEM_NAME Windows)
set(TOOLCHAIN_PREFIX i686-w64-mingw32)

# cross compilers to use for C, C++ and Fortran
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_Fortran_COMPILER ${TOOLCHAIN_PREFIX}-gfortran)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

# Windows 9x compatibility:
#  - Code is generated for GLIDE_TARGET_CPU. The default (Pentium) uses no
#    CMOV, MMX or SSE, so the binaries also run on Pentium and K6 systems.
#  - Windows 9x refuses PE images that require a Windows version above 4.x.
#  - There is no libgcc DLL on Windows 9x, so libgcc is linked statically.
set(GLIDE_TARGET_CPU "pentium" CACHE STRING "CPU to generate code for (-march)")
set(CMAKE_C_FLAGS_INIT "-march=${GLIDE_TARGET_CPU}")

set(GLIDE_WIN9X_LINKER_FLAGS
    "-static-libgcc -Wl,--major-os-version,4 -Wl,--minor-os-version,0 -Wl,--major-subsystem-version,4 -Wl,--minor-subsystem-version,0")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${GLIDE_WIN9X_LINKER_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${GLIDE_WIN9X_LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${GLIDE_WIN9X_LINKER_FLAGS}")

# target environment on the build host system
set(CMAKE_FIND_ROOT_PATH /usr/${TOOLCHAIN_PREFIX})

# modify default behavior of FIND_XXX() commands
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
