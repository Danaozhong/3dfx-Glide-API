CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

PROJECT(glide2x_voodoo2 VERSION 1.0 DESCRIPTION "Glide2x API for Voodoo2" LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
include(GlideArch)
include(GlideDll)
include(GlideFxGasm)
include(GlideWin9x)

# xdraw2.asm is the only assembly source.
if(BUILD_32BIT)
    enable_language(ASM_NASM)
endif()

OPTION(USE_3DNOW "Support 3DNow" OFF) # this probably anyway doesn't work


    

set (NEWPCI_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../swlibs/newpci/pcilib)
set (GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

set(GLIDE_SOURCES 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/fifo.c
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gsplash.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/g3df.c  
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gu.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/guclip.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gpci.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gump.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/diglide.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/disst.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/ditex.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gbanner.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gerror.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gmovie.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/digutex.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/ddgump.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gaa.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gdraw.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gglide.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/glfb.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gsst.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gtex.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gtexdl.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gutex.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/cpuid.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/fpu.c 
    #${CMAKE_CURRENT_SOURCE_DIR}/glide/src/xtexdl.c
)

if (${BUILD_32BIT})
    set (GLIDE_ASM_SOURCE
       ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/xdraw2.asm
    )
else()
    set(GLIDE_SOURCES ${GLIDE_SOURCES} 
        # 64bit doesn't support compiling assembly - use the C code instead
        ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/xdraw2_x64.c
    )
endif()

if (${USE_3DNOW})
    set(GLIDE_SOURCES ${GLIDE_SOURCES} 
        ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/xdraw2_3dnow.c
        ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/xdraw3_3dnow.c
        ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/xtexdl_3dnow.c
    )
else()
    set(GLIDE_SOURCES ${GLIDE_SOURCES} 
        ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gxdraw.c
        ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/xtexdl.c
    )
endif()


set(GLIDE_OUTSIDE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/digutex.c
    ${NEWPCI_LIB_DIR}/fxpci.c
    ${NEWPCI_LIB_DIR}/fxmsr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/canopus.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/dac.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/gamma.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/gdebug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/info.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/print.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/sli.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/sst1init.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/video.c
    ${CMAKE_CURRENT_SOURCE_DIR}/init/fxremap.c
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
add_library(${PROJECT_NAME}_config INTERFACE)

target_include_directories(${PROJECT_NAME}_config INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src
    ${CMAKE_CURRENT_SOURCE_DIR}/incsrc
    ${CMAKE_CURRENT_SOURCE_DIR}/init
    ${CMAKE_CURRENT_SOURCE_DIR}/../../swlibs/fxmisc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../swlibs/newpci/pcilib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../swlibs/fxmemmap
    ${GENERATED_DIR}
)

target_compile_definitions(${PROJECT_NAME}_config INTERFACE
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
# $<CONFIG:Debug> is case insensitive and also works with multi-config generators.
target_compile_definitions(${PROJECT_NAME}_config INTERFACE
    $<$<CONFIG:Debug>:GDBG_INFO_ON>
    $<$<CONFIG:Debug>:GLIDE_DEBUG>
    $<$<CONFIG:Debug>:GLIDE_SANITY_ASSERT>
    $<$<CONFIG:Debug>:GLIDE_SANITY_SIZE>
)

if(WIN32 AND NOT MSVC)
    # MinGW predefines WINNT, which the Glide sources use for Windows NT
    # specific code.
    target_compile_options(${PROJECT_NAME}_config INTERFACE $<$<COMPILE_LANGUAGE:C>:-UWINNT>)
endif()

# OS-specific-specific defines
if(WIN32)
    
    # All attributes starting with "_NASM" are needed to be able to build the asm sources correctly.
    # The *.asm files contain os-specific sections, that are switched using the defines below.
    # NASM does not provide any _WIN32 or unix defines, so we have to add them ourselves.
    target_compile_definitions(${PROJECT_NAME}_config INTERFACE
        _NASM_BUILD_FOR_WIN32
    )
    
else()
    target_compile_definitions(${PROJECT_NAME}_config INTERFACE
        _NASM_BUILD_FOR_LINUX
    )
endif()

glide_add_fxgasm_headers(${PROJECT_NAME}_fxgasm
    SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/fxgasm_tool/src/fxgasm.c
    CONFIG ${PROJECT_NAME}_config
    OUTPUT_DIR ${GENERATED_DIR}
    GLIDE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glide/src
)

add_library(${PROJECT_NAME}_objects OBJECT
    ${GLIDE_SOURCES}
    
    ${GLIDE_OUTSIDE_SOURCES}
    ${GLIDE_ASM_SOURCE})
target_link_libraries(${PROJECT_NAME}_objects PRIVATE ${PROJECT_NAME}_config)
add_dependencies(${PROJECT_NAME}_objects ${PROJECT_NAME}_fxgasm)

# Create a shared library 
glide_add_dll(${PROJECT_NAME} OBJECTS ${PROJECT_NAME}_objects OUTPUT_NAME glide2x)

target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_NAME}_config)
glide_check_win9x_compatibility(${PROJECT_NAME})

if(MSVC)
  # disable all warnings for now (too many)
  target_compile_options(${PROJECT_NAME}_objects PRIVATE /W0)
else()
  target_compile_options(${PROJECT_NAME}_objects PRIVATE -Wall -Wextra -Wpedantic)
endif()
