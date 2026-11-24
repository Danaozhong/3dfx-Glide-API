# Simulator of the 3dfx Napalm chip, used by the HAL_CSIM build of Glide
# (see BUILD_HAL_CSIM in the top level CMakeLists.txt).
#
# It contains the interface headers and a simulated board that Glide can
# initialize and render to, see include/csim.h.
#
# The board is written in C++, its interface to Glide is C.

add_library(csim STATIC
    src/board.cpp
    src/color_combine.cpp
    src/command_fifo.cpp
    src/csim.cpp
    src/fbi_pixel_pipeline.cpp
    src/frame_buffer.cpp
    src/rasterization.cpp
    src/registers.cpp
    src/render_state.cpp
    src/texture_unit.cpp
    src/triangle_setup.cpp
)
target_compile_features(csim PRIVATE cxx_std_17)
set_target_properties(csim PROPERTIES CXX_EXTENSIONS OFF)

# Consumers need csim.h, tests csimstats.h.
target_include_directories(csim PUBLIC include)

# The board implements the interface declared in fxhal.h of Glide3x, which
# has to be compiled with the same configuration as Glide.
target_include_directories(csim PRIVATE
    ${CMAKE_SOURCE_DIR}/glide3x/h5/incsrc
)
target_link_libraries(csim PRIVATE glide_common fxmisc pcilib)
target_compile_definitions(csim PRIVATE
    CSIM_BUILD
    HAL_CSIM=1
    GLIDE3
    H3
    H4
    FX_GLIDE_NAPALM=1
)
set_target_properties(csim PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The Glide DLL is then linked as C++. Link the C++ runtime of MinGW
# statically, so the DLL does not need libstdc++-6.dll and libgcc next to it.
if(MINGW)
    target_link_options(csim INTERFACE -static-libstdc++ -static-libgcc)
endif()
