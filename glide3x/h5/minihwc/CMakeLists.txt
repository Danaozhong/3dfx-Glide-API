if(WIN32)
    set(MINIHWC_OS_SPECIFIC_SOURCES
        win_mode.c
    )
else ()
    set(MINIHWC_OS_SPECIFIC_SOURCES
        lin_mode.c
        linhwc.c
        gpio.c
    )
endif()

if (${BUILD_32BIT})
    # This code is anyway only relevant for Win9x, and can be excluded for x64 builds.
    set (MINHWC_ASM_SOURCES ${MINHWC_ASM_SOURCES}
        win9x.asm
    )
endif()

add_library(minihwc
    ${MINHWC_ASM_SOURCES}
    ${MINIHWC_OS_SPECIFIC_SOURCES}
    hwcio.c
    gdebug.c
    minihwc.c
)

if (${BUILD_32BIT})
    # 32 bit compilation
    target_compile_definitions(minihwc PUBLIC
        -DGL_X86
    )
endif()

if(WIN32)
    # Windows-only defines
    target_compile_definitions(minihwc PUBLIC
        -D__WIN32__
        -DHWC_ACCESS_DDRAW=1
        -DHWC_EXT_INIT=1
        -DHWC_MINIVDD_HACK=1
    )
endif()


target_compile_definitions(minihwc PUBLIC
    # subsystem
    -DH3
    -DH4
    -DFX_GLIDE_H5_CSIM=1
    -DFX_GLIDE_NAPALM=1
    
)


target_include_directories(minihwc PUBLIC .)

target_link_libraries(minihwc PUBLIC fxmisc h3 glide_common)
