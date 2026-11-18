

add_subdirectory(fxmemmap)
add_subdirectory(fxmisc)
add_subdirectory(newpci)
add_subdirectory(texus2)

if(BUILD_HAL_CSIM)
    add_subdirectory(csim)
endif()
