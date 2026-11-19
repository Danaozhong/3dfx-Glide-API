CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

PROJECT(fxbldno VERSION 1.0 DESCRIPTION "Glide3x FX Buildnumber Tool for Voodoo3/4/5" LANGUAGES C)


# Create an executable with the main
add_executable(${CMAKE_PROJECT_NAME} src/fxbldno.c)


target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}/../
    ${CMAKE_SOURCE_DIR}/../
    ${CMAKE_SOURCE_DIR}/../../../incsrc
    ${CMAKE_SOURCE_DIR}/../../../init
    ${CMAKE_SOURCE_DIR}/../../../minihwc
    ${CMAKE_SOURCE_DIR}/../../../../../swlibs/fxmisc
    ${CMAKE_SOURCE_DIR}/../../../../../swlibs/newpci/pcilib
    ${CMAKE_SOURCE_DIR}/../../../../../swlibs/fxmemmap
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
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
    -DFX_GLIDE_NAPALM=1

    # other 
    -DGLIDE_PLUG
    -DGLIDE_SPLASH

    # Use MMX
    -DGL_MMX
    -DGL_SSE
    -DGL_SSE2
    -DGL_X86

    # Do not use the ASM routines, but the C triangle setup routines instead
    -DGLIDE_USE_C_TRISETUP
)

if(WIN32)
    # Windows-only defines
    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
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
else()
    # Linux defines
    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
        -D__linux__
    )
endif()

# Additional debug logs when building debug variant
# $<CONFIG:Debug> is case insensitive and also works with multi-config generators.
target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC
    $<$<CONFIG:Debug>:GDBG_INFO_ON>
    $<$<CONFIG:Debug>:GLIDE_DEBUG>
    $<$<CONFIG:Debug>:GLIDE_SANITY_ASSERT>
    $<$<CONFIG:Debug>:GLIDE_SANITY_SIZE>
)
