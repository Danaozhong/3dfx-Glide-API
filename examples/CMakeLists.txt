# Example programs of Glide3x for Voodoo4/5.
#
#   csim_teapot/  The Utah teapot, Gouraud shaded and depth buffered. Renders
#                 on the simulated board (swlibs/csim) and saves the frames
#                 as images, only built with BUILD_HAL_CSIM.
#   3dfx/         The test programs of the 3dfx Glide3 SDK, built with
#                 GLIDE_BUILD_3DFX_EXAMPLES.

if(BUILD_HAL_CSIM)
    add_subdirectory(csim_teapot)
endif()

if(GLIDE_BUILD_3DFX_EXAMPLES)
    add_subdirectory(3dfx)
endif()
