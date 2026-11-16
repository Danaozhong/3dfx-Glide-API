include(GlideDll)
include(GlideFxGasm)
include(GlideWin9x)

set (GLIDE_H5_DIR ${CMAKE_SOURCE_DIR}/glide3x/h5)

set (CINIT_DIR ${GLIDE_H5_DIR}/cinit)

set (MINIHWC_DIR ${GLIDE_H5_DIR}/minihwc)

set (GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

set(GLIDE_SOURCES 
    ${GLIDE_H5_DIR}/glide3/src/fifo.c
    ${GLIDE_H5_DIR}/glide3/src/distate.c
    ${GLIDE_H5_DIR}/glide3/src/gstrip.c
    ${GLIDE_H5_DIR}/glide3/src/distrip.c
    ${GLIDE_H5_DIR}/glide3/src/diget.c
    ${GLIDE_H5_DIR}/glide3/src/gsplash.c 
    ${GLIDE_H5_DIR}/glide3/src/g3df.c  
    ${GLIDE_H5_DIR}/glide3/src/gu.c 
    ${GLIDE_H5_DIR}/glide3/src/gpci.c 
    ${GLIDE_H5_DIR}/glide3/src/diglide.c 
    ${GLIDE_H5_DIR}/glide3/src/disst.c 
    ${GLIDE_H5_DIR}/glide3/src/ditex.c 

    ${GLIDE_H5_DIR}/glide3/src/gbanner.c 
    ${GLIDE_H5_DIR}/glide3/src/gerror.c 
    ${GLIDE_H5_DIR}/glide3/src/gaa.c 
    ${GLIDE_H5_DIR}/glide3/src/gdraw.c 
    ${GLIDE_H5_DIR}/glide3/src/gglide.c 
    ${GLIDE_H5_DIR}/glide3/src/glfb.c 
    ${GLIDE_H5_DIR}/glide3/src/gsst.c 
    ${GLIDE_H5_DIR}/glide3/src/gtex.c 
    ${GLIDE_H5_DIR}/glide3/src/gtexdl.c 
    #${GLIDE_H5_DIR}/glide3/src/gutex.c 
    ${GLIDE_H5_DIR}/glide3/src/xtexdl_def.c
    #${GLIDE_H5_DIR}/glide3/src/cpudtect_stubs.c
    ${GLIDE_H5_DIR}/glide3/src/gthread.c
)



set(GLIDE_SOURCES ${GLIDE_SOURCES} 
    # We always use the C routines for drawing triangles, never the asm or 3dnow ones
    ${GLIDE_H5_DIR}/glide3/src/gxdraw.c
)

# platform specific compile files
if(WIN32)
    # Windows build
    set(GLIDE_SOURCES ${GLIDE_SOURCES}
        ${GLIDE_H5_DIR}/glide3/src/gsfc.c
        ${GLIDE_H5_DIR}/glide3/src/winsurf.c
    )
        
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
        ${MINIHWC_DIR}/hwcio.c
        ${MINIHWC_DIR}/gdebug.c
        
        ${FXMISC_DIR}/fxos.c
        ${FXMISC_DIR}/fximg.c
    )
    

    if (${USE_DRI})
        set(GLIDE_OUTSIDE_SOURCES ${GLIDE_OUTSIDE_SOURCES}
            ${FXMISC_DIR}/linhwc.c
            ${FXMISC_DIR}/linutil.c
        )
    else()
        set(GLIDE_OUTSIDE_SOURCES ${GLIDE_OUTSIDE_SOURCES}
            ${MINIHWC_DIR}/minihwc.c
            ${MINIHWC_DIR}/lin_mode.c
            ${MINIHWC_DIR}/gpio.c
            ${CINIT_DIR}/h3cinit.c
        )
    endif()
endif()

# Compile configuration shared by the DLL and the fxgasm header generator,
# both have to see the same structure layouts.
add_library(glide_config INTERFACE)

target_include_directories(glide_config INTERFACE
    ${GLIDE_H5_DIR}/glide3/src
    ${GLIDE_H5_DIR}/incsrc
    ${GLIDE_H5_DIR}/init
    ${GLIDE_H5_DIR}/minihwc
    ${CINIT_DIR}
    ${CMAKE_SOURCE_DIR}/swlibs/newpci/pcilib
    ${GENERATED_DIR}
)

