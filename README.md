# 3dfx Glide API

This is the source code to 3Dfx Glide for Voodoo graphics accelerators. It's forked from the original CVS repo of Glide open source project at sourceforge:  https://sourceforge.net/projects/glide/

The source is licensed under 3DFX GLIDE Source Code General Public License.

## CI Status
![win-glide3-vodoo345-msvc-build](https://github.com/Danaozhong/3dfx-Glide-API/actions/workflows/build_glide3_voodoo45_msvc.yml/badge.svg)
![win-glide3-vodoo345-gcc-build](https://github.com/Danaozhong/3dfx-Glide-API/actions/workflows/build_glide3_voodoo45_gcc.yml/badge.svg)
![win-glide3-vodoo2-gcc-build](https://github.com/Danaozhong/3dfx-Glide-API/actions/workflows/build_glide3_voodoo2.yml/badge.svg)
![win-glide2-vodoo2-gcc-build](https://github.com/Danaozhong/3dfx-Glide-API/actions/workflows/build_glide2_voodoo3.yml/badge.svg)
![win-glide2-vodoo2-gcc-build](https://github.com/Danaozhong/3dfx-Glide-API/actions/workflows/build_glide2_voodoo2.yml/badge.svg)
![linux-glide2-vodoo2-gcc-build](https://github.com/Danaozhong/3dfx-Glide-API/actions/workflows/build_glide2_linux.yml/badge.svg)

## Motivation

Although 3dfx disappeared from the marked in 2000, there is still a large community of enthusiasts that enjoy using 3dfx video cards up to this day.
A part of these enthusiasts focus on providing software for modern-day operating systems, and this is where this project comes in.

The Glide API is, similar to OpenGL/Direct3D, a 3D API. It is used by many old-school video games, and in order to run these games on present-day OS, the Glide API libraries need to be available.

This project is a port of the Open-Source Linux Glide libraries, and supports cross-platform builds for Linux and Windows, both x86 and x64.

The toolchain is ported to CMake to simplify cross-platform builds, and get rid of a large set of hard-to-use makefiles.

## How to Build?
The libraries are tested under Linux, using the mingw-w64 cross-platform compiler. Visual Studio builds might also work, but are currently not tested in the Github Action CI environment.

The Glide API comes in two versions, Glide2x and Glide3x. Both versions have different libraries for the video card series they are supporting:
* sst1 is for Voodoo1 cards
* cvg is for Voodoo2 cards
* h3 is for Voodoo3/Velocity cards
* h5 (Glide3x only) is for Voodoo4/5 cards

The simplest way to build the libraries is to use the provided docker container, and refer at the build steps of the Github actions.


The struct offset headers `fxgasm.h` and `fxinline.h` are generated automatically during the build, with the same compiler and configuration as the DLL. Do not copy them into the source directories, the build stops if it finds old copies there.

### Build Example (Windows 98, MinGW cross compile)

Windows 9x requires a 32-bit MinGW build. The `mingw-w64-x86_32.cmake` toolchain files set up everything needed for it:
* code is generated for Pentium CPUs (override with `-DGLIDE_TARGET_CPU=...`),
* the DLL is marked as compatible with Windows 4.0,
* libgcc is linked statically.

After linking, the build checks that the DLL does not require a newer Windows version and does not import DLLs that are missing on Windows 9x (e.g. `libgcc_s`, `libwinpthread` or the Universal CRT). This check can be disabled with `-DGLIDE_CHECK_WIN9X=OFF`.

Glide3 for Voodoo3/4/5, from the repository root:
```sh
mkdir bin_win_x32
cd bin_win_x32
cmake -DCMAKE_TOOLCHAIN_FILE=../mingw-w64-x86_32.cmake ..
cmake --build .
```
The other libraries are built the same way from their directories (e.g. `glide2x/cvg`), using the toolchain file in that directory.

`BUILD_32BIT` is detected from the compiler. It only needs to be set for native 32-bit Linux builds (`-DBUILD_32BIT=ON`, which adds `-m32`).

### Build Example (Windows, Visual Studio)

Let's build the Glide3 DLL for Voodoo3/4/5 using Visual Studio. Note that the DLLs only work on Windows 9x when built with a compiler that still supports it (Visual Studio 2005 or older), current Visual Studio versions can not target Windows 9x.

#### Run FX Build Number Header Generator

Go to the Glide3 source directory, and create a binary directory.
```ps1
cd glide3x\h5\glide3\src\fxbldno_tool
mkdir bin_win32
cd bin_win32
```

Generate project, build, generate header files... 
```ps1
cmake -A Win32 ..
cmake --build .
cd ../..
./fxbldno_tool/bin_win32/Debug/fxbldno > fxbldno.h
```

#### Generate the Glide3 DLL
Move back to the repository root, and generate the build directory.
```ps1
cd ..\..\..\..
mkdir bin_win32
cd bin_win32
```
Similar to the steps before, generate your Visual Studio solution, and build the DLL.
```ps1
cmake -A Win32 ..
cmake --build .
```
The glide3x.dll should be generated in the bin_win32\glide3x\h5\Debug directory, ready for use.

## Where to Download?
The libraries are automatically build by the CI for every commit, so you can also use the CI builds (see the "Actions" tab https://github.com/Danaozhong/glide/actions for the download link).

