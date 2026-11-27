CMAKE_MINIMUM_REQUIRED(VERSION 3.13)

PROJECT(glide2x_voodoo3 VERSION 1.0 DESCRIPTION "Glide2x API for Voodoo Banshee/Voodoo3" LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake)
include(GlideArch)
include(GlideDll)
include(GlideFxGasm)
include(GlideWin9x)

# win9x.asm is the only assembly source.
if(WIN32 AND BUILD_32BIT)
    enable_language(ASM_NASM)
endif()

OPTION(USE_3DNOW "Support 3DNow" OFF) # this probably anyway doesn't work
OPTION(USE_FIFO "Enable FIFO support" ON)

    

set (NEWPCI_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../swlibs/newpci/pcilib)
set (FXMISC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../swlibs/fxmisc)
set (MINIHWC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/minihwc)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/xtexdl_def.c
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/cpudtect_stubs.c
)



set(GLIDE_SOURCES ${GLIDE_SOURCES} 
    # We always use the C routines for drawing triangles, never the asm or 3dnow ones
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src/gxdraw.c
)

# platform specific compile files
if(WIN32)
    # Windows build
    set(GLIDE_OUTSIDE_SOURCES ${GLIDE_OUTSIDE_SOURCES}
        ${MINIHWC_DIR}/hwcio.c
        ${MINIHWC_DIR}/gdebug.c
        ${MINIHWC_DIR}/minihwc.c
        ${MINIHWC_DIR}/win_mode.c
    )
    if (${BUILD_32BIT})
        # This code is anyway only relevant for Win9x, and can be excluded for x64 builds.
        set (GLIDE_ASM_SOURCES ${GLIDE_ASM_SOURCES}
            ${MINIHWC_DIR}/win9x.asm
        )
    endif()
else()
    # Linux build
    set(GLIDE_OUTSIDE_SOURCES ${GLIDE_OUTSIDE_SOURCES}
        ${NEWPCI_LIB_DIR}/fxpci.c
        ${NEWPCI_LIB_DIR}/fxlinux.c
        
        ${MINIHWC_DIR}/hwcio.c
        ${MINIHWC_DIR}/gdebug.c
        
        ${FXMISC_DIR}/fxos.c
        ${FXMISC_DIR}/fximg.c
    )
endif()

# Compile configuration shared by the DLL and the fxgasm header generator,
# both have to see the same structure layouts.
add_library(${PROJECT_NAME}_config INTERFACE)

target_include_directories(${PROJECT_NAME}_config INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/glide/src
    ${CMAKE_CURRENT_SOURCE_DIR}/incsrc
    ${CMAKE_CURRENT_SOURCE_DIR}/init
    ${CMAKE_CURRENT_SOURCE_DIR}/minihwc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../swlibs/fxmisc
    ${CMAKE_CURRENT_SOURCE_DIR}/../../swlibs/newpci/pcilib
    ${CMAKE_CURRENT_SOURCE_DIR}/../../swlibs/fxmemmap
    ${GENERATED_DIR}
)

target_compile_definitions(${PROJECT_NAME}_config INTERFACE
    # general
    -D__3Dfx_PCI_CFG__ 
    -DGLIDE_HW_TRI_SETUP=1
    -DGLIDE_INIT_HWC    
    -DGLIDE_PACKED_RGB=0
    -DGLIDE_TRI_CULLING=1 
    -DGLIDE_DEFAULT_GAMMA=1.3f
    -DGLIDE_LIB=1
    -DHAL_HW=1
    
    # subsystem
    -DH3
    
    # other 
    -DGLIDE_PLUG 
    -DGLIDE_SPLASH 
    
    # forgot where this came from
    #-DGLIDE_DISPATCH_SETUP=1 // for asm only
    -DGLIDE_DISPATCH_DOWNLOAD=1 
    -DGLIDE_CHIP_BROADCAST=1 
    -DGLIDE_BLIT_CLEAR=1 

    -DGL_X86
    
    # Do not use the ASM routines, but the C triangle setup routines instead
    -DGLIDE_USE_C_TRISETUP=1
)
# Use FIFO?
if (${USE_FIFO})
    target_compile_definitions(${PROJECT_NAME}_config INTERFACE
        -DUSE_PACKET_FIFO=1
        -DGLIDE_PACKET3_TRI_SETUP=1
    )
endif()


if (NOT ${BUILD_32BIT})
    target_compile_definitions(${PROJECT_NAME}_config INTERFACE
        -DGLIDE_BUILD_64BIT
    )
endif()


if(WIN32)
    # Windows-only defines. WINNT must not be defined, it selects the
    # Windows NT kernel mode driver interface instead of the Win9x one.
    target_compile_definitions(${PROJECT_NAME}_config INTERFACE
        -DHWC_EXT_INIT=1
        -DFX_DLL_ENABLE 
        -DHWC_ACCESS_DDRAW=1
    )
    if(NOT MSVC)
        # MinGW predefines WINNT
        target_compile_options(${PROJECT_NAME}_config INTERFACE $<$<COMPILE_LANGUAGE:C>:-UWINNT>)
    endif()
else()
    # Linux defines
    target_compile_definitions(${PROJECT_NAME}_config INTERFACE
        -D__linux__
    )
endif()


# Additional debug logs when building debug variant
# $<CONFIG:Debug> is case insensitive and also works with multi-config generators.
target_compile_definitions(${PROJECT_NAME}_config INTERFACE
    $<$<CONFIG:Debug>:GDBG_INFO_ON>
    $<$<CONFIG:Debug>:GLIDE_DEBUG>
    $<$<CONFIG:Debug>:GLIDE_SANITY_ASSERT>
    $<$<CONFIG:Debug>:GLIDE_SANITY_SIZE>
)

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
    ${GLIDE_ASM_SOURCES})
target_link_libraries(${PROJECT_NAME}_objects PRIVATE ${PROJECT_NAME}_config)
add_dependencies(${PROJECT_NAME}_objects ${PROJECT_NAME}_fxgasm)

# Create a shared library 
glide_add_dll(${PROJECT_NAME} OBJECTS ${PROJECT_NAME}_objects OUTPUT_NAME glide2x)

target_link_libraries(${PROJECT_NAME} PUBLIC ${PROJECT_NAME}_config)
glide_check_win9x_compatibility(${PROJECT_NAME})

#if(MSVC)
#  # disable all warnings for now (too many)
#  target_compile_options(${PROJECT_NAME}_objects PRIVATE /W0)
#else()
#  target_compile_options(${PROJECT_NAME}_objects PRIVATE -Wall -Wextra -Wpedantic)
#endif()

if(WIN32)
    # Windows build uses DirectDraw to detect possible screen resolutions and prepare
    # the context, therefore we need to link the DirectDraw libs.
    target_link_libraries(${PROJECT_NAME} PUBLIC
        dxguid.lib
        ddraw.lib
    )
endif()