target_link_libraries(glide_config INTERFACE
    glide_common
    fxmisc
    fxmemmap
)

target_compile_definitions(glide_config INTERFACE
    # general
    -DGLIDE3 
    -DGLIDE3_ALPHA 
    -DGLIDE_HW_TRI_SETUP=1 
    -DGLIDE_INIT_HWC 
    -DGLIDE_PACKED_RGB=0 
    -DGLIDE_PACKET3_TRI_SETUP=1 
    -DGLIDE_TRI_CULLING=1 
    -DUSE_PACKET_FIFO=1
    -DGLIDE_CHECK_CONTEXT

    # subsystem
    -DH3
    -DH4
    -DFX_GLIDE_H5_CSIM=1
    -DFX_GLIDE_NAPALM=1
    
    # other 
    -DGLIDE_PLUG
    -DGLIDE_SPLASH

    # Use MMX
    #-DGL_MMX
    #-DGL_SSE
    #-DGL_SSE2
    #-DGL_X86

    # Do not use the ASM routines, but the C triangle setup routines instead.
    # GL_X86 would additionally require the CPU detection and FPU precision
    # routines from cpudtect.asm.
    -DGLIDE_USE_C_TRISETUP
)


if (NOT ${BUILD_32BIT})
    target_compile_definitions(glide_config INTERFACE
        -DGLIDE_BUILD_64BIT
    )
endif()


if(WIN32)
    # Windows-only defines
    target_compile_definitions(glide_config INTERFACE
        -D__WIN32__
        -DFX_DLL_ENABLE
        -DHWC_ACCESS_DDRAW=1
        -DHWC_EXT_INIT=1
        -DGLIDE_ALT_TAB=1

        -DBETA=1 
        -DHWC_MINIVDD_HACK=1 
        -DWIN40COMPAT=1 
        -DWINXP_ALT_TAB_FIX=1 
        -DWINXP_SAFER_ALT_TAB_FIX=1 
        -DNEED_MSGFILE_ASSIGN
    )
    if(NOT MSVC)
        # MinGW predefines WINNT, which the Glide sources use for Windows NT
        # specific code.
        target_compile_options(glide_config INTERFACE $<$<COMPILE_LANGUAGE:C>:-UWINNT>)
    endif()
else()
    # Linux defines
    target_compile_definitions(glide_config INTERFACE
        -D__linux__
    )

endif()


# Additional debug logs when building debug variant
if(CMAKE_BUILD_TYPE MATCHES DEBUG)
    target_compile_definitions(glide_config INTERFACE
        -DGDBG_INFO_ON 
        -DGLIDE_DEBUG
        -DGLIDE_SANITY_ASSERT
        -DGLIDE_SANITY_SIZE
    )
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
    SOURCE ${GLIDE_H5_DIR}/glide3/src/fxgasm_tool/src/fxgasm.c
    CONFIG glide_config
    OUTPUT_DIR ${GENERATED_DIR}
    GLIDE_SOURCE_DIR ${GLIDE_H5_DIR}/glide3/src
)

add_library(glide_objects OBJECT
    ${GLIDE_SOURCES}
    ${GLIDE_OUTSIDE_SOURCES}
    ${GLIDE_ASM_SOURCES}
    )
target_link_libraries(glide_objects PRIVATE
    glide_config
    pcilib
)
add_dependencies(glide_objects fxgasm_headers)

# Create a shared library 
glide_add_dll(${CMAKE_PROJECT_NAME} OBJECTS glide_objects OUTPUT_NAME glide3x)

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
    glide_config
    pcilib
)
glide_check_win9x_compatibility(${CMAKE_PROJECT_NAME})

if (${EMBED_TEXUS2})
    target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
        texus2
    )
endif()

#if(MSVC)
#  # disable all warnings for now (too many)
#  target_compile_options(glide_objects PRIVATE /W0)
#else()
#  target_compile_options(glide_objects PRIVATE -Wall -Wextra -Wpedantic)
#endif()

if(WIN32)
    # Windows build uses DirectDraw to detect possible screen resolutions and prepare
    # the context, therefore we need to link the DirectDraw libs.
    target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
        dxguid.lib
        ddraw.lib
    )
endif()
