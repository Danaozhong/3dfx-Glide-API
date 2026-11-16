CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

PROJECT(Glide2x VERSION 1.0 DESCRIPTION "Template solution by Clemens Zangl" LANGUAGES C ASM_NASM)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../../cmake)
include(GlideArch)
include(GlideFxGasm)
include(GlideWin9x)

OPTION(USE_3DNOW "Support 3DNow" OFF) # this probably anyway doesn't work


# Set the configuration for the assembler
if (${BUILD_32BIT})
    if(MSVC)
        # Nothing to do - MSVC supports building 32bit using a different cmake invokation
    else()
        # Set this flag for the compiler and linker
        SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
        SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -m32")
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
    

set (NEWPCI_LIB_DIR ${CMAKE_SOURCE_DIR}/../../swlibs/newpci/pcilib)
set (GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

set(GLIDE_SOURCES 
    ${CMAKE_SOURCE_DIR}/glide/src/fifo.c
    ${CMAKE_SOURCE_DIR}/glide/src/gsplash.c 
    ${CMAKE_SOURCE_DIR}/glide/src/g3df.c  
    ${CMAKE_SOURCE_DIR}/glide/src/gu.c 
    ${CMAKE_SOURCE_DIR}/glide/src/guclip.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gpci.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gump.c 
    ${CMAKE_SOURCE_DIR}/glide/src/diglide.c 
    ${CMAKE_SOURCE_DIR}/glide/src/disst.c 
    ${CMAKE_SOURCE_DIR}/glide/src/ditex.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gbanner.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gerror.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gmovie.c 
    ${CMAKE_SOURCE_DIR}/glide/src/digutex.c 
    ${CMAKE_SOURCE_DIR}/glide/src/ddgump.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gaa.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gdraw.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gglide.c 
    ${CMAKE_SOURCE_DIR}/glide/src/glfb.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gsst.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gtex.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gtexdl.c 
    ${CMAKE_SOURCE_DIR}/glide/src/gutex.c 
    ${CMAKE_SOURCE_DIR}/glide/src/cpuid.c 
    ${CMAKE_SOURCE_DIR}/glide/src/fpu.c 
    #${CMAKE_SOURCE_DIR}/glide/src/xtexdl.c
)

if (${BUILD_32BIT})
    set (GLIDE_ASM_SOURCE
       ${CMAKE_SOURCE_DIR}/glide/src/xdraw2.asm
    )
else()
    set(GLIDE_SOURCES ${GLIDE_SOURCES} 
        # 64bit doesn't support compiling assembly - use the C code instead
        ${CMAKE_SOURCE_DIR}/glide/src/xdraw2_x64.c
    )
endif()

if (${USE_3DNOW})
    set(GLIDE_SOURCES ${GLIDE_SOURCES} 
        ${CMAKE_SOURCE_DIR}/glide/src/xdraw2_3dnow.c
        ${CMAKE_SOURCE_DIR}/glide/src/xdraw3_3dnow.c
        ${CMAKE_SOURCE_DIR}/glide/src/xtexdl_3dnow.c
    )
else()
    set(GLIDE_SOURCES ${GLIDE_SOURCES} 
        ${CMAKE_SOURCE_DIR}/glide/src/gxdraw.c
        ${CMAKE_SOURCE_DIR}/glide/src/xtexdl.c
    )
endif()


set(GLIDE_OUTSIDE_SOURCES
    ${CMAKE_SOURCE_DIR}/glide/src/digutex.c
    ${NEWPCI_LIB_DIR}/fxpci.c
    ${NEWPCI_LIB_DIR}/fxmsr.c
    ${CMAKE_SOURCE_DIR}/init/canopus.c
    ${CMAKE_SOURCE_DIR}/init/dac.c
    ${CMAKE_SOURCE_DIR}/init/gamma.c
    ${CMAKE_SOURCE_DIR}/init/gdebug.c
    ${CMAKE_SOURCE_DIR}/init/info.c
    ${CMAKE_SOURCE_DIR}/init/parse.c
    ${CMAKE_SOURCE_DIR}/init/print.c
    ${CMAKE_SOURCE_DIR}/init/sli.c
    ${CMAKE_SOURCE_DIR}/init/sst1init.c
    ${CMAKE_SOURCE_DIR}/init/util.c
    ${CMAKE_SOURCE_DIR}/init/video.c
    ${CMAKE_SOURCE_DIR}/init/fxremap.c
)

# platform specific compile files
set(WINDOWS_DEPENDENCIES
    ${NEWPCI_LIB_DIR}/fxnt.c
    ${NEWPCI_LIB_DIR}/fxw32.c
    ${NEWPCI_LIB_DIR}/fxvxd.c
)

set(LINUX_DEPENDENCIES
    ${NEWPCI_LIB_DIR}/fxlinux.c
)

if(WIN32)
    set(GLIDE_OUTSIDE_SOURCES ${GLIDE_OUTSIDE_SOURCES}
       ${WINDOWS_DEPENDENCIES}
    )    
else()
    set(GLIDE_OUTSIDE_SOURCES ${GLIDE_OUTSIDE_SOURCES}
       ${LINUX_DEPENDENCIES} 
    )
endif()

# Compile configuration shared by the DLL and the fxgasm header generator,
# both have to see the same structure layouts.
add_library(glide_config INTERFACE)

target_include_directories(glide_config INTERFACE
    ${CMAKE_SOURCE_DIR}/glide/src
    ${CMAKE_SOURCE_DIR}/incsrc
    ${CMAKE_SOURCE_DIR}/init
    ${CMAKE_SOURCE_DIR}/../../swlibs/fxmisc
    ${CMAKE_SOURCE_DIR}/../../swlibs/newpci/pcilib
    ${CMAKE_SOURCE_DIR}/../../swlibs/fxmemmap
    ${GENERATED_DIR}
)

target_compile_definitions(glide_config INTERFACE
    -DDIRECTX 
    -DFX_DLL_ENABLE 
    -D__3Dfx_PCI_CFG__ 
    -DGLIDE_HW_TRI_SETUP=1 
    -DGLIDE_PACKED_RGB=1 
    -DGLIDE_PACKET3_TRI_SETUP=1 
    -DGLIDE_TRI_CULLING=1 
    -DGLIDE_DEFAULT_GAMMA=1.3f
    -DGLIDE_LIB=1
    
    -DUSE_PACKET_FIFO=1 
    -DGLIDE_DISPATCH_SETUP=1 
    -DGLIDE_DISPATCH_DOWNLOAD=1 
    -DGLIDE_CHIP_BROADCAST=1 
    -DGLIDE_BLIT_CLEAR=1 
    -DCVG 
    -DGLIDE_PLUG 
    -DGLIDE_SPLASH
    -DGL_X86
    -DGLIDE_USE_C_TRISETUP
)
if(CMAKE_BUILD_TYPE MATCHES DEBUG)
    target_compile_definitions(glide_config INTERFACE
        -DGDBG_INFO_ON 
        -DGLIDE_DEBUG
        -DGLIDE_SANITY_ASSERT
        -DGLIDE_SANITY_SIZE
    )
endif()

if(WIN32 AND NOT MSVC)
    # MinGW predefines WINNT, which the Glide sources use for Windows NT
    # specific code.
    target_compile_options(glide_config INTERFACE $<$<COMPILE_LANGUAGE:C>:-UWINNT>)
endif()

# OS-specific-specific defines
if(WIN32)
    
    # All attributes starting with "_NASM" are needed to be able to build the asm sources correctly.
    # The *.asm files contain os-specific sections, that are switched using the defines below.
    # NASM does not provide any _WIN32 or unix defines, so we have to add them ourselves.
    target_compile_definitions(glide_config INTERFACE
        _NASM_BUILD_FOR_WIN32
    )
    
else()
    target_compile_definitions(glide_config INTERFACE
        _NASM_BUILD_FOR_LINUX
    )
endif()

glide_add_fxgasm_headers(fxgasm_headers
    SOURCE ${CMAKE_SOURCE_DIR}/glide/src/fxgasm_tool/src/fxgasm.c
    CONFIG glide_config
    OUTPUT_DIR ${GENERATED_DIR}
    GLIDE_SOURCE_DIR ${CMAKE_SOURCE_DIR}/glide/src
)

# Create a shared library 
add_library(${CMAKE_PROJECT_NAME} SHARED 
    ${GLIDE_SOURCES}
    
    ${GLIDE_OUTSIDE_SOURCES}
    ${GLIDE_ASM_SOURCE})

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC glide_config)
add_dependencies(${CMAKE_PROJECT_NAME} fxgasm_headers)
glide_check_win9x_compatibility(${CMAKE_PROJECT_NAME})

if(MSVC)
  # disable all warnings for now (too many)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W0)
else()
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()
