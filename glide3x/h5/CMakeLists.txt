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



# The 32-bit Windows DLL uses the x86 assembler routines for the triangle setup,
# vertex lists and texture downloads, like the original 3dfx DLL. The variants
# for 3DNow!, MMX, SSE and SSE2 are picked at runtime from the CPU features.
# Everywhere else the C routines are used.
if(WIN32 AND BUILD_32BIT AND NOT MSVC AND NOT BUILD_HAL_CSIM)
    set(GLIDE_X86_ASM_DEFAULT ON)
else()
    set(GLIDE_X86_ASM_DEFAULT OFF)
endif()
option(GLIDE_X86_ASM "Use the x86 assembler routines of Glide (32-bit MinGW only)" ${GLIDE_X86_ASM_DEFAULT})

if(GLIDE_X86_ASM)
    set(GLIDE_SOURCES ${GLIDE_SOURCES}
        ${GLIDE_H5_DIR}/glide3/src/cpuid.c
    )
else()
    set(GLIDE_SOURCES ${GLIDE_SOURCES}
        ${GLIDE_H5_DIR}/glide3/src/gxdraw.c
    )
endif()

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
            ${MINIHWC_DIR}/linhwc.c
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
    -DGLIDE_PACKED_RGB=0 
    -DGLIDE_PACKET3_TRI_SETUP=1 
    -DGLIDE_TRI_CULLING=1 
    -DUSE_PACKET_FIFO=1

    # subsystem
    -DH3
    -DH4
    -DFX_GLIDE_NAPALM=1
    
    # other 
    -DGLIDE_PLUG
    -DGLIDE_SPLASH
)

if(GLIDE_X86_ASM)
    # Each assembler source is built once per variant, selected by only one
    # of these defines (see glide_x86_asm_variant below).
    set(not_asm $<NOT:$<COMPILE_LANGUAGE:ASM_NASM>>)
    target_compile_definitions(glide_config INTERFACE
        $<${not_asm}:GL_X86>
        $<${not_asm}:GL_AMD3D>
        $<${not_asm}:GL_MMX>
        $<${not_asm}:GL_SSE>
        $<${not_asm}:GL_SSE2>
    )
else()
    target_compile_definitions(glide_config INTERFACE
        -DGLIDE_USE_C_TRISETUP
    )
endif()


if (NOT ${BUILD_32BIT})
    target_compile_definitions(glide_config INTERFACE
        -DGLIDE_BUILD_64BIT
    )
endif()

if(BUILD_HAL_CSIM)
    # The hardware is accessed through the simulator hooks of the HAL
    # (fxHal*, halLoad*/halStore*), see swlibs/csim.
    target_compile_definitions(glide_config INTERFACE
        -DHAL_CSIM=1
        -DGLIDE_INIT_HAL=1
    )
    # Only the headers: the fake board is linked into the library below, so
    # that programs linking the library share its single instance.
    target_include_directories(glide_config INTERFACE
        $<TARGET_PROPERTY:csim,INTERFACE_INCLUDE_DIRECTORIES>
    )
else()
    # The hardware is accessed through minihwc.
    target_compile_definitions(glide_config INTERFACE
        -DGLIDE_INIT_HWC
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
    if (${USE_DRI})
        # linhwc.c talks to the tdfx kernel module and the X server through
        # the DRI, instead of programming the hardware itself (minihwc).
        target_compile_definitions(glide_config INTERFACE
            -DDRI_BUILD
        )
    endif()
endif()

# Checks whether the display driver took the context away (hwcShareContextData
# of minihwc), which the DRI build does not have.
if (NOT ${USE_DRI})
    target_compile_definitions(glide_config INTERFACE
        -DGLIDE_CHECK_CONTEXT
    )
endif()


# Additional debug logs when building debug variant
# $<CONFIG:Debug> is case insensitive and also works with multi-config generators.
target_compile_definitions(glide_config INTERFACE
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
    target_compile_definitions(glide_config INTERFACE
        _NASM_BUILD_FOR_WIN32
    )
    
else()
    target_compile_definitions(glide_config INTERFACE
        _NASM_BUILD_FOR_LINUX
    )
endif()

if(GLIDE_X86_ASM)
    # glide_x86_asm_variant(<output variable> <source> <variant> [<define>])
    # Assembles glide3/src/<source>.asm with <define>, through a wrapper in
    # the build directory, as one source can only be built once per target.
    function(glide_x86_asm_variant out source variant)
        set(wrapper ${GENERATED_DIR}/${source}_${variant}.asm)
        set(content "")
        if(ARGC GREATER 3)
            set(content "%define ${ARGV3} 1\n")
        endif()
        string(APPEND content "%include \"${source}.asm\"\n")
        set(old_content "")
        if(EXISTS ${wrapper})
            file(READ ${wrapper} old_content)
        endif()
        if(NOT old_content STREQUAL content)
            file(WRITE ${wrapper} "${content}")
        endif()
        set(src ${GLIDE_H5_DIR}/glide3/src)
        set_source_files_properties(${wrapper} PROPERTIES
            # MinGW prefixes C symbols with an underscore.
            COMPILE_OPTIONS "--prefix;_"
            OBJECT_DEPENDS "${src}/${source}.asm;${src}/xos.inc;${src}/xdraw2.inc")
        set(${out} ${wrapper} PARENT_SCOPE)
    endfunction()

    foreach(variant
            "xdraw2;def" "xdraw3;def"
            "xdraw2;3dnow;GL_AMD3D" "xdraw3;3dnow;GL_AMD3D" "xtexdl;3dnow;GL_AMD3D"
            "xtexdl;mmx;GL_MMX"
            "xdraw2;sse;GL_SSE" "xdraw3;sse;GL_SSE"
            "xtexdl;sse2;GL_SSE2")
        glide_x86_asm_variant(wrapper ${variant})
        list(APPEND GLIDE_ASM_SOURCES ${wrapper})
    endforeach()

    # grDrawTriangle, gdraw.c has it in C for debug builds.
    glide_x86_asm_variant(wrapper xdrawtri mingw __MINGW32__)
    list(APPEND GLIDE_ASM_SOURCES $<$<NOT:$<CONFIG:Debug>>:${wrapper}>)
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
if(BUILD_HAL_CSIM)
    # Do not mix it up with the library for real hardware.
    glide_add_dll(${CMAKE_PROJECT_NAME} OBJECTS glide_objects OUTPUT_NAME glide3x_csim)
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME glide3x_csim)
else()
    glide_add_dll(${CMAKE_PROJECT_NAME} OBJECTS glide_objects OUTPUT_NAME glide3x)
endif()

target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC
    glide_config
    pcilib
)
if(BUILD_HAL_CSIM)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE csim)
endif()
if(UNIX)
    # exp() and pow() of the fog and gamma tables.
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE m)
endif()
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
