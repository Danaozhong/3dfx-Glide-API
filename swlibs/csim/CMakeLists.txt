# Stub of the 3dfx chip simulator, used by the HAL_CSIM build of Glide
# (see BUILD_HAL_CSIM in the top level CMakeLists.txt).
#
# It contains the interface headers and do-nothing implementations. To build
# against a real simulator, replace the contents of this directory with its
# csim.h and sources.

add_library(csim STATIC
    src/csim_stub.c
)

# Consumers need csim.h.
target_include_directories(csim PUBLIC include)

# The stub implements the interface declared in fxhal.h of Glide3x, which
# has to be compiled with the same configuration as Glide.
target_include_directories(csim PRIVATE
    ${CMAKE_SOURCE_DIR}/glide3x/h5/incsrc
)
target_link_libraries(csim PRIVATE glide_common fxmisc pcilib)
target_compile_definitions(csim PRIVATE
    HAL_CSIM=1
    GLIDE3
    H3
    H4
    FX_GLIDE_NAPALM=1
)
set_target_properties(csim PROPERTIES POSITION_INDEPENDENT_CODE ON)
